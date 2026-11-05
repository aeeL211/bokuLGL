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

// Batas yang dipakai No Clip. Nilainya jauh di luar ukuran dunia mana pun, jadi
// pemeriksaan tabrakan tidak pernah menyempitkannya lagi.
constexpr float kBatasBebas = 1.0e9f;
constexpr int32_t kBatasTeleport = 1024;

Field fieldAktorPemain = {};
Field fieldPemainLokal = {};
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

// Pos.tile dan Pos.world memakai ruang koordinat berbeda. Skala dan titik nol
// diturunkan dari perpindahan pemain sendiri, jadi tidak ada angka ajaib yang
// di-hardcode. Tiap sumbu dikalibrasi terpisah karena pemain jarang bergerak
// satu tile penuh di kedua sumbu pada saat yang sama.
struct Kalibrasi {
    bool siapX;
    bool siapY;
    int sampelX;
    int sampelY;
    float skalaX;
    float skalaY;
    float offsetX;
    float offsetY;
    bool adaAcuan;
    float acuanDuniaX;
    float acuanDuniaY;
    float acuanTileX;
    float acuanTileY;
};
Kalibrasi kalibrasi = {};

// Kecepatan dan gravitasi hanya mengubah Physics.speed milik pemain lokal.
// Nilai asli game tidak pernah ditimpa permanen: pada 1,0x nilai dasar dipasang
// kembali, dan setelah respawn hook membaca nilai baru dari game.
void* manajerTerakhir = nullptr;
void* fisikaTerakhir = nullptr;
float tulisXTerakhir = 0.0f;
float tulisYTerakhir = 0.0f;
float kaliTerakhir = 1.0f;
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

// Perpindahan antar-tile memberi pasangan (tile, dunia). Rasio keduanya adalah
// skala yang dipakai game, dan selisihnya adalah titik nol dunia. Sampel baru
// hanya dipakai setelah perpindahan minimal satu tile pada sumbu itu.
void tambahSampel(float& skala, float& offset, int& sampel, float deltaTile, float deltaDunia,
                  float tile, float dunia) {
    if (fabsf(deltaTile) < 0.999f)
        return;
    float kandidat = deltaDunia / deltaTile;
    if (!std::isfinite(kandidat) || kandidat < 0.05f || kandidat > 4096.0f)
        return;
    if (sampel == 0) {
        skala = kandidat;
        offset = dunia - tile * kandidat;
        sampel = 1;
        return;
    }
    skala = skala * 0.8f + kandidat * 0.2f;
    offset = offset * 0.8f + (dunia - tile * skala) * 0.2f;
    sampel++;
}

void perbaruiKalibrasi(const Vektor2& dunia, const Vektor2& tile) {
    // Tanpa gerakan, hanya posisi sekarang yang bisa dipakai. Rasio dunia/tile
    // masih berguna kalau titik nol dunia memang nol; nilai ini hanya tebakan
    // awal dan akan dikoreksi sampel perpindahan.
    if (!kalibrasi.siapX && fabsf(tile.x) > 1.0f) {
        float rasio = dunia.x / tile.x;
        if (std::isfinite(rasio) && rasio > 0.05f && rasio < 4096.0f) {
            kalibrasi.skalaX = rasio;
            kalibrasi.offsetX = dunia.x - tile.x * rasio;
            kalibrasi.sampelX = 0;
            kalibrasi.siapX = true;
            LOGI("Teleport: tebakan skala X %.2f dari posisi sekarang", rasio);
        }
    }
    if (!kalibrasi.siapY && fabsf(tile.y) > 1.0f) {
        float rasio = dunia.y / tile.y;
        if (std::isfinite(rasio) && rasio > 0.05f && rasio < 4096.0f) {
            kalibrasi.skalaY = rasio;
            kalibrasi.offsetY = dunia.y - tile.y * rasio;
            kalibrasi.sampelY = 0;
            kalibrasi.siapY = true;
            LOGI("Teleport: tebakan skala Y %.2f dari posisi sekarang", rasio);
        }
    }

    if (!kalibrasi.adaAcuan) {
        kalibrasi.acuanDuniaX = dunia.x;
        kalibrasi.acuanDuniaY = dunia.y;
        kalibrasi.acuanTileX = tile.x;
        kalibrasi.acuanTileY = tile.y;
        kalibrasi.adaAcuan = true;
        return;
    }

    float deltaTileX = tile.x - kalibrasi.acuanTileX;
    float deltaTileY = tile.y - kalibrasi.acuanTileY;
    bool ambilX = fabsf(deltaTileX) >= 0.999f;
    bool ambilY = fabsf(deltaTileY) >= 0.999f;
    if (!ambilX && !ambilY)
        return;

    int sampelXSebelum = kalibrasi.sampelX;
    int sampelYSebelum = kalibrasi.sampelY;
    if (ambilX) {
        tambahSampel(kalibrasi.skalaX, kalibrasi.offsetX, kalibrasi.sampelX, deltaTileX,
                     dunia.x - kalibrasi.acuanDuniaX, tile.x, dunia.x);
        kalibrasi.acuanDuniaX = dunia.x;
        kalibrasi.acuanTileX = tile.x;
    }
    if (ambilY) {
        tambahSampel(kalibrasi.skalaY, kalibrasi.offsetY, kalibrasi.sampelY, deltaTileY,
                     dunia.y - kalibrasi.acuanDuniaY, tile.y, dunia.y);
        kalibrasi.acuanDuniaY = dunia.y;
        kalibrasi.acuanTileY = tile.y;
    }
    kalibrasi.siapX = kalibrasi.siapX || kalibrasi.sampelX > 0;
    kalibrasi.siapY = kalibrasi.siapY || kalibrasi.sampelY > 0;
    if (kalibrasi.sampelX == 1 && sampelXSebelum == 0)
        LOGI("Teleport: kalibrasi X skala %.2f offset %.2f", kalibrasi.skalaX,
             kalibrasi.offsetX);
    if (kalibrasi.sampelY == 1 && sampelYSebelum == 0)
        LOGI("Teleport: kalibrasi Y skala %.2f offset %.2f", kalibrasi.skalaY,
             kalibrasi.offsetY);
}

