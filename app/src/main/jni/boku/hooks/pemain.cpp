#include "hooks.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "fitur.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "player_service.h"
#include "targets.h"

namespace {

// Vector2 hanya dua float berurutan, ditulis langsung ke field struct milik game.
struct Vektor2 {
    float x;
    float y;
};

using FnDeteksi = void (*)(void*, void**, int32_t, float, float, void*);
using FnPosCheck = void (*)(void*, void**, void*);

// Batas yang dipakai No Clip. Nilainya jauh di luar ukuran dunia mana pun, jadi
// pemeriksaan tabrakan tidak pernah menyempitkannya lagi.
constexpr float kBatasBebas = 1.0e9f;
constexpr int32_t kBatasTeleport = 1024;

Field fieldAktorPemain = {};
Field fieldPemainLokal = {};
Field fieldAktorFisika = {};
Field fieldDeteksiAktor = {};

// Batas langkah yang dipakai Speed_Limit game. Nilainya sama dengan
// _Tile.pixel_Size - 1 dan juga dibaca kamera sebagai jarak maksimum sebelum
// bergerak, jadi hasil skala kecepatan harus tetap berada di dalamnya.
Field fieldBatasKecepatan = {};
Field fieldBatasKecepatanNilai = {};

// Tinggi dunia dipakai untuk menerjemahkan Y menu ke Y UI game.
// Tile__Manager.Display__Y menghitung Y UI = Y tile - h/2, jadi
// Y tile = Y UI + h/2.
Field fieldAktorTile = {};
Field fieldUkuranDunia = {};
Field fieldTinggiDunia = {};

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
int logTinggiDunia = -1;

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

// Tinggi dunia dalam tile. UI game menaruh Y 0 di tengah dunia, jadi
// Y UI = Y tile - h/2. Bila field belum terbaca, 0 berarti tanpa geseran dan
// Y tile dipakai apa adanya.
int32_t tinggiDuniaTile(void* manajerPemain) {
    if (!manajerPemain || !fieldAktorPemain.field || !fieldAktorTile.field ||
        !fieldUkuranDunia.field || !fieldTinggiDunia.field)
        return 0;
    void* manajer = bacaPtr(manajerPemain, fieldAktorPemain);
    void* tile = manajer ? bacaPtr(manajer, fieldAktorTile) : nullptr;
    void* ukuran = tile ? bacaPtr(tile, fieldUkuranDunia) : nullptr;
    int32_t tinggi = ukuran ? bacaInt(ukuran, fieldTinggiDunia) : 0;
    return tinggi > 0 && tinggi <= 100000 ? tinggi : 0;
}

void simpanPosisi(bool adaTile, const Vektor2& tile, const Vektor2& dunia) {
    pemain::simpanPosisi(adaTile, tile.x, tile.y, dunia.x, dunia.y);
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

Vektor2 keDunia(float tileX, float tileY, const Vektor2& duniaSekarang, const Vektor2& tileSekarang,
                bool adaTileSekarang) {
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
    int32_t uiY = permintaan.y.load();
    if (tileX < -kBatasTeleport || tileX > kBatasTeleport || uiY < -kBatasTeleport ||
        uiY > kBatasTeleport) {
        LOGE("Teleport ditolak: X %d, Y UI %d di luar batas -%d..%d", tileX, uiY, kBatasTeleport,
             kBatasTeleport);
        return;
    }
    // Menu memakai Y UI game: 0 adalah tengah dunia, sama dengan
    // Tile__Manager.Display__Y (Y tile - h/2). Fisika memakai Y tile, jadi
    // Y UI digeser setengah tinggi dunia.
    int32_t tileY = uiY + tinggiDuniaTile(manajer) / 2;

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

    Vektor2 duniaBaru =
        keDunia(static_cast<float>(tileX), static_cast<float>(tileY), duniaLama, tileLama, adaTile);
    if (!std::isfinite(duniaBaru.x) || !std::isfinite(duniaBaru.y)) {
        LOGE("Teleport ditolak: hasil konversi bukan angka");
        return;
    }
    if (!kalibrasi.siap) {
        LOGI("Teleport: skala belum terkalibrasi, satuan tile dipakai apa adanya");
    }

    LOGI("Teleport: target UI=(%d, %d) tile=(%d, %d) dunia=(%.1f, %.1f) skala=%.2f "
         "offset=(%.2f, %.2f)",
         tileX, uiY, tileX, tileY, duniaBaru.x, duniaBaru.y,
         kalibrasi.siap ? kalibrasi.skala : 1.0f, kalibrasi.offsetX, kalibrasi.offsetY);
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

// Speed_Limit game membatasi speed.add.x dan speed.add.y ke ±_speed.limit
// sebelum Pos__Check. Hasil skala mod memakai batas yang sama supaya langkah
// tidak pernah melewati satu tile; kalau lewat, border dan tile di antaranya
// tidak sempat diperbarui dan render ikut rusak.
float batasLangkah(void* manajerFisika) {
    return bacaFloat(bacaPtr(manajerFisika, fieldBatasKecepatan), fieldBatasKecepatanNilai);
}

float batasiLangkah(float nilai, float batas) {
    if (!std::isfinite(batas) || batas <= 0.0f)
        return nilai;
    if (nilai > batas)
        return batas;
    if (nilai < -batas)
        return -batas;
    return nilai;
}

// Physics__Manager.a menunjuk Manager, dan Manager.player adalah pemain lokal.
// Pos__Check dipanggil untuk setiap badan fisika, jadi hook hanya menyentuh
// badan yang sama dengan Player__Manager.phy milik pemain lokal.
bool fisikaMilikPemain(void* manajerFisika, void* fisika) {
    void* global = bacaPtr(manajerFisika, fieldAktorFisika);
    void* pemain = bacaPtr(global, fieldPemainLokal);
    return pemain && bacaPtr(pemain, fieldFisika) == fisika;
}

// Pos__Check hanya menghitung _world.check = pos.world + speed.add. Semua
// gerakan pemain, termasuk terbang, berenang, dan meluncur, lewat titik ini,
// jadi kecepatan X dan komponen turun Y diskalakan di sini lalu dikembalikan.
// Komponen naik tidak disentuh supaya lompatan tetap sama.
void hookPosCheck(void* sendiri, void** fisika, void* metode) {
    auto asli = reinterpret_cast<FnPosCheck>(target(T_POS_CHECK).orig);
    if (!asli)
        return;

    float kelipatan = ambilKelipatan(fitur().kecepatanPemain.load(), 0.5f, 5.0f);
    float rasio = ambilKelipatan(fitur().gravitasiPemain.load(), 0.0f, 3.0f);
    void* objek = fisika ? *fisika : nullptr;
    if ((kelipatan == 1.0f && rasio == 1.0f) || !objek || !fisikaMilikPemain(sendiri, objek)) {
        asli(sendiri, fisika, metode);
        return;
    }

    void* kecepatan = bacaPtr(objek, fieldKecepatan);
    void* tambahan = kecepatan ? bacaPtr(kecepatan, fieldKecepatanAdd) : nullptr;
    if (!tambahan) {
        asli(sendiri, fisika, metode);
        return;
    }
    float asliX = bacaFloat(tambahan, fieldKecepatanAddX);
    float asliY = bacaFloat(tambahan, fieldKecepatanAddY);
    if (!std::isfinite(asliX) || !std::isfinite(asliY)) {
        asli(sendiri, fisika, metode);
        return;
    }

    float batas = batasLangkah(sendiri);
    if (kelipatan != 1.0f)
        tulisFloat(tambahan, fieldKecepatanAddX, batasiLangkah(asliX * kelipatan, batas));
    // Hanya gerak turun yang mengikuti slider gravitasi. Dorongan naik dari
    // lompatan dibiarkan, jadi 0,0x membuat pemain melayang.
    if (rasio != 1.0f && asliY < 0.0f)
        tulisFloat(tambahan, fieldKecepatanAddY, batasiLangkah(asliY * rasio, batas));

    asli(sendiri, fisika, metode);

    tulisFloat(tambahan, fieldKecepatanAddX, asliX);
    tulisFloat(tambahan, fieldKecepatanAddY, asliY);
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
        LOGE("Field speed.add tidak ditemukan; Kecepatan dan Gravitasi pemain nonaktif");
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

// Manager.tile._world_Size.h adalah tinggi dunia dalam tile, sumber yang sama
// dengan Tile__Manager.Display__Y saat daftar zona belum siap. Field ini dipakai
// untuk menerjemahkan Y menu ke Y UI game; bila hilang, Y tile dipakai apa
// adanya dan hanya muncul peringatan di log.
bool cariFieldDunia() {
    if (!cariField(kelasField(fieldAktorPemain), "tile", fieldAktorTile) ||
        !cariField(kelasField(fieldAktorTile), "_world_Size", fieldUkuranDunia) ||
        !cariField(kelasField(fieldUkuranDunia), "h", fieldTinggiDunia)) {
        fieldAktorTile = {};
        fieldUkuranDunia = {};
        fieldTinggiDunia = {};
        LOGE("Field tinggi dunia tidak lengkap; Titik Y memakai tile apa adanya");
        return false;
    }
    LOGI("Field tinggi dunia siap");
    return true;
}

} // namespace

// Field pembanding pemain lokal dicari sebelum hook fitur dipasang, supaya hook
// yang berjalan untuk semua pemain sudah bisa membedakan badan tamu sejak
// panggilan pertama. Dipanggil juga oleh pasangHookPemain.
void cariFieldPemainLokal() {
    void* kelasManajer = kelasDari("Player__Manager");
    cariField(kelasManajer, "a", fieldAktorPemain);
    cariField(kelasField(fieldAktorPemain), "player", fieldPemainLokal);
}

// Pembanding untuk hook yang berjalan pada state milik semua pemain. Selama
// field pembanding belum siap, fitur dibiarkan berjalan seperti sebelumnya agar
// tidak mati diam-diam ketika nama field berubah.
bool pemainLokalHook(void* manajer) {
    if (!manajer)
        return false;
    if (!fieldAktorPemain.field || !fieldPemainLokal.field)
        return true;
    return pemainLokal(manajer);
}

// Dipanggil di akhir update pemain lokal, di thread game, karena posisi dan
// fisika game tidak boleh disentuh dari thread menu.
void saatUpdatePemain(void* manajer) {
    if (!fieldAktorPemain.field || !fieldPemainLokal.field || !fieldFisika.field)
        return;
    if (!pemainLokal(manajer))
        return;

    void* fisika = bacaPtr(manajer, fieldFisika);
    void* posisi = fisika ? bacaPtr(fisika, fieldPosisi) : nullptr;
    int32_t tinggi = tinggiDuniaTile(manajer);
    if (tinggi != logTinggiDunia) {
        logTinggiDunia = tinggi;
        if (tinggi > 0)
            LOGI("Titik Y UI: tinggi dunia %d tile, Y UI 0 di tile %d", tinggi, tinggi / 2);
        else
            LOGI("Titik Y UI: tinggi dunia tidak terbaca, Y tile dipakai apa adanya");
    }
    if (posisi) {
        Vektor2 dunia = {};
        Vektor2 tile = {};
        bool adaDunia = bacaVektor(posisi, fieldDunia, dunia);
        bool adaTile = bacaTile(posisi, tile);
        if (adaDunia && pemain::layaniPosisi()) {
            Vektor2 tileUi = tile;
            tileUi.y -= static_cast<float>(tinggi) * 0.5f;
            simpanPosisi(adaTile, tileUi, dunia);
        }
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

void pasangHookGerak() {
    // Pos__Check dipanggil untuk semua badan fisika, jadi butuh Physics__Manager.a
    // untuk mengenali pemain lokal. Kecepatan dan gravitasi memakai titik yang sama,
    // dan batas langkahnya dibaca dari Physics__Manager._speed.limit milik game.
    void* kelasFisikaManajer = cariKelas("Physics__Manager");
    if (!cariField(kelasFisikaManajer, "a", fieldAktorFisika))
        LOGE("Field fisika manajer tidak ditemukan; Kecepatan dan Gravitasi pemain nonaktif");
    else if (!fieldKecepatanAdd.field || !fieldKecepatanAddX.field || !fieldKecepatanAddY.field)
        LOGE("Field speed.add tidak lengkap; Kecepatan dan Gravitasi pemain nonaktif");
    else {
        if (cariField(kelasFisikaManajer, "_speed", fieldBatasKecepatan) &&
            cariField(kelasField(fieldBatasKecepatan), "limit", fieldBatasKecepatanNilai))
            LOGI("Field batas kecepatan siap");
        else {
            fieldBatasKecepatan = {};
            fieldBatasKecepatanNilai = {};
            LOGE("Field batas kecepatan tidak ditemukan; langkah mengikuti batas game saja");
        }
        pasangHook(target(T_POS_CHECK), reinterpret_cast<void*>(hookPosCheck));
    }
}

void pasangHookPemain() {
    cariFieldPemainLokal();
    void* kelasManajer = kelasDari("Player__Manager");
    if (!fieldAktorPemain.field || !fieldPemainLokal.field) {
        LOGE("Field pemain tidak lengkap, Teleport, Kecepatan, Gravitasi, dan No Clip nonaktif");
        return;
    }
    if (!cariFieldFisika(kelasManajer)) {
        LOGE("Field fisika pemain tidak lengkap, Teleport, Kecepatan, dan No Clip nonaktif");
        return;
    }
    LOGI("Field pemain OK");

    cariFieldDunia();

    kelasTile = cariKelasField("_Tile", "pixel_Size");
    if (kelasTile && cariField(kelasTile, "pixel_Size", fieldUkuranPixel))
        LOGI("Field ukuran tile siap");
    else
        LOGE("Field pixel_Size tidak ditemukan; skala awal teleport memakai collider");

    pasangHookGerak();

    void* kelasDeteksi = cariKelas("Physics__Detect");
    bool siapDinding = cariField(kelasDeteksi, "a", fieldDeteksiAktor) && cariFieldMargin();
    if (!siapDinding || !target(T_DETECT_SET).addr) {
        LOGE("Field tembus dinding tidak lengkap; No Clip nonaktif");
        return;
    }
    LOGI("Field tembus dinding siap");
    pasangHook(target(T_DETECT_SET), reinterpret_cast<void*>(hookDeteksiFisika));
}
