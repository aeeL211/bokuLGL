#include "hooks.h"

#include <pthread.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "fitur.h"
#include "hook.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Vector2 hanya dua float berurutan, ditulis langsung ke field struct milik game.
struct Vektor2 {
    float x;
    float y;
};

using FnDeteksi = void (*)(void*, void**, int32_t, float, float, void*);
using FnPosCheck = void (*)(void*, void**, void*);
using FnGravitasi = void (*)(void*, void*);

// Batas yang dipakai No Clip. Nilainya jauh di luar ukuran dunia mana pun, jadi
// pemeriksaan tabrakan tidak pernah menyempitkannya lagi.
constexpr float kBatasBebas = 1.0e9f;
constexpr int32_t kBatasTeleport = 1024;

Field fieldAktorPemain = {};
Field fieldPemainLokal = {};
Field fieldAktorFisika = {};
Field fieldPemainStatus = {};
Field fieldDeteksiAktor = {};

// Posisi tile milik game disimpan di Physics.pos, terpisah dari Transform Unity.
// Transform hanya gambar yang ditulis ulang tiap frame, jadi teleport menulis
// posisi fisika beserta seluruh titik jejaknya.
Field fieldFisika = {};
Field fieldPosisi = {};
Field fieldDunia = {};
Field fieldTile = {};
Field fieldJejak = {};
Field fieldJejakPrev = {};
Field fieldJejakCheck = {};
Field fieldJejakBaru = {};
Field fieldJejakAkhir = {};
Field fieldSegar = {};
Field fieldKecepatan = {};
Field fieldKecepatanX = {};
Field fieldKecepatanY = {};
Field fieldKecepatanAdd = {};
Field fieldKecepatanAddX = {};
Field fieldKecepatanAddY = {};

// Skala awal teleport diambil dari ukuran tile game, dengan collider pemain
// sebagai cadangan: size adalah ukuran dalam satuan dunia, sedangkan tile_Size
// adalah jumlah tile yang ditempati.
Field fieldCollider = {};
Field fieldUkuranCollider = {};
Field fieldUkuranTile = {};
Field fieldUkuranTileW = {};
Field fieldUkuranTileH = {};
Field fieldUkuranPixel = {};
// Nama _Tile dipakai dua kelas berbeda, jadi kelas yang punya pixel_Size
// diikat sekali saat hook dipasang. Pemindaian penuh tidak diulang tiap frame.
void* kelasTile = nullptr;

// Tile bisa berupa Vector2 pecahan atau Vector2Int. Tipe field dibaca sekali
// saat hook dipasang supaya penulisan tidak menebak lebar tiap field.
bool tileBulat = false;

// Margin adalah kotak gerak yang dihitung ulang tiap frame dari tile di sekitar
// badan. Sisi kiri, kanan, dan atas dibuka, sedangkan bawah dibiarkan supaya
// lantai tetap dipijak.
Field fieldMargin = {};
Field fieldMarginAtas = {};
Field fieldMarginKiri = {};
Field fieldMarginKanan = {};

// Posisi untuk baris menu. Nilainya hanya salinan angka, jadi aman dibaca dari
// thread UI tanpa menyentuh objek IL2CPP.
pthread_mutex_t kunciPosisi = PTHREAD_MUTEX_INITIALIZER;
struct PosisiRingkas {
    bool ada;
    bool adaTile;
    float tileX;
    float tileY;
    float duniaX;
    float duniaY;
};
PosisiRingkas posisiRingkas = {};

// Baris posisi hanya dibaca menu sekali per detik. Salinan diperbarui selama
// dua detik sesudah pembacaan terakhir, jadi menu yang tertutup tidak lagi
// mengunci mutex tiap frame.
constexpr double kMasaPosisi = 2.0;
std::atomic<double> posisiDiminta{0.0};

