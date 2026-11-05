#include "hooks.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hook.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// String model berisi beberapa ratus field yang dipisah. Batas ini hanya
// menghentikan panjang yang rusak agar tidak berjalan melewati string.
constexpr int32_t kBatasKarakterModel = 16384;
// Satu entri per skin berbeda yang bisa tampil dalam satu sesi; pengulangan hanya
// memakan satu pencarian tabel.
constexpr int kBatasHash = 512;
constexpr int kUkuranTabelHash = 1024;
constexpr int kTopengTabelHash = kUkuranTabelHash - 1;
constexpr int kBatasSlot = 1000;
constexpr size_t kBatasBytePath = 1024;
// Kunci tamu itu pendek; yang lebih panjang tidak layak jadi entri cache.
constexpr size_t kBatasBytePengguna = 64;
// Satu entri per tamu yang modelnya bisa disalin dari menu.
constexpr int kMaksTamu = 16;
// Berkas dibaca per potongan, jadi tidak ada buffer sebesar string model.
constexpr size_t kBatasBytePotongan = 1024;
constexpr uint64_t kAwalHash = 1469598103934665603ull;
constexpr uint64_t kPrimaHash = 1099511628211ull;

char teksPath[kBatasBytePath];

// Hash model yang sudah punya slot, beserta nomor slotnya, supaya permintaan
// salin berikutnya bisa menyebut tempat skin itu sudah tersimpan.
struct SlotModel {
    uint64_t hash;
    int slot;
};

SlotModel tabelHash[kUkuranTabelHash];
int jumlahHashModel = 0;
int slotBerikut = 1;
bool slotSiap = false;

// Model tamu disimpan di memori saat mereka masuk room. Penulisan ke slot baru
// terjadi ketika pemain memintanya dari menu, jadi tidak ada skin yang masuk
// tanpa persetujuan pemakai.
struct ModelTamu {
    char kunci[kBatasBytePengguna];
    char* model;
};

ModelTamu daftarTamu[kMaksTamu];
int jumlahTamu = 0;
int tamuBerikut = 0;

// Hook masuk tamu dan hook update tamu bisa berjalan di thread yang berbeda,
// jadi cache-nya dijaga kunci. Pembaca menyalin modelnya lalu melepas kunci,
// supaya entri yang diganti tidak terbaca setengah jadi.
pthread_mutex_t kunciTamu = PTHREAD_MUTEX_INITIALIZER;

// Model milik satu tamu, disalin dari cache supaya entrinya boleh diganti
// setelah fungsi pengambil selesai.
struct SalinanModel {
    char* teks = nullptr;
    ~SalinanModel() { free(teks); }
};

Field fieldTamu = {};
Field fieldDataManajer = {};
Field fieldModel = {};
Field fieldNamaBerkas = {};
bool fieldSiap = false;
bool sudahCatatTidakSiap = false;

uint64_t hashTeks(const char* teks, size_t panjang, uint64_t awal = kAwalHash) {
    uint64_t hasil = awal;
    for (size_t i = 0; i < panjang; i++) {
        hasil ^= static_cast<uint8_t>(teks[i]);
        hasil *= kPrimaHash;
    }
    return hasil;
}

// Tabel terbuka dengan indeks dari hash itu sendiri, jadi skin yang sudah ada
// ditemukan tanpa memindai daftar. Hash nol dipindahkan ke satu supaya tidak
// pernah terbaca sebagai slot kosong.
int cariSlot(uint64_t nilai) {
    if (!nilai)
        nilai = 1;
    int i = static_cast<int>(nilai & kTopengTabelHash);
    while (tabelHash[i].hash != 0) {
        if (tabelHash[i].hash == nilai)
            return tabelHash[i].slot;
        i = (i + 1) & kTopengTabelHash;
    }
    return 0;
}

void catatSlot(uint64_t nilai, int slot) {
    if (jumlahHashModel >= kBatasHash)
        return;
    if (!nilai)
        nilai = 1;
    int i = static_cast<int>(nilai & kTopengTabelHash);
    while (tabelHash[i].hash != 0 && tabelHash[i].hash != nilai)
        i = (i + 1) & kTopengTabelHash;
    if (tabelHash[i].hash == nilai)
        return;
    tabelHash[i].hash = nilai;
    tabelHash[i].slot = slot;
    jumlahHashModel++;
}

void lepasModelTamu(ModelTamu& entri) {
    free(entri.model);
    entri.model = nullptr;
    entri.kunci[0] = '\0';
}

