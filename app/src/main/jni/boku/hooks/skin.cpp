#include "hooks.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// String model berisi beberapa ratus field yang dipisah. Batas ini hanya
// menghentikan panjang yang rusak agar tidak berjalan melewati string.
constexpr int32_t kLimitCharacterModel = 16384;
// Satu entri per skin berbeda yang bisa tampil dalam satu sesi; pengulangan hanya
// memakan satu pencarian tabel.
constexpr int kLimitHash = 512;
constexpr int kSizeTableHash = 1024;
constexpr int kMaskTableHash = kSizeTableHash - 1;
constexpr int kLimitSlot = 1000;
constexpr size_t kLimitBytePath = 1024;
// Kunci tamu itu pendek; yang lebih panjang tidak layak jadi entri cache.
constexpr size_t kLimitByteUser = 64;
// Satu entri per tamu yang modelnya bisa disalin dari menu.
constexpr int kMaxGuest = 16;
// Berkas dibaca per potongan, jadi tidak ada buffer sebesar string model.
constexpr size_t kLimitBytePiece = 1024;
constexpr uint64_t kInitialHash = 1469598103934665603ull;
constexpr uint64_t kPrimaHash = 1099511628211ull;

char textPath[kLimitBytePath];

// Hash model yang sudah punya slot, beserta nomor slotnya, supaya permintaan
// salin berikutnya bisa menyebut tempat skin itu sudah tersimpan.
struct SlotModel {
    uint64_t hash;
    int slot;
};

SlotModel tableHash[kSizeTableHash];
int countHashModel = 0;
int slotNext = 1;
bool slotReady = false;

// Model tamu disimpan di memori saat mereka masuk room. Penulisan ke slot baru
// terjadi ketika pemain memintanya dari menu, jadi tidak ada skin yang masuk
// tanpa persetujuan pemakai.
struct ModelGuest {
    char key[kLimitByteUser];
    char* model;
};

ModelGuest guestList[kMaxGuest];
int countGuest = 0;
int guestNext = 0;

// Hook masuk tamu dan hook update tamu bisa berjalan di thread yang berbeda,
// jadi cache-nya dijaga kunci. Pembaca menyalin modelnya lalu melepas kunci,
// supaya entri yang diganti tidak terbaca setengah jadi.
pthread_mutex_t guestKey = PTHREAD_MUTEX_INITIALIZER;

// Model milik satu tamu, disalin dari cache supaya entrinya boleh diganti
// setelah fungsi pengambil selesai.
struct SalinanModel {
    char* text = nullptr;
    ~SalinanModel() { free(text); }
};

Field fieldGuest = {};
Field fieldDataManager = {};
Field fieldModel = {};
Field fieldNameFile = {};
Field guestListField = {};
Field fieldItemGuest = {};
Field guestSizeField = {};
Field guestKeyField = {};
Field fieldModelGuest = {};
bool fieldReady = false;
bool fieldGuestReady = false;
bool alreadyRecordNotReady = false;

uint64_t hashText(const char* text, size_t length, uint64_t initial = kInitialHash) {
    uint64_t result = initial;
    for (size_t i = 0; i < length; i++) {
        result ^= static_cast<uint8_t>(text[i]);
        result *= kPrimaHash;
    }
    return result;
}

// Tabel terbuka dengan indeks dari hash itu sendiri, jadi skin yang sudah ada
// ditemukan tanpa memindai daftar. Hash nol dipindahkan ke satu supaya tidak
// pernah terbaca sebagai slot kosong.
int findSlot(uint64_t value) {
    if (!value)
        value = 1;
    int i = static_cast<int>(value & kMaskTableHash);
    while (tableHash[i].hash != 0) {
        if (tableHash[i].hash == value)
            return tableHash[i].slot;
        i = (i + 1) & kMaskTableHash;
    }
    return 0;
}

void recordSlot(uint64_t value, int slot) {
    if (countHashModel >= kLimitHash)
        return;
    if (!value)
        value = 1;
    int i = static_cast<int>(value & kMaskTableHash);
    while (tableHash[i].hash != 0 && tableHash[i].hash != value)
        i = (i + 1) & kMaskTableHash;
    if (tableHash[i].hash == value)
        return;
    tableHash[i].hash = value;
    tableHash[i].slot = slot;
    countHashModel++;
}

