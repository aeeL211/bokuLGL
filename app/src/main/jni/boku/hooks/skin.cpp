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

// The model string holds several hundred separated fields. This limit only
// stops broken lengths from running past the string.
constexpr int32_t kLimitCharacterModel = 16384;
// One entry per distinct skin that can appear in a single session; a repeat only
// costs one table lookup.
constexpr int kLimitHash = 512;
constexpr int kSizeTableHash = 1024;
constexpr int kMaskTableHash = kSizeTableHash - 1;
constexpr int kLimitSlot = 1000;
constexpr size_t kLimitBytePath = 1024;
// Guest keys are short; longer ones are not worth a cache entry.
constexpr size_t kLimitByteUser = 64;
// One entry per guest whose model can be copied from the menu.
constexpr int kMaxGuest = 16;
// The file is read in pieces, so no buffer the size of the model string is needed.
constexpr size_t kLimitBytePiece = 1024;
constexpr uint64_t kInitialHash = 1469598103934665603ull;
constexpr uint64_t kPrimeHash = 1099511628211ull;

char textPath[kLimitBytePath];

// Hash of a model that already owns a slot, plus its slot number, so the next
// copy request can point at where that skin is already stored.
struct SlotModel {
    uint64_t hash;
    int slot;
};

SlotModel tableHash[kSizeTableHash];
int countHashModel = 0;
int slotNext = 1;
bool slotReady = false;

// Guest models are kept in memory when they enter the room. Writing to a slot
// only happens when the player asks for it from the menu, so no skin is saved
// without the user's consent.
struct ModelGuest {
    char key[kLimitByteUser];
    char* model;
};

ModelGuest guestList[kMaxGuest];
int countGuest = 0;
int guestNext = 0;

// The guest join hook and the guest update hook can run on different threads,
// so the cache is guarded by a lock. Readers copy the model and then release the
// lock, so a replaced entry is never read half-written.
pthread_mutex_t guestKey = PTHREAD_MUTEX_INITIALIZER;

// Model owned by a single guest, copied from the cache so the entry can be
// replaced after the getter returns.
struct ModelCopy {
    char* text = nullptr;
    ~ModelCopy() { free(text); }
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
        result *= kPrimeHash;
    }
    return result;
}

// Open table indexed by the hash itself, so an existing skin is found without
// scanning a list. A zero hash is moved to one so it is never read as an empty
// slot.
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

// Slots are reused from the start when the list is full, so the oldest guest can
// no longer be copied without leaving and joining again.
ModelGuest& slotGuest(const char* key) {
    for (int i = 0; i < countGuest; i++) {
        if (!strcmp(guestList[i].key, key))
            return guestList[i];
    }
    int index = countGuest;
    if (countGuest < kMaxGuest) {
        countGuest++;
    } else {
        index = guestNext;
        guestNext = (guestNext + 1) % kMaxGuest;
        releaseModelGuest(guestList[index]);
    }
    snprintf(guestList[index].key, sizeof(guestList[index].key), "%s", key);
    return guestList[index];
}

// One UTF-16 unit becomes at most three UTF-8 bytes, and the UTF-8 writer leaves
// four bytes at the end; this size fits the whole string contents.
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

// The model folder belongs to the same Manager as the guest list holder, so the
// game's own writer places its files in Save/Model without guessing the path.
// The file name lives on a holder that a half-built save does not own yet, and
// the path builder dereferences it, so such a save is reported as missing.
void* modelData(void* managerGuest) {
    if (!fieldReady)
        return nullptr;
    void* manager = readPtr(managerGuest, fieldGuest);
    void* data = readPtr(manager, fieldDataManager);
    void* model = readPtr(data, fieldModel);
    return readPtr(model, fieldNameFile) ? model : nullptr;
}

// The game's own path for a model slot, so its layout is never guessed here.
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

// The game writes the model's own display name the same way, so the slot gets a
// normal Name.txt next to Data.txt.
void writeNameSlot(void* model, int slot, void* text) {
    auto call =
        reinterpret_cast<void (*)(void*, int32_t, void*, void*)>(target(T_DATA_MODEL_NAME).addr);
    if (call)
        call(model, slot, text, target(T_DATA_MODEL_NAME).method);
}