// Slot dipakai ulang dari awal saat daftar penuh, jadi tamu yang paling lama
// tidak lagi bisa disalin tanpa keluar dan masuk lagi.
ModelTamu& slotTamu(const char* kunci) {
    for (int i = 0; i < jumlahTamu; i++) {
        if (!strcmp(daftarTamu[i].kunci, kunci))
            return daftarTamu[i];
    }
    int indeks = jumlahTamu;
    if (jumlahTamu < kMaksTamu) {
        jumlahTamu++;
    } else {
        indeks = tamuBerikut;
        tamuBerikut = (tamuBerikut + 1) % kMaksTamu;
        lepasModelTamu(daftarTamu[indeks]);
    }
    snprintf(daftarTamu[indeks].kunci, sizeof(daftarTamu[indeks].kunci), "%s", kunci);
    return daftarTamu[indeks];
}

// Folder model dimiliki Manager yang sama dengan pemegang daftar tamu, jadi
// penulis milik game sendiri menaruh berkasnya di Save/Model tanpa menebak path.
// Nama berkasnya ada di pemegang yang belum dimiliki penyimpanan setengah jadi,
// dan pembangun path melakukan dereference ke sana, jadi penyimpanan seperti itu
// dilaporkan tidak ada.
void* modelData(void* manajerTamu) {
    if (!fieldSiap)
        return nullptr;
    void* manajer = bacaPtr(manajerTamu, fieldTamu);
    void* data = bacaPtr(manajer, fieldDataManajer);
    void* model = bacaPtr(data, fieldModel);
    return bacaPtr(model, fieldNamaBerkas) ? model : nullptr;
}

// Path milik game sendiri untuk sebuah slot model, jadi tata letaknya tidak
// pernah ditebak di sini.
bool pathSlot(void* model, int slot) {
    auto panggil =
        reinterpret_cast<void* (*)(void*, int32_t, void*)>(target(T_DATA_MODEL_PATH).addr);
    return panggil && keUtf8(panggil(model, slot, target(T_DATA_MODEL_PATH).method), teksPath,
                             sizeof(teksPath));
}

bool slotAda(void* model, int slot) { return pathSlot(model, slot) && access(teksPath, F_OK) == 0; }

void tulisSlot(void* model, int slot, void* text) {
    auto panggil =
        reinterpret_cast<void (*)(void*, int32_t, void*, void*)>(target(T_DATA_MODEL_WRITE).addr);
    if (panggil)
        panggil(model, slot, text, target(T_DATA_MODEL_WRITE).method);
}

// Game menulis nama tampilan modelnya sendiri dengan cara yang sama, jadi slot
// mendapat Name.txt biasa di samping Data.txt.
void tulisNamaSlot(void* model, int slot, void* text) {
    auto panggil =
        reinterpret_cast<void (*)(void*, int32_t, void*, void*)>(target(T_DATA_MODEL_NAME).addr);
    if (panggil)
        panggil(model, slot, text, target(T_DATA_MODEL_NAME).method);
}

// Skin dari sesi sebelumnya sudah memiliki slotnya, jadi hash setiap Data.txt di
// disk dibaca sekali dan penulisan berikutnya melanjutkan setelahnya. Isinya
// dibaca per potongan supaya tidak perlu buffer sebesar string model.
bool bacaSlot(void* model, int slot) {
    if (!pathSlot(model, slot))
        return false;
    int fd = open(teksPath, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    char potongan[kBatasBytePotongan];
    uint64_t nilai = kAwalHash;
    bool adaIsi = false;
    ssize_t n = 0;
    while ((n = read(fd, potongan, sizeof(potongan))) > 0) {
        nilai = hashTeks(potongan, static_cast<size_t>(n), nilai);
        adaIsi = true;
    }
    close(fd);
    if (adaIsi)
        catatSlot(nilai, slot);
    return true;
}

void isiSlotAwal(void* model) {
    int slot = 1;
    while (slot <= kBatasSlot && bacaSlot(model, slot))
        slot++;
    slotBerikut = slot;
    LOGI("Slot skin: %d tersimpan, berikutnya %d", jumlahHashModel, slotBerikut);
}

// Model tamu disalin ke memori, bukan ke disk, karena menu memutuskan kapan
// skin itu disimpan. Kuncinya adalah id tamu yang juga dipakai daftar pemain.
void simpanModelTamu(void* kunci, void* model) {
    char teksKunci[kBatasBytePengguna];
    if (!kunci || !model || !keUtf8(kunci, teksKunci, sizeof(teksKunci)) || !teksKunci[0])
        return;
    int32_t panjang = panjangString(model);
    if (panjang <= 0 || panjang > kBatasKarakterModel)
        return;

    // Satu unit UTF-16 menjadi paling banyak tiga byte UTF-8, dan penulis UTF-8
    // menyisakan empat byte di ujung; ukuran ini pas untuk seluruh isi string.
    size_t ukuran = static_cast<size_t>(panjang) * 3 + 4;
    auto* teks = static_cast<char*>(malloc(ukuran));
    if (!teks)
        return;
    if (!keUtf8(model, teks, ukuran)) {
        free(teks);
        return;
    }

    pthread_mutex_lock(&kunciTamu);
    ModelTamu& entri = slotTamu(teksKunci);
    free(entri.model);
    entri.model = teks;
    pthread_mutex_unlock(&kunciTamu);
    LOGI("Model tamu dicatat: %s (%d karakter)", teksKunci, panjang);
}

bool ambilModelTamu(const char* kunci, SalinanModel& hasil) {
    pthread_mutex_lock(&kunciTamu);
    for (int i = 0; i < jumlahTamu; i++) {
        if (strcmp(daftarTamu[i].kunci, kunci) || !daftarTamu[i].model)
            continue;
        hasil.teks = strdup(daftarTamu[i].model);
        break;
    }
    pthread_mutex_unlock(&kunciTamu);
    return hasil.teks != nullptr;
}

} // namespace