void releaseModelGuest(ModelGuest& entry) {
    free(entry.model);
    entry.model = nullptr;
    entry.key[0] = '\0';
}

// Slot dipakai ulang dari awal saat daftar penuh, jadi tamu yang paling lama
// tidak lagi bisa disalin tanpa keluar dan masuk lagi.
ModelGuest& slotGuest(const char* key) {
    for (int i = 0; i < countGuest; i++) {
        if (!strcmp(guestList[i].key, key))
            return guestList[i];
    }
    int indeks = countGuest;
    if (countGuest < kMaxGuest) {
        countGuest++;
    } else {
        indeks = guestNext;
        guestNext = (guestNext + 1) % kMaxGuest;
        releaseModelGuest(guestList[indeks]);
    }
    snprintf(guestList[indeks].key, sizeof(guestList[indeks].key), "%s", key);
    return guestList[indeks];
}

// Satu unit UTF-16 menjadi paling banyak tiga byte UTF-8, dan penulis UTF-8
// menyisakan empat byte di ujung; ukuran ini pas untuk seluruh isi string.
char* copyText(void* text) {
    int32_t length = lengthString(text);
    if (length <= 0 || length > kLimitCharacterModel)
        return nullptr;
    size_t size = static_cast<size_t>(length) * 3 + 4;
    auto* result = static_cast<char*>(malloc(size));
    if (!result)
        return nullptr;
    if (!toUtf8(text, result, size)) {
        free(result);
        return nullptr;
    }
    return result;
}

// Folder model dimiliki Manager yang sama dengan pemegang daftar tamu, jadi
// penulis milik game sendiri menaruh berkasnya di Save/Model tanpa menebak path.
// Nama berkasnya ada di pemegang yang belum dimiliki penyimpanan setengah jadi,
// dan pembangun path melakukan dereference ke sana, jadi penyimpanan seperti itu
// dilaporkan tidak ada.
void* modelData(void* managerGuest) {
    if (!fieldReady)
        return nullptr;
    void* manager = readPtr(managerGuest, fieldGuest);
    void* data = readPtr(manager, fieldDataManager);
    void* model = readPtr(data, fieldModel);
    return readPtr(model, fieldNameFile) ? model : nullptr;
}

// Path milik game sendiri untuk sebuah slot model, jadi tata letaknya tidak
// pernah ditebak di sini.
bool pathSlot(void* model, int slot) {
    auto call =
        reinterpret_cast<void* (*)(void*, int32_t, void*)>(target(T_DATA_MODEL_PATH).addr);
    return call && toUtf8(call(model, slot, target(T_DATA_MODEL_PATH).method), textPath,
                             sizeof(textPath));
}

bool slotHas(void* model, int slot) { return pathSlot(model, slot) && access(textPath, F_OK) == 0; }

void writeSlot(void* model, int slot, void* text) {
    auto call =
        reinterpret_cast<void (*)(void*, int32_t, void*, void*)>(target(T_DATA_MODEL_WRITE).addr);
    if (call)
        call(model, slot, text, target(T_DATA_MODEL_WRITE).method);
}

// Game menulis nama tampilan modelnya sendiri dengan cara yang sama, jadi slot
// mendapat Name.txt biasa di samping Data.txt.
void writeNameSlot(void* model, int slot, void* text) {
    auto call =
        reinterpret_cast<void (*)(void*, int32_t, void*, void*)>(target(T_DATA_MODEL_NAME).addr);
    if (call)
        call(model, slot, text, target(T_DATA_MODEL_NAME).method);
}