double sekarangDetik() {
    timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

// Pos.tile dan Pos.world memakai ruang koordinat berbeda. Selisih keduanya
// memberi satu skala yang berlaku untuk kedua sumbu, jadi teleport tidak perlu
// menebak titik nol Y dari rasio dunia/tile yang bisa meleset.
struct Kalibrasi {
    bool siap;
    bool pasti;
    bool dariGerak;
    float skala;
    int sampel;
    float offsetX;
    float offsetY;
    bool adaAcuan;
    float acuanDuniaX;
    float acuanDuniaY;
    float acuanTileX;
    float acuanTileY;
};
Kalibrasi kalibrasi = {};

// Kecepatan dan gravitasi hanya mengubah fisika pemain lokal. Nilai asli game
// tidak pernah ditimpa permanen: pengali dipasang selama satu panggilan method
// game lalu nilainya dikembalikan seperti semula.
int logKecepatan = -1;
int logGravitasi = -1;

struct VerifikasiTeleport {
    int sisa;
    float tileX;
    float tileY;
    Vektor2 dunia;
};
VerifikasiTeleport verifikasi = {};

bool pemainLokal(void* manajer) {
    void* global = bacaPtr(manajer, fieldAktorPemain);
    return global && bacaPtr(global, fieldPemainLokal) == manajer;
}

bool bacaVektor(void* objek, const Field& bidang, Vektor2& nilai) {
    if (!objek || !bidang.field)
        return false;
    Field y = bidang;
    y.offset += sizeof(float);
    nilai.x = bacaFloat(objek, bidang);
    nilai.y = bacaFloat(objek, y);
    return std::isfinite(nilai.x) && std::isfinite(nilai.y);
}

void tulisVektor(void* objek, Field bidang, const Vektor2& nilai) {
    if (!objek || !bidang.field)
        return;
    tulisFloat(objek, bidang, nilai.x);
    bidang.offset += sizeof(float);
    tulisFloat(objek, bidang, nilai.y);
}

bool bacaTile(void* posisi, Vektor2& nilai) {
    if (!posisi || !fieldTile.field)
        return false;
    if (!tileBulat)
        return bacaVektor(posisi, fieldTile, nilai);
    Field y = fieldTile;
    y.offset += sizeof(int32_t);
    nilai.x = static_cast<float>(bacaInt(posisi, fieldTile));
    nilai.y = static_cast<float>(bacaInt(posisi, y));
    return std::isfinite(nilai.x) && std::isfinite(nilai.y);
}

// Tile bulat tidak boleh memakai pembulatan pustaka: nilainya ditulis ke field
// int32, jadi cukup geser setengah ke arah yang benar sebelum dipotong.
int32_t bulatkanTile(float nilai) {
    return static_cast<int32_t>(nilai >= 0.0f ? nilai + 0.5f : nilai - 0.5f);
}

void tulisTile(void* posisi, float x, float y) {
    if (!posisi || !fieldTile.field)
        return;
    if (!tileBulat) {
        Vektor2 nilai = {x, y};
        tulisVektor(posisi, fieldTile, nilai);
        return;
    }
    tulisInt(posisi, fieldTile, bulatkanTile(x));
    Field bidangY = fieldTile;
    bidangY.offset += sizeof(int32_t);
    tulisInt(posisi, bidangY, bulatkanTile(y));
}

void simpanPosisi(bool adaTile, const Vektor2& tile, const Vektor2& dunia) {
    pthread_mutex_lock(&kunciPosisi);
    posisiRingkas.ada = true;
    posisiRingkas.adaTile = adaTile;
    posisiRingkas.tileX = adaTile ? tile.x : 0.0f;
    posisiRingkas.tileY = adaTile ? tile.y : 0.0f;
    posisiRingkas.duniaX = dunia.x;
    posisiRingkas.duniaY = dunia.y;
    pthread_mutex_unlock(&kunciPosisi);
}

// Perpindahan pemain memberi pasangan (tile, dunia). Rasio keduanya adalah
// satuan dunia per tile, dan skalanya sama untuk kedua sumbu. Sampel baru
// dipakai setelah perpindahan minimal empat tile supaya pembulatan angka tile
// tidak mendominasi hasil.
bool tambahSampel(float deltaTile, float deltaDunia, const char* sumbu) {
    if (fabsf(deltaTile) < 4.0f)
        return false;
    float kandidat = deltaDunia / deltaTile;
    if (!std::isfinite(kandidat) || kandidat < 0.05f || kandidat > 4096.0f)
        return false;
    if (!kalibrasi.dariGerak) {
        kalibrasi.skala = kandidat;
        kalibrasi.dariGerak = true;
        kalibrasi.sampel = 1;
        LOGI("Teleport: skala %.2f dari gerakan %s", kandidat, sumbu);
        return true;
    }
    kalibrasi.skala = kalibrasi.skala * 0.8f + kandidat * 0.2f;
    kalibrasi.sampel++;
    if (kalibrasi.sampel % 8 == 0)
        LOGI("Teleport: skala %.2f dari %d sampel", kalibrasi.skala, kalibrasi.sampel);
    return true;
}

// Skala awal teleport diambil dari ukuran tile game. Bila field itu belum
// terbaca, collider pemain menjadi cadangan: size adalah ukuran badan dalam
// satuan dunia, sedangkan tile_Size adalah jumlah tile yang ditempati. Gerakan
// pemain menggantikan tebakan collider pada sampel pertama.
void cobaSkalaAwal(void* fisika) {
    if (kalibrasi.pasti || !fisika)
        return;

    // _Tile.pixel_Size adalah ukuran satu tile dalam satuan dunia dan berlaku
    // untuk kedua sumbu, jadi nilainya tidak bergantung pada model pemain.
    if (kelasTile && fieldUkuranPixel.field) {
        void* statis = dataFieldStatis(kelasTile);
        int32_t piksel = statis ? bacaInt(statis, fieldUkuranPixel) : 0;
        if (piksel > 0) {
            kalibrasi.skala = static_cast<float>(piksel);
            kalibrasi.siap = true;
            kalibrasi.pasti = true;
            LOGI("Teleport: skala awal %.2f dari pixel_Size", kalibrasi.skala);
            return;
        }
    }

    if (kalibrasi.siap)
        return;

    if (!fieldCollider.field || !fieldUkuranCollider.field || !fieldUkuranTile.field ||
        !fieldUkuranTileW.field || !fieldUkuranTileH.field)
        return;
    void* collider = bacaPtr(fisika, fieldCollider);
    if (!collider)
        return;
    void* ukuranTile = bacaPtr(collider, fieldUkuranTile);
    if (!ukuranTile)
        return;

    Vektor2 ukuran = {};
    if (!bacaVektor(collider, fieldUkuranCollider, ukuran))
        return;
    float tileW = bacaFloat(ukuranTile, fieldUkuranTileW);
    float tileH = bacaFloat(ukuranTile, fieldUkuranTileH);

    float kandidat = 0.0f;
    if (std::isfinite(tileW) && tileW > 0.01f && ukuran.x > 0.01f)
        kandidat = ukuran.x / tileW;
    else if (std::isfinite(tileH) && tileH > 0.01f && ukuran.y > 0.01f)
        kandidat = ukuran.y / tileH;
    if (!std::isfinite(kandidat) || kandidat < 0.05f || kandidat > 4096.0f)
        return;

    kalibrasi.skala = kandidat;
    kalibrasi.siap = true;
    LOGI("Teleport: skala awal %.2f dari collider", kandidat);
}

void perbaruiKalibrasi(const Vektor2& dunia, const Vektor2& tile) {
    if (!kalibrasi.adaAcuan) {
        kalibrasi.acuanDuniaX = dunia.x;
        kalibrasi.acuanDuniaY = dunia.y;
        kalibrasi.acuanTileX = tile.x;
        kalibrasi.acuanTileY = tile.y;
        float skala = kalibrasi.siap ? kalibrasi.skala : 1.0f;
        kalibrasi.offsetX = dunia.x - tile.x * skala;
        kalibrasi.offsetY = dunia.y - tile.y * skala;
        kalibrasi.adaAcuan = true;
        return;
    }

    // Skala dari pixel_Size sudah pasti, jadi sampel gerakan tidak boleh
    // menggantinya. Titik nol tetap diperbarui untuk jalur tanpa field tile.
    if (kalibrasi.pasti) {
        kalibrasi.offsetX = dunia.x - tile.x * kalibrasi.skala;
        kalibrasi.offsetY = dunia.y - tile.y * kalibrasi.skala;
        return;
    }

    float deltaTileX = tile.x - kalibrasi.acuanTileX;
    float deltaTileY = tile.y - kalibrasi.acuanTileY;
    if (fabsf(deltaTileX) < 0.5f && fabsf(deltaTileY) < 0.5f)
        return;

    bool ambil = false;
    if (fabsf(deltaTileX) >= fabsf(deltaTileY)) {
        ambil = tambahSampel(deltaTileX, dunia.x - kalibrasi.acuanDuniaX, "X");
    } else {
        ambil = tambahSampel(deltaTileY, dunia.y - kalibrasi.acuanDuniaY, "Y");
    }
    if (!ambil)
        return;

    kalibrasi.siap = true;
    kalibrasi.acuanDuniaX = dunia.x;
    kalibrasi.acuanDuniaY = dunia.y;
    kalibrasi.acuanTileX = tile.x;
    kalibrasi.acuanTileY = tile.y;
    kalibrasi.offsetX = dunia.x - tile.x * kalibrasi.skala;
    kalibrasi.offsetY = dunia.y - tile.y * kalibrasi.skala;
}

Vektor2 keDunia(float tileX, float tileY, const Vektor2& duniaSekarang,
                const Vektor2& tileSekarang, bool adaTileSekarang) {
    float skala = kalibrasi.siap ? kalibrasi.skala : 1.0f;
    // Titik acuan sekarang membatalkan selisih tetap antara tile dan dunia,
    // jadi rumus ini tetap benar walau titik nol sumbu Y belum pernah dihitung.
    if (adaTileSekarang) {
        Vektor2 hasil = {duniaSekarang.x + (tileX - tileSekarang.x) * skala,
                         duniaSekarang.y + (tileY - tileSekarang.y) * skala};
        return hasil;
    }
    Vektor2 hasil = {tileX * skala, tileY * skala};
    if (kalibrasi.adaAcuan) {
        hasil.x = kalibrasi.offsetX + tileX * skala;
        hasil.y = kalibrasi.offsetY + tileY * skala;
    }
    return hasil;
}

bool tulisPosisiFisika(void* posisi, float tileX, float tileY, const Vektor2& dunia) {
    if (!posisi || !fieldDunia.field)
        return false;

    tulisVektor(posisi, fieldDunia, dunia);
    tulisTile(posisi, tileX, tileY);

    void* jejak = bacaPtr(posisi, fieldJejak);
    if (jejak) {
        tulisVektor(jejak, fieldJejakPrev, dunia);
        tulisVektor(jejak, fieldJejakCheck, dunia);
        tulisVektor(jejak, fieldJejakBaru, dunia);
        tulisVektor(jejak, fieldJejakAkhir, dunia);
    }
    // Titik acuan berikutnya adalah tujuan teleport, bukan titik lama yang
    // jaraknya bisa ribuan tile, supaya kalibrasi tidak memakai lompatan ini
    // sebagai sampel gerakan.
    kalibrasi.adaAcuan = true;
    kalibrasi.acuanDuniaX = dunia.x;
    kalibrasi.acuanDuniaY = dunia.y;
    kalibrasi.acuanTileX = tileX;
    kalibrasi.acuanTileY = tileY;
    float skala = kalibrasi.siap ? kalibrasi.skala : 1.0f;
    kalibrasi.offsetX = dunia.x - tileX * skala;
    kalibrasi.offsetY = dunia.y - tileY * skala;
    return true;
}

void kosongkanKecepatan(void* fisika) {
    void* kecepatan = bacaPtr(fisika, fieldKecepatan);
    if (!kecepatan)
        return;
    tulisFloat(kecepatan, fieldKecepatanX, 0.0f);
    tulisFloat(kecepatan, fieldKecepatanY, 0.0f);
    // Tambahan kecepatan ikut dipakai Pos__Check pada frame berikutnya, jadi
    // sisa langkah sebelum teleport juga harus hilang.
    void* tambahan = bacaPtr(kecepatan, fieldKecepatanAdd);
    if (!tambahan)
        return;
    tulisFloat(tambahan, fieldKecepatanAddX, 0.0f);
    tulisFloat(tambahan, fieldKecepatanAddY, 0.0f);
}

// Game memakai flag ini untuk membangun ulang tile di sekitar badan, jadi tile
// tujuan langsung dipakai pada frame berikutnya.
void segarkanTile(void* fisika) {
    if (fieldSegar.field)
        tulisBool(fisika, fieldSegar, true);
}

void verifikasiTeleport(void* posisi) {
    if (verifikasi.sisa <= 0)
        return;
    verifikasi.sisa--;
    if (verifikasi.sisa > 0)
        return;

    Vektor2 dunia = {};
    Vektor2 tile = {};
    bool adaDunia = bacaVektor(posisi, fieldDunia, dunia);
    bool adaTile = bacaTile(posisi, tile);
    if (!adaDunia) {
        LOGE("Teleport: posisi sesudah tidak terbaca");
        return;
    }
    if (adaTile) {
        LOGI("Teleport: sesudah tile=(%.2f, %.2f) dunia=(%.1f, %.1f) selisih=(%.2f, %.2f)/"
             "(%.1f, %.1f)",
             tile.x, tile.y, dunia.x, dunia.y, tile.x - verifikasi.tileX, tile.y - verifikasi.tileY,
             dunia.x - verifikasi.dunia.x, dunia.y - verifikasi.dunia.y);
    } else {
        LOGI("Teleport: sesudah dunia=(%.1f, %.1f) selisih=(%.1f, %.1f)", dunia.x, dunia.y,
             dunia.x - verifikasi.dunia.x, dunia.y - verifikasi.dunia.y);
    }
}

void terapkanTeleport(void* manajer) {
    PermintaanTeleport& permintaan = permintaanTeleport();
    if (!permintaan.menunggu.exchange(false))
        return;

    void* fisika = bacaPtr(manajer, fieldFisika);
    if (!fisika) {
        LOGE("Teleport: fisika pemain tidak ditemukan");
        return;
    }
    void* posisi = bacaPtr(fisika, fieldPosisi);
    if (!posisi) {
        LOGE("Teleport: posisi fisika tidak terbaca");
        return;
    }

    int32_t tileX = permintaan.x.load();
    int32_t tileY = permintaan.y.load();
    if (tileX < -kBatasTeleport || tileX > kBatasTeleport || tileY < -kBatasTeleport ||
        tileY > kBatasTeleport) {
        LOGE("Teleport ditolak: tile %d, %d di luar batas -%d..%d", tileX, tileY, kBatasTeleport,
             kBatasTeleport);
        return;
    }

    Vektor2 duniaLama = {};
    Vektor2 tileLama = {};
    bool adaDunia = bacaVektor(posisi, fieldDunia, duniaLama);
    bool adaTile = bacaTile(posisi, tileLama);
    if (!adaDunia) {
        LOGE("Teleport ditolak: posisi dunia tidak terbaca");
        return;
    }
    cobaSkalaAwal(fisika);
    if (adaTile) {
        LOGI("Teleport: sebelum tile=(%.2f, %.2f) dunia=(%.1f, %.1f)", tileLama.x, tileLama.y,
             duniaLama.x, duniaLama.y);
    } else {
        LOGI("Teleport: sebelum dunia=(%.1f, %.1f), tile tidak terbaca", duniaLama.x, duniaLama.y);
    }

    Vektor2 duniaBaru = keDunia(static_cast<float>(tileX), static_cast<float>(tileY), duniaLama,
                                tileLama, adaTile);
    if (!std::isfinite(duniaBaru.x) || !std::isfinite(duniaBaru.y)) {
        LOGE("Teleport ditolak: hasil konversi bukan angka");
        return;
    }
    if (!kalibrasi.siap) {
        LOGI("Teleport: skala belum terkalibrasi, satuan tile dipakai apa adanya");
    }

    LOGI("Teleport: target tile=(%d, %d) dunia=(%.1f, %.1f) skala=%.2f offset=(%.2f, %.2f)",
         tileX, tileY, duniaBaru.x, duniaBaru.y, kalibrasi.siap ? kalibrasi.skala : 1.0f,
         kalibrasi.offsetX, kalibrasi.offsetY);
    if (!tulisPosisiFisika(posisi, static_cast<float>(tileX), static_cast<float>(tileY),
                           duniaBaru)) {
        LOGE("Teleport: posisi fisika tidak bisa ditulis");
        return;
    }
    kosongkanKecepatan(fisika);
    segarkanTile(fisika);

    verifikasi.sisa = 3;
    verifikasi.tileX = static_cast<float>(tileX);
    verifikasi.tileY = static_cast<float>(tileY);
    verifikasi.dunia = duniaBaru;
}

float ambilKelipatan(int32_t nilai, float minimum, float maksimum) {
    float hasil = static_cast<float>(nilai) / 10.0f;
    if (!std::isfinite(hasil))
        return 1.0f;
    if (hasil < minimum)
        return minimum;
    if (hasil > maksimum)
        return maksimum;
    return hasil;
}

// Physics__Manager.a menunjuk Manager, dan Manager.player adalah pemain lokal.
// Pos__Check dipanggil untuk setiap badan fisika, jadi hook hanya menyentuh
// badan yang sama dengan Player__Manager.phy milik pemain lokal.
bool fisikaMilikPemain(void* manajerFisika, void* fisika) {
    void* global = bacaPtr(manajerFisika, fieldAktorFisika);
    void* pemain = bacaPtr(global, fieldPemainLokal);
    return pemain && bacaPtr(pemain, fieldFisika) == fisika;
}

// Pos__Check hanya menghitung _world.check = pos.world + speed.add. Tambahan
// kecepatan diskalakan tepat sebelum perhitungan dan dikembalikan sesudahnya,
// jadi nilai asli game tidak pernah tertimpa permanen. Sumbu Y tidak disentuh
// karena gravitasi punya slider sendiri.
void hookPosCheck(void* sendiri, void** fisika, void* metode) {
    auto asli = reinterpret_cast<FnPosCheck>(target(T_POS_CHECK).orig);
    if (!asli)
        return;

    float kelipatan = ambilKelipatan(fitur().kecepatanPemain.load(), 0.5f, 5.0f);
    void* objek = fisika ? *fisika : nullptr;
    if (kelipatan == 1.0f || !objek || !fisikaMilikPemain(sendiri, objek)) {
        asli(sendiri, fisika, metode);
        return;
    }

    void* kecepatan = bacaPtr(objek, fieldKecepatan);
    void* tambahan = kecepatan ? bacaPtr(kecepatan, fieldKecepatanAdd) : nullptr;
    float asliX = tambahan ? bacaFloat(tambahan, fieldKecepatanAddX) : 0.0f;
    if (!tambahan || !std::isfinite(asliX)) {
        asli(sendiri, fisika, metode);
        return;
    }

    tulisFloat(tambahan, fieldKecepatanAddX, asliX * kelipatan);
    asli(sendiri, fisika, metode);
    tulisFloat(tambahan, fieldKecepatanAddX, asliX);
}

// Transform__Gravity menarik speed.y ke bawah selama di udara. Selisih satu
// panggilan itu yang diskalakan, jadi nilai lain tidak ikut berubah. Rasio 0,0x
// membuang tarikan turun: kecepatan turun langsung dinolkan supaya pemain tidak
// jatuh, sedangkan dorongan naik dari lompatan dibiarkan dan baru berhenti saat
// gravitasi dinaikkan lagi.
void hookTransformGravitasi(void* sendiri, void* metode) {
    auto asli = reinterpret_cast<FnGravitasi>(target(T_AIR_GRAVITY).orig);
    if (!asli)
        return;

    float rasio = ambilKelipatan(fitur().gravitasiPemain.load(), 0.0f, 3.0f);
    void* pemain = sendiri ? bacaPtr(sendiri, fieldPemainStatus) : nullptr;
    void* fisika = pemain && pemainLokal(pemain) ? bacaPtr(pemain, fieldFisika) : nullptr;
    void* kecepatan = fisika ? bacaPtr(fisika, fieldKecepatan) : nullptr;
    if (rasio == 1.0f || !kecepatan) {
        asli(sendiri, metode);
        return;
    }

    float sebelum = bacaFloat(kecepatan, fieldKecepatanY);
    asli(sendiri, metode);
    float sesudah = bacaFloat(kecepatan, fieldKecepatanY);
    if (!std::isfinite(sebelum) || !std::isfinite(sesudah))
        return;
    float delta = sesudah - sebelum;
    if (fabsf(delta) < 0.0001f)
        return;

    float hasil = rasio <= 0.0f ? (sebelum > 0.0f ? sebelum : 0.0f) : sebelum + delta * rasio;
    if (std::isfinite(hasil))
        tulisFloat(kecepatan, fieldKecepatanY, hasil);
}

// Kotak gerak dari tile dipakai pemain saat melangkah, jadi sisi kiri, kanan,
// dan atas dibuka selebar mungkin. Sisi bawah dibiarkan supaya lantai tetap
// dipijak.
void bebaskanDinding(void* fisika) {
    void* margin = fisika ? bacaPtr(fisika, fieldMargin) : nullptr;
    if (!margin)
        return;
    int32_t jumlah = panjangArray(margin);
    char* data = dataArray(margin);
    for (int32_t i = 0; i < jumlah; i++) {
        void* entri = *reinterpret_cast<void**>(data + static_cast<size_t>(i) * sizeof(void*));
        if (!entri)
            continue;
        tulisFloat(entri, fieldMarginAtas, kBatasBebas);
        tulisFloat(entri, fieldMarginKiri, -kBatasBebas);
        tulisFloat(entri, fieldMarginKanan, kBatasBebas);
    }
}

// Deteksi fisika berjalan untuk setiap badan di dunia, jadi hanya badan yang
// sama dengan fisika pemain lokal yang dibebaskan.
bool fisikaPemainSendiri(void* deteksi, void** fisika) {
    void* global = bacaPtr(deteksi, fieldDeteksiAktor);
    void* manajer = bacaPtr(global, fieldPemainLokal);
    void* objek = fisika ? *fisika : nullptr;
    return manajer && objek && bacaPtr(manajer, fieldFisika) == objek;
}

// Dipasang tepat setelah kotak gerak dihitung dari tile dan sebelum pemain
// memakainya untuk melangkah, jadi dinding hilang pada frame yang sama.
void hookDeteksiFisika(void* sendiri, void** fisika, int32_t waktu, float posX, float posY,
                       void* metode) {
    auto asli = reinterpret_cast<FnDeteksi>(target(T_DETECT_SET).orig);
    if (!asli)
        return;
    asli(sendiri, fisika, waktu, posX, posY, metode);
    if (!fitur().noClip || !fisikaPemainSendiri(sendiri, fisika))
        return;
    bebaskanDinding(fisika ? *fisika : nullptr);
}

void periksaTipeTile() {
    if (!fieldTile.field)
        return;
    void* kelas = kelasField(fieldTile);
    const char* nama = kelas ? apiIl2cpp().className(kelas) : nullptr;
    tileBulat = nama && strstr(nama, "Int") != nullptr;
    LOGI("Teleport: field tile %s (%s)", nama ? nama : "tanpa nama",
         tileBulat ? "bilangan bulat" : "pecahan");
}

bool cariFieldFisika(void* kelasManajer) {
    if (!cariField(kelasManajer, "phy", fieldFisika))
        return false;
    void* kelasFisika = kelasField(fieldFisika);
    if (!cariField(kelasFisika, "pos", fieldPosisi) ||
        !cariField(kelasFisika, "speed", fieldKecepatan) ||
        !cariField(kelasFisika, "refresh", fieldSegar) ||
        !cariField(kelasFisika, "margin", fieldMargin))
        return false;

    void* kelasPosisi = kelasField(fieldPosisi);
    if (!cariField(kelasPosisi, "world", fieldDunia) ||
        !cariField(kelasPosisi, "_world", fieldJejak))
        return false;
    // Tile adalah posisi yang dipakai fisika untuk mencari tabrakan. Tanpa field
    // ini teleport kembali menulis world saja dan bisa ditarik ke tile lama.
    if (!cariField(kelasPosisi, "tile", fieldTile))
        LOGE("Field tile tidak ditemukan, teleport memakai world saja");
    periksaTipeTile();

    void* kelasJejak = kelasField(fieldJejak);
    if (!cariField(kelasJejak, "prev", fieldJejakPrev) ||
        !cariField(kelasJejak, "check", fieldJejakCheck) ||
        !cariField(kelasJejak, "new_", fieldJejakBaru) ||
        !cariField(kelasJejak, "end", fieldJejakAkhir))
        return false;

    void* kelasKecepatan = kelasField(fieldKecepatan);
    if (!cariField(kelasKecepatan, "x", fieldKecepatanX) ||
        !cariField(kelasKecepatan, "y", fieldKecepatanY))
        return false;

    // add dipakai Pos__Check untuk menghitung langkah berikutnya. Bila field
    // ini hilang, hook kecepatan dilewati tetapi teleport dan No Clip tetap
    // berjalan.
    if (!cariField(kelasKecepatan, "add", fieldKecepatanAdd) ||
        !cariField(kelasField(fieldKecepatanAdd), "x", fieldKecepatanAddX) ||
        !cariField(kelasField(fieldKecepatanAdd), "y", fieldKecepatanAddY)) {
        fieldKecepatanAdd = {};
        fieldKecepatanAddX = {};
        fieldKecepatanAddY = {};
        LOGE("Field speed.add tidak ditemukan; Kecepatan pemain nonaktif");
    } else {
        LOGI("Field kecepatan tambahan siap");
    }

    // Collider hanya dipakai untuk menebak skala awal teleport. Bila salah satu
    // field tidak ada, teleport tetap memakai skala dari gerakan pemain.
    if (cariField(kelasFisika, "collider", fieldCollider) &&
        cariField(kelasField(fieldCollider), "size", fieldUkuranCollider) &&
        cariField(kelasField(fieldCollider), "tile_Size", fieldUkuranTile) &&
        cariField(kelasField(fieldUkuranTile), "w", fieldUkuranTileW) &&
        cariField(kelasField(fieldUkuranTile), "h", fieldUkuranTileH)) {
        LOGI("Field collider siap");
    } else {
        fieldUkuranCollider = {};
        fieldUkuranTile = {};
        fieldUkuranTileW = {};
        fieldUkuranTileH = {};
        LOGE("Field collider tidak lengkap; skala awal teleport dilewati");
    }
    return true;
}

bool cariFieldMargin() {
    void* kelasMargin = kelasElemenField(fieldMargin);
    return cariField(kelasMargin, "top", fieldMarginAtas) &&
           cariField(kelasMargin, "left", fieldMarginKiri) &&
           cariField(kelasMargin, "right", fieldMarginKanan);
}

} // namespace