// Skins from earlier sessions already own their slots, so the hash of every
// Data.txt on disk is read once and the next write continues after it. The
// contents are read in pieces so no buffer the size of the model string is needed.
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
    LOGI("Skin slots: %d stored, next %d", countHashModel, slotNext);
}

// Guest models are copied into memory, not to disk, because the menu decides when
// that skin is saved. The key is the guest id that the player list also uses.
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
    LOGI("Guest model recorded: %s (%d characters)", keyText, lengthString(model));
}

bool getGuestModel(const char* key, ModelCopy& result) {
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

// Guests already in the room before the mod loaded never pass through the Add
// hook. The model in use can be read from the live list, then converted back to
// a string by the game's own Model__To__Str.
bool guestModelAlive(void* managerGuest, const char* key, ModelCopy& result) {
    if (!fieldGuestReady)
        return false;
    void* manager = readPtr(managerGuest, fieldGuest);
    void* list = readPtr(manager, guestListField);
    void* item = readPtr(list, fieldItemGuest);
    int32_t count = item ? readInt(list, guestSizeField) : 0;
    int32_t capacity = item ? lengthArray(item) : 0;
    if (count <= 0 || count > capacity)
        return false;

    auto toText = reinterpret_cast<void* (*)(void*, void*)>(target(T_MODEL_TO_STR).addr);
    if (!toText) {
        LOGE("Target _Model.Model__To__Str not found, live guest model unavailable");
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

// The live guest list fields are used when the Add cache is empty. The names
// match those used by the Room section, so both point at the same list.
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

    ModelCopy copied;
    if (!getGuestModel(key, copied) || !copied.text || !*copied.text) {
        free(copied.text);
        copied.text = nullptr;
        if (!guestModelAlive(managerGuest, key, copied) || !copied.text) {
            LOGI("Guest model not found: %s", key);
            return CopyResult::Missing;
        }
        LOGI("Guest model read from live list: %s", key);
    }
    const char* modelText = copied.text;

    void* model = modelData(managerGuest);
    if (!model) {
        if (!alreadyRecordNotReady) {
            alreadyRecordNotReady = true;
            LOGE("Skin model not ready, skin copy skipped");
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
        LOGE("Skin slots full, %d skins stored", countHashModel);
        return CopyResult::Failed;
    }

    void* textModel = stringFromUtf8(modelText);
    if (!textModel) {
        LOGE("Failed to build model string for slot %d", slotNext);
        return CopyResult::Failed;
    }
    writeSlot(model, slotNext, textModel);
    if (!slotHas(model, slotNext)) {
        LOGE("Skin write failed: slot %d", slotNext);
        return CopyResult::Failed;
    }

    slot = slotNext++;
    recordSlot(value, slot);
    LOGI("Guest skin saved: slot %d (%zu bytes)", slot, strlen(modelText));

    if (name && *name) {
        void* nameText = stringFromUtf8(name);
        if (nameText) {
            writeNameSlot(model, slot, nameText);
            LOGI("Skin name: slot %d", slot);
        }
    }
    return CopyResult::Saved;
}

namespace {

using FnAdd = void (*)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*,
                          void*, void*, void*, void*, bool, bool, bool, bool, bool, bool, void*);
using FnInitModel = void (*)(void*, void*, void*);

// The guest list is built from this single call on both host and client, and its
// Model argument is the full skin string. That model is only kept in memory
// until the player requests it from the menu.
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

// The model can arrive through Init__Model after the guest joins, so the string
// received here is recorded too. The key already exists on the guest itself.
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
    LOGI("Skin model fields %s", fieldReady ? "OK" : "missing, skin copy inactive");

    fieldGuestReady = findFieldGuestAlive();
    LOGI("Guest list fields %s", fieldGuestReady ? "OK" : "missing, skin copy from cache only");

    installHook(target(T_GUEST_ADD), reinterpret_cast<void*>(hookAdd));
    installHook(target(T_GUEST_INIT_MODEL), reinterpret_cast<void*>(hookInitModel));
}