HasilSalin salinModelTamu(void* manajerTamu, const char* kunci, const char* nama, int& slot) {
    slot = 0;
    if (!fieldSiap || !manajerTamu || !kunci || !*kunci)
        return HasilSalin::Gagal;

    SalinanModel tersalin;
    if (!ambilModelTamu(kunci, tersalin) || !*tersalin.teks)
        return HasilSalin::TidakAda;
    const char* modelTeks = tersalin.teks;

    void* model = modelData(manajerTamu);
    if (!model) {
        if (!sudahCatatTidakSiap) {
            sudahCatatTidakSiap = true;
            LOGE("Model skin belum siap, salin skin dilewati");
        }
        return HasilSalin::Gagal;
    }

    if (!slotSiap) {
        slotSiap = true;
        isiSlotAwal(model);
    }

    uint64_t nilai = hashTeks(modelTeks, strlen(modelTeks));
    int tersimpan = cariSlot(nilai);
    if (tersimpan) {
        slot = tersimpan;
        return HasilSalin::SudahAda;
    }
    if (slotBerikut > kBatasSlot) {
        LOGE("Slot skin penuh, %d skin tersimpan", jumlahHashModel);
        return HasilSalin::Gagal;
    }

    void* teksModel = stringDariUtf8(modelTeks);
    if (!teksModel) {
        LOGE("String model gagal dibuat untuk slot %d", slotBerikut);
        return HasilSalin::Gagal;
    }
    tulisSlot(model, slotBerikut, teksModel);
    if (!slotAda(model, slotBerikut)) {
        LOGE("Penulisan skin gagal: slot %d", slotBerikut);
        return HasilSalin::Gagal;
    }

    slot = slotBerikut++;
    catatSlot(nilai, slot);
    LOGI("Skin tamu disimpan: slot %d (%zu byte)", slot, strlen(modelTeks));

    if (nama && *nama) {
        void* teksNama = stringDariUtf8(nama);
        if (teksNama) {
            tulisNamaSlot(model, slot, teksNama);
            LOGI("Nama skin: slot %d", slot);
        }
    }
    return HasilSalin::Tersimpan;
}

namespace {

using FnTambah = void (*)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*,
                          void*, void*, void*, void*, bool, bool, bool, bool, bool, bool, void*);

// Daftar tamu dibangun dari satu panggilan ini di host maupun client, dan
// argumen Model-nya adalah string skin penuh. Model itu hanya disimpan di
// memori sampai pemain memintanya lewat menu.
void hookTambah(void* self, void* user, int32_t z, int32_t x, int32_t y, int32_t dir, int32_t s,
                int32_t obj, void* key, void* dollName, void* parent, void* model, void* ping,
                bool build, bool detonate, bool saveWorld, bool dollInteract, bool dollMove,
                bool dollSetting, void* method) {
    simpanModelTamu(key, model);

    auto asli = reinterpret_cast<FnTambah>(target(T_GUEST_ADD).orig);
    if (asli)
        asli(self, user, z, x, y, dir, s, obj, key, dollName, parent, model, ping, build, detonate,
             saveWorld, dollInteract, dollMove, dollSetting, method);
}

} // namespace

void pasangHookSkin() {
    void* kelasTamu = kelasDari("Guest__Manager");
    fieldSiap = cariField(kelasTamu, "a", fieldTamu);
    void* kelasManajer = fieldSiap ? kelasField(fieldTamu) : nullptr;
    fieldSiap = fieldSiap && cariField(kelasManajer, "data", fieldDataManajer);
    void* kelasData = fieldSiap ? kelasField(fieldDataManajer) : nullptr;
    fieldSiap = fieldSiap && cariField(kelasData, "model", fieldModel);
    void* kelasModel = fieldSiap ? kelasField(fieldModel) : nullptr;
    fieldSiap = fieldSiap && cariField(kelasModel, "_filename", fieldNamaBerkas);
    LOGI("Field model skin %s", fieldSiap ? "OK" : "tidak ada, salin skin tidak aktif");

    pasangHook(target(T_GUEST_ADD), reinterpret_cast<void*>(hookTambah));
}
