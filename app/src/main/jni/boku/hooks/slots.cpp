#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Game membawa 9999 slot per jenis penyimpanan. INT32_MAX membuat setiap
// penyimpanan tanpa batas, yang ditawarkan mod ini sebagai ganti membeli slot
// satu per satu.
constexpr int32_t kLimitSlot = INT32_MAX;

// Setiap jenis penyimpanan adalah field Global, mungkin satu tingkat di bawahnya,
// yang kelasnya membawa penghitung data_Slot_Num yang diisi loader saat mulai.
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

// Loader bisa saja sudah berjalan sebelum hook ini dipasang, jadi nilai yang
// ditinggalkannya dinaikkan sekali saat start juga.
void raiseSlot(void* base, const Slot& slot) {
    if (!slot.fieldCount.field)
        return;

    void* holder = readPtr(base, slot.fieldFirst);
    if (slot.secondary)
        holder = readPtr(holder, slot.fieldSecondary);
    if (holder && readInt(holder, slot.fieldCount) < kLimitSlot)
        writeInt(holder, slot.fieldCount, kLimitSlot);
}

// Setiap jenis penyimpanan memuat jumlah slot miliknya lewat satu fungsi ini.
// Menaikkan jumlahnya ke batas membuka seluruh penyimpanan tanpa membeli slot
// satu per satu.
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
                LOGE("Field slot tidak ditemukan: %s.%s", slot.first, slot.secondary);
            else
                LOGE("Field slot tidak ditemukan: %s", slot.first);
            continue;
        }
        raiseSlot(base, slot);
        ready++;
    }

    int count = static_cast<int>(sizeof(slots) / sizeof(slots[0]));
    if (ready == count)
        LOGI("Field slot OK");
    else
        LOGE("Field slot: %d dari %d ditemukan", ready, count);
    LOGI("Batas slot %d", kLimitSlot);

    installHook(target(T_SLOT_LOAD), reinterpret_cast<void*>(hookLoad));
}