// Skin dari sesi sebelumnya sudah memiliki slotnya, jadi hash setiap Data.txt di
// disk dibaca sekali dan penulisan berikutnya melanjutkan setelahnya. Isinya
// dibaca per potongan supaya tidak perlu buffer sebesar string model.
bool readSlot(void* model, int slot) {
    if (!pathSlot(model, slot))
        return false;
    int fd = open(textPath, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    char piece[kLimitBytePiece];
    uint64_t value = kInitialHash;
    bool hasContent = false;
    ssize_t n = 0;
    while ((n = read(fd, piece, sizeof(piece))) > 0) {
        value = hashText(piece, static_cast<size_t>(n), value);
        hasContent = true;
    }
    close(fd);
    if (hasContent)
        recordSlot(value, slot);
    return true;
}

void fillInitialSlot(void* model) {
    int slot = 1;
    while (slot <= kLimitSlot && readSlot(model, slot))
        slot++;
    slotNext = slot;
    LOGI("Slot skin: %d tersimpan, berikutnya %d", countHashModel, slotNext);
}

// Model tamu disalin ke memori, bukan ke disk, karena menu memutuskan kapan
// skin itu disimpan. Kuncinya adalah id tamu yang juga dipakai daftar pemain.
void saveModelGuest(void* key, void* model) {
    char keyText[kLimitByteUser];
    if (!key || !model || !toUtf8(key, keyText, sizeof(keyText)) || !keyText[0])
        return;
    char* text = copyText(model);
    if (!text)
        return;

    pthread_mutex_lock(&guestKey);
    ModelGuest& entry = slotGuest(keyText);
    free(entry.model);
    entry.model = text;
    pthread_mutex_unlock(&guestKey);
    LOGI("Model tamu dicatat: %s (%d karakter)", keyText, lengthString(model));
}

bool getGuestModel(const char* key, SalinanModel& result) {
    pthread_mutex_lock(&guestKey);
    for (int i = 0; i < countGuest; i++) {
        if (strcmp(guestList[i].key, key) || !guestList[i].model)
            continue;
        result.text = strdup(guestList[i].model);
        break;
    }
    pthread_mutex_unlock(&guestKey);
    return result.text != nullptr;
}

// Tamu yang sudah ada di room sebelum mod dimuat tidak pernah melewati hook
// Add. Model yang sedang dipakai bisa dibaca dari daftar hidup, lalu diubah
// kembali menjadi string oleh Model__To__Str milik game sendiri.
bool guestModelAlive(void* managerGuest, const char* key, SalinanModel& result) {
    if (!fieldGuestReady)
        return false;
    void* manager = readPtr(managerGuest, fieldGuest);
    void* list = readPtr(manager, guestListField);
    void* item = readPtr(list, fieldItemGuest);
    int32_t count = item ? readInt(list, guestSizeField) : 0;
    int32_t kapasitas = item ? lengthArray(item) : 0;
    if (count <= 0 || count > kapasitas)
        return false;

    auto toText = reinterpret_cast<void* (*)(void*, void*)>(target(T_MODEL_TO_STR).addr);
    if (!toText) {
        LOGE("Target _Model.Model__To__Str tidak ditemukan, model tamu hidup tidak terbaca");
        return false;
    }

    char* data = arrayData(item);
    char keyText[kLimitByteUser];
    for (int32_t i = 0; i < count; i++) {
        void* guest = *reinterpret_cast<void**>(data + static_cast<size_t>(i) * sizeof(void*));
        if (!guest || !toUtf8(readPtr(guest, guestKeyField), keyText, sizeof(keyText)))
            continue;
        if (strcmp(keyText, key))
            continue;
        void* model = readPtr(guest, fieldModelGuest);
        if (!model)
            return false;
        result.text = copyText(toText(model, target(T_MODEL_TO_STR).method));
        return result.text != nullptr;
    }
    return false;
}

// Field daftar tamu hidup dipakai saat cache Add kosong. Namanya sama dengan
// yang dipakai section Room, jadi keduanya menunjuk daftar yang sama.
bool findFieldGuestAlive() {
    if (!fieldGuest.field)
        return false;
    void* managerClass = fieldClass(fieldGuest);
    if (!findField(managerClass, "guests", guestListField))
        return false;
    void* klassList = fieldClass(guestListField);
    if (!findField(klassList, "_items", fieldItemGuest) ||
        !findField(klassList, "_size", guestSizeField))
        return false;
    void* klassGuest = fieldElementClass(fieldItemGuest);
    return findField(klassGuest, "key", guestKeyField) &&
           findField(klassGuest, "model", fieldModelGuest);
}

} // namespace