Vektor2 keDunia(float tileX, float tileY) {
    // Skala biasanya sama di kedua sumbu, jadi sumbu yang belum punya sampel
    // meminjam skala sumbu lain. Titik nolnya tetap hanya dipakai bila sumbu
    // itu sendiri sudah terkalibrasi.
    float skalaCadangan = kalibrasi.siapX ? kalibrasi.skalaX
                                          : (kalibrasi.siapY ? kalibrasi.skalaY : 1.0f);
    Vektor2 hasil = {tileX * skalaCadangan, tileY * skalaCadangan};
    if (kalibrasi.siapX)
        hasil.x = kalibrasi.offsetX + tileX * kalibrasi.skalaX;
    if (kalibrasi.siapY)
        hasil.y = kalibrasi.offsetY + tileY * kalibrasi.skalaY;
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
    // Kalibrasi berikutnya harus mulai dari titik tujuan, bukan dari titik lama
    // yang jaraknya bisa ribuan tile.
    kalibrasi.adaAcuan = false;
    return true;
}

void kosongkanKecepatan(void* fisika) {
    void* kecepatan = bacaPtr(fisika, fieldKecepatan);
    if (!kecepatan)
        return;
    tulisFloat(kecepatan, fieldKecepatanX, 0.0f);
    tulisFloat(kecepatan, fieldKecepatanY, 0.0f);
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
             tile.x, tile.y, dunia.x, dunia.y, tile.x - verifikasi.tileX,
             tile.y - verifikasi.tileY, dunia.x - verifikasi.dunia.x,
             dunia.y - verifikasi.dunia.y);
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
        LOGE("Teleport ditolak: tile %d, %d di luar batas -%d..%d", tileX, tileY,
             kBatasTeleport, kBatasTeleport);
        return;
    }

    Vektor2 duniaLama = {};
    Vektor2 tileLama = {};
    bool adaDunia = bacaVektor(posisi, fieldDunia, duniaLama);
    bool adaTile = bacaTile(posisi, tileLama);
    if (adaDunia && adaTile) {
        LOGI("Teleport: sebelum tile=(%.2f, %.2f) dunia=(%.1f, %.1f)", tileLama.x, tileLama.y,
             duniaLama.x, duniaLama.y);
    } else if (adaDunia) {
        LOGI("Teleport: sebelum dunia=(%.1f, %.1f), tile tidak terbaca", duniaLama.x,
             duniaLama.y);
    }

    Vektor2 duniaBaru = keDunia(static_cast<float>(tileX), static_cast<float>(tileY));
    if (!std::isfinite(duniaBaru.x) || !std::isfinite(duniaBaru.y)) {
        LOGE("Teleport ditolak: hasil konversi bukan angka");
        return;
    }
    if (!kalibrasi.siapX || !kalibrasi.siapY) {
        LOGI("Teleport: kalibrasi belum lengkap, sumbu yang kosong memakai satuan tile apa adanya");
    }

    LOGI("Teleport: target tile=(%d, %d) dunia=(%.1f, %.1f) skala=(%.2f, %.2f) offset=(%.2f, "
         "%.2f)",
         tileX, tileY, duniaBaru.x, duniaBaru.y, kalibrasi.skalaX, kalibrasi.skalaY,
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

bool miripNilai(float kiri, float kanan) {
    return fabsf(kiri - kanan) <= 0.001f * (1.0f + fabsf(kanan));
}

// Physics.speed adalah kecepatan yang dipakai game untuk langkah berikutnya.
// Sumbu X diskalakan penuh, sedangkan sumbu Y hanya menerima tambahan
// gravitasi. Gravitasi game menumpuk di speed.y tiap frame, jadi selisih
// terhadap nilai yang kita tulis frame lalu adalah tambahan satu frame itu;
// tanpa pembanding itu pengali akan menumpuk sendiri.
void terapkanKecepatanGravitasi(void* manajer) {
    void* fisika = bacaPtr(manajer, fieldFisika);
    if (!fisika)
        return;
    void* kecepatan = bacaPtr(fisika, fieldKecepatan);
    if (!kecepatan)
        return;

    float mentahX = bacaFloat(kecepatan, fieldKecepatanX);
    float mentahY = bacaFloat(kecepatan, fieldKecepatanY);
    if (!std::isfinite(mentahX) || !std::isfinite(mentahY))
        return;

    bool lanjut = manajer == manajerTerakhir && fisika == fisikaTerakhir;
    float kelipatan = ambilKelipatan(fitur().kecepatanPemain.load(), 0.5f, 5.0f);
    float gravitasi = ambilKelipatan(fitur().gravitasiPemain.load(), 0.0f, 3.0f);

    // Bila game tidak menyentuh speed.x, nilainya masih hasil kali frame lalu.
    // Bagi dulu supaya pengali tidak menumpuk, lalu kalikan dengan nilai baru.
    float dasarX = mentahX;
    if (lanjut && kaliTerakhir != 1.0f && miripNilai(mentahX, tulisXTerakhir))
        dasarX = mentahX / kaliTerakhir;
    if (kelipatan != 1.0f)
        dasarX *= kelipatan;

    // Hanya jatuh yang dipercepat. Lompatan (speed.y positif) dan pendaratan
    // (speed.y kembali nol) dibiarkan apa adanya.
    float dasarY = mentahY;
    if (lanjut && gravitasi != 1.0f) {
        float delta = mentahY - tulisYTerakhir;
        if (tulisYTerakhir <= 0.0f && mentahY < 0.0f && delta < -0.0001f && delta > -20.0f)
            dasarY += delta * (gravitasi - 1.0f);
    }

    if (!std::isfinite(dasarX) || !std::isfinite(dasarY))
        return;
    if (dasarX > 10000.0f)
        dasarX = 10000.0f;
    if (dasarX < -10000.0f)
        dasarX = -10000.0f;
    if (dasarY > 10000.0f)
        dasarY = 10000.0f;
    if (dasarY < -10000.0f)
        dasarY = -10000.0f;

    tulisFloat(kecepatan, fieldKecepatanX, dasarX);
    tulisFloat(kecepatan, fieldKecepatanY, dasarY);
    tulisXTerakhir = dasarX;
    tulisYTerakhir = dasarY;
    kaliTerakhir = kelipatan;
    manajerTerakhir = manajer;
    fisikaTerakhir = fisika;

    int nilaiKecepatan = fitur().kecepatanPemain.load();
    if (nilaiKecepatan != logKecepatan) {
        logKecepatan = nilaiKecepatan;
        LOGI("Kecepatan pemain: %.1fx", kelipatan);
    }
    int nilaiGravitasi = fitur().gravitasiPemain.load();
    if (nilaiGravitasi != logGravitasi) {
        logGravitasi = nilaiGravitasi;
        LOGI("Gravitasi pemain: %.1fx", gravitasi);
    }
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
    return cariField(kelasKecepatan, "x", fieldKecepatanX) &&
           cariField(kelasKecepatan, "y", fieldKecepatanY);
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
        if (adaDunia && adaTile)
            perbaruiKalibrasi(dunia, tile);
        verifikasiTeleport(posisi);
    }

    terapkanTeleport(manajer);
    terapkanKecepatanGravitasi(manajer);
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

void pasangHookPemain() {
    void* kelasManajer = kelasDari("Player__Manager");
    if (!cariField(kelasManajer, "a", fieldAktorPemain) ||
        !cariField(kelasField(fieldAktorPemain), "player", fieldPemainLokal)) {
        LOGE("Field pemain tidak lengkap, Teleport dan No Clip nonaktif");
        return;
    }
    if (!cariFieldFisika(kelasManajer)) {
        LOGE("Field fisika pemain tidak lengkap, Teleport dan No Clip nonaktif");
        return;
    }
    LOGI("Field pemain OK");

    void* kelasDeteksi = cariKelas("Physics__Detect");
    bool siapDinding = cariField(kelasDeteksi, "a", fieldDeteksiAktor) && cariFieldMargin();
    if (!siapDinding || !target(T_DETECT_SET).addr) {
        LOGE("Field tembus dinding tidak lengkap; No Clip nonaktif");
        return;
    }
    LOGI("Field tembus dinding siap");
    pasangHook(target(T_DETECT_SET), reinterpret_cast<void*>(hookDeteksiFisika));
}