// Dipanggil di akhir update pemain lokal, di thread game, karena posisi dan
// fisika game tidak boleh disentuh dari thread menu.
void saatUpdatePemain(void* manajer) {
    if (!fieldAktorPemain.field || !fieldPemainLokal.field || !fieldFisika.field)
        return;
    if (!pemainLokal(manajer))
        return;

    void* fisika = bacaPtr(manajer, fieldFisika);
    void* posisi = fisika ? bacaPtr(fisika, fieldPosisi) : nullptr;
    if (posisi) {
        Vektor2 dunia = {};
        Vektor2 tile = {};
        bool adaDunia = bacaVektor(posisi, fieldDunia, dunia);
        bool adaTile = bacaTile(posisi, tile);
        if (adaDunia && sekarangDetik() < posisiDiminta.load())
            simpanPosisi(adaTile, tile, dunia);
        if (adaDunia && adaTile) {
            cobaSkalaAwal(fisika);
            perbaruiKalibrasi(dunia, tile);
        }
        verifikasiTeleport(posisi);
    }

    terapkanTeleport(manajer);

    int nilaiKecepatan = fitur().kecepatanPemain.load();
    if (nilaiKecepatan != logKecepatan) {
        logKecepatan = nilaiKecepatan;
        LOGI("Kecepatan pemain: %.1fx", ambilKelipatan(nilaiKecepatan, 0.5f, 5.0f));
    }
    int nilaiGravitasi = fitur().gravitasiPemain.load();
    if (nilaiGravitasi != logGravitasi) {
        logGravitasi = nilaiGravitasi;
        LOGI("Gravitasi pemain: %.1fx", ambilKelipatan(nilaiGravitasi, 0.0f, 3.0f));
    }
}