CopyResult copyGuestModel(void* managerGuest, const char* key, const char* name, int& slot) {
    slot = 0;
    if (!fieldReady || !managerGuest || !key || !*key)
        return CopyResult::Failed;

    SalinanModel tersalin;
    if (!getGuestModel(key, tersalin) || !tersalin.text || !*tersalin.text) {
        free(tersalin.text);
        tersalin.text = nullptr;
        if (!guestModelAlive(managerGuest, key, tersalin) || !tersalin.text) {
            LOGI("Model tamu tidak ditemukan: %s", key);
            return CopyResult::Missing;
        }
        LOGI("Model tamu dibaca dari daftar: %s", key);
    }
    const char* modelText = tersalin.text;

    void* model = modelData(managerGuest);
    if (!model) {
        if (!alreadyRecordNotReady) {
            alreadyRecordNotReady = true;
            LOGE("Model skin belum siap, salin skin dilewati");
        }
        return CopyResult::Failed;
    }

    if (!slotReady) {
        slotReady = true;
        fillInitialSlot(model);
    }

    uint64_t value = hashText(modelText, strlen(modelText));
    int saved = findSlot(value);
    if (saved) {
        slot = saved;
        return CopyResult::Exists;
    }
    if (slotNext > kLimitSlot) {
        LOGE("Slot skin penuh, %d skin tersimpan", countHashModel);
        return CopyResult::Failed;
    }

    void* textModel = stringFromUtf8(modelText);
    if (!textModel) {
        LOGE("String model gagal dibuat untuk slot %d", slotNext);
        return CopyResult::Failed;
    }
    writeSlot(model, slotNext, textModel);
    if (!slotHas(model, slotNext)) {
        LOGE("Penulisan skin gagal: slot %d", slotNext);
        return CopyResult::Failed;
    }

    slot = slotNext++;
    recordSlot(value, slot);
    LOGI("Skin tamu disimpan: slot %d (%zu byte)", slot, strlen(modelText));

    if (name && *name) {
        void* nameText = stringFromUtf8(name);
        if (nameText) {
            writeNameSlot(model, slot, nameText);
            LOGI("Nama skin: slot %d", slot);
        }
    }
    return CopyResult::Saved;
}

namespace {

using FnAdd = void (*)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*,
                          void*, void*, void*, void*, bool, bool, bool, bool, bool, bool, void*);
using FnInitModel = void (*)(void*, void*, void*);

// Daftar tamu dibangun dari satu panggilan ini di host maupun client, dan
// argumen Model-nya adalah string skin penuh. Model itu hanya disimpan di
// memori sampai pemain memintanya lewat menu.
void hookAdd(void* self, void* user, int32_t z, int32_t x, int32_t y, int32_t dir, int32_t s,
                int32_t obj, void* key, void* dollName, void* parent, void* model, void* ping,
                bool build, bool detonate, bool saveWorld, bool dollInteract, bool dollMove,
                bool dollSetting, void* method) {
    saveModelGuest(key, model);

    auto original = reinterpret_cast<FnAdd>(target(T_GUEST_ADD).orig);
    if (original)
        original(self, user, z, x, y, dir, s, obj, key, dollName, parent, model, ping, build, detonate,
             saveWorld, dollInteract, dollMove, dollSetting, method);
}

// Model bisa tiba lewat Init__Model setelah tamu masuk, jadi string yang
// diterima di sini juga dicatat. Kuncinya sudah ada di tamu itu sendiri.
void hookInitModel(void* own, void* model, void* method) {
    saveModelGuest(readPtr(own, guestKeyField), model);
    auto original = reinterpret_cast<FnInitModel>(target(T_GUEST_INIT_MODEL).orig);
    if (original)
        original(own, model, method);
}

} // namespace

void installSkinHook() {
    void* klassGuest = classOf("Guest__Manager");
    fieldReady = findField(klassGuest, "a", fieldGuest);
    void* managerClass = fieldReady ? fieldClass(fieldGuest) : nullptr;
    fieldReady = fieldReady && findField(managerClass, "data", fieldDataManager);
    void* klassData = fieldReady ? fieldClass(fieldDataManager) : nullptr;
    fieldReady = fieldReady && findField(klassData, "model", fieldModel);
    void* klassModel = fieldReady ? fieldClass(fieldModel) : nullptr;
    fieldReady = fieldReady && findField(klassModel, "_filename", fieldNameFile);
    LOGI("Field model skin %s", fieldReady ? "OK" : "tidak ada, salin skin tidak aktif");

    fieldGuestReady = findFieldGuestAlive();
    LOGI("Field daftar tamu %s", fieldGuestReady ? "OK" : "tidak ada, salin skin hanya dari cache");

    installHook(target(T_GUEST_ADD), reinterpret_cast<void*>(hookAdd));
    installHook(target(T_GUEST_INIT_MODEL), reinterpret_cast<void*>(hookInitModel));
}
