#include "hooks.h"

#include <cstdint>

#include "feature_flags.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// The game ships 9999 slots per storage type. INT32_MAX makes every storage
// unlimited, which this mod offers instead of buying slots one at a time.
constexpr int32_t kLimitSlot = INT32_MAX;

// Each storage type is a Global field, possibly one level below it, whose class
// carries the data_Slot_Num counter filled by the loader at startup.
struct Slot {
    const char* first;
    const char* secondary;
    Field fieldFirst;
    Field fieldSecondary;
    Field fieldCount;
};

Slot slots[] = {
    {"model", nullptr, {}, {}, {}},       {"face", nullptr, {}, {}, {}},
    {"portrait", nullptr, {}, {}, {}},    {"photo", nullptr, {}, {}, {}},
    {"_favorite", nullptr, {}, {}, {}},   {"_melody", nullptr, {}, {}, {}},
    {"world", nullptr, {}, {}, {}},       {"doll", nullptr, {}, {}, {}},
    {"toolbar", "face", {}, {}, {}},      {"toolbar", "hh", {}, {}, {}},
    {"_paint", "painting", {}, {}, {}},   {"_paint", "palette", {}, {}, {}},
    {"_block", "pack", {}, {}, {}},       {"_block", "own", {}, {}, {}},
    {"_block", "list", {}, {}, {}},       {"_jukebox", "library", {}, {}, {}},
    {"_jukebox", "playlist", {}, {}, {}},
};

bool findFieldSlot(Slot& slot) {
    void* klassGlobal = classOf("Global");
    if (!findField(klassGlobal, slot.first, slot.fieldFirst))
        return false;

    void* klassHolder = fieldClass(slot.fieldFirst);
    if (slot.secondary) {
        if (!findField(klassHolder, slot.secondary, slot.fieldSecondary))
            return false;
        klassHolder = fieldClass(slot.fieldSecondary);
    }
    return findField(klassHolder, "data_Slot_Num", slot.fieldCount);
}

// The loader may already have run before this hook was installed, so the value
// it left is raised once at startup as well.
void raiseSlot(void* base, const Slot& slot) {
    if (!slot.fieldCount.field)
        return;

    void* holder = readPtr(base, slot.fieldFirst);
    if (slot.secondary)
        holder = readPtr(holder, slot.fieldSecondary);
    if (holder && readInt(holder, slot.fieldCount) < kLimitSlot)
        writeInt(holder, slot.fieldCount, kLimitSlot);
}

// Each storage type loads its slot count through this single function. Raising
// the count to the limit unlocks the whole storage without buying slots one at
// a time.
void hookLoad(int32_t* count, void* key, int32_t standalone, void* method) {
    auto original =
        reinterpret_cast<void (*)(int32_t*, void*, int32_t, void*)>(target(T_SLOT_LOAD).orig);
    if (!original)
        return;

    original(count, key, standalone, method);
    if (features().slot && count && *count < kLimitSlot)
        *count = kLimitSlot;
}

} // namespace

void installSlotHook() {
    void* base = dataFieldStatic(classOf("Global"));

    int ready = 0;
    for (Slot& slot : slots) {
        if (!findFieldSlot(slot)) {
            if (slot.secondary)
                LOGE("Slot field not found: %s.%s", slot.first, slot.secondary);
            else
                LOGE("Slot field not found: %s", slot.first);
            continue;
        }
        raiseSlot(base, slot);
        ready++;
    }

    int count = static_cast<int>(sizeof(slots) / sizeof(slots[0]));
    if (ready == count)
        LOGI("Slot fields OK");
    else
        LOGE("Slot fields: %d of %d found", ready, count);
    LOGI("Slot limit %d", kLimitSlot);

    installHook(target(T_SLOT_LOAD), reinterpret_cast<void*>(hookLoad));
}