// Menyalin posisi terakhir untuk baris menu. Formatnya:
// tileX<TAB>tileY<TAB>duniaX<TAB>duniaY, dengan bagian tile kosong bila field
// tile tidak tersedia.
void salinPosisiPemain(char* keluaran, size_t ukuran) {
    if (!keluaran || ukuran == 0)
        return;
    keluaran[0] = '\0';
    posisiDiminta.store(sekarangDetik() + kMasaPosisi);

    PosisiRingkas salinan = {};
    pthread_mutex_lock(&kunciPosisi);
    salinan = posisiRingkas;
    pthread_mutex_unlock(&kunciPosisi);

    if (!salinan.ada)
        return;
    if (salinan.adaTile) {
        snprintf(keluaran, ukuran, "%.2f\t%.2f\t%.1f\t%.1f", salinan.tileX, salinan.tileY,
                 salinan.duniaX, salinan.duniaY);
    } else {
        snprintf(keluaran, ukuran, "\t\t%.1f\t%.1f", salinan.duniaX, salinan.duniaY);
    }
}

void pasangHookKecepatanGravitasi() {
    // Pos__Check dipanggil untuk semua badan fisika, jadi butuh Physics__Manager.a
    // untuk mengenali pemain lokal.
    if (!cariField(cariKelas("Physics__Manager"), "a", fieldAktorFisika))
        LOGE("Field fisika manajer tidak ditemukan; Kecepatan pemain nonaktif");
    else if (!fieldKecepatanAdd.field || !fieldKecepatanAddX.field)
        LOGE("Field speed.add tidak lengkap; Kecepatan pemain nonaktif");
    else
        pasangHook(target(T_POS_CHECK), reinterpret_cast<void*>(hookPosCheck));

    if (!cariField(cariKelas("Player__State__Std__Air"), "m", fieldPemainStatus))
        LOGE("Field gravitasi pemain tidak ditemukan; Gravitasi pemain nonaktif");
    else
        pasangHook(target(T_AIR_GRAVITY), reinterpret_cast<void*>(hookTransformGravitasi));
}

void pasangHookPemain() {
    void* kelasManajer = kelasDari("Player__Manager");
    if (!cariField(kelasManajer, "a", fieldAktorPemain) ||
        !cariField(kelasField(fieldAktorPemain), "player", fieldPemainLokal)) {
        LOGE("Field pemain tidak lengkap, Teleport, Kecepatan, Gravitasi, dan No Clip nonaktif");
        return;
    }
    if (!cariFieldFisika(kelasManajer)) {
        LOGE("Field fisika pemain tidak lengkap, Teleport, Kecepatan, dan No Clip nonaktif");
        return;
    }
    LOGI("Field pemain OK");

    kelasTile = cariKelasField("_Tile", "pixel_Size");
    if (kelasTile && cariField(kelasTile, "pixel_Size", fieldUkuranPixel))
        LOGI("Field ukuran tile siap");
    else
        LOGE("Field pixel_Size tidak ditemukan; skala awal teleport memakai collider");

    pasangHookKecepatanGravitasi();

    void* kelasDeteksi = cariKelas("Physics__Detect");
    bool siapDinding = cariField(kelasDeteksi, "a", fieldDeteksiAktor) && cariFieldMargin();
    if (!siapDinding || !target(T_DETECT_SET).addr) {
        LOGE("Field tembus dinding tidak lengkap; No Clip nonaktif");
        return;
    }
    LOGI("Field tembus dinding siap");
    pasangHook(target(T_DETECT_SET), reinterpret_cast<void*>(hookDeteksiFisika));
}
