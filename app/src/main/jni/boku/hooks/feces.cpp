#include "hooks.h"

#include <cstdint>
#include <ctime>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Entri lemparan menskalakan kecepatannya dengan nilai ini, jadi ujung atas
// rentangnya adalah lemparan terkuat yang bisa dibuat kotoran.
constexpr float kPowerFull = 1.0f;

// Lemparan bawaan mengisi charge selama tombol ditekan dan hanya melepas saat
// tombol dilepas, jadi tombol yang ditahan butuh pemicunya sendiri. Jendelanya
// sama dengan pengulangan pistol dan buster.
constexpr long long kRhythmThrowMs = 180;

Field fieldManager = {};
Field fieldStateManager = {};
Field fieldThrowManager = {};
Field fieldCharge = {};
Field fieldChargeActive = {};

// Setiap pemain melempar dari objek state-nya sendiri, jadi irama, arah lempar
// terakhir, dan frame charge mengikuti manajer pemain.
struct Throw {
    void* manager;
    long long lastMs;
    bool up;
    int frameCharge;
};
constexpr int kCountThrow = 32;
Throw throwList[kCountThrow] = {};
int throwNext = 0;

long long nowMs() {
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<long long>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

Throw& throwFor(void* manager) {
    Throw* slotKosong = nullptr;
    for (Throw& throwItem : throwList) {
        if (throwItem.manager == manager)
            return throwItem;
        if (!throwItem.manager && !slotKosong)
            slotKosong = &throwItem;
    }

    Throw& slot = slotKosong ? *slotKosong : throwList[throwNext];
    throwNext = (throwNext + 1) % kCountThrow;
    slot = {};
    slot.manager = manager;
    return slot;
}

// Bernilai benar paling sering sekali per jendela, jadi tombol yang ditahan
// melempar dengan laju tetap.
bool rhythmReady(Throw& throwItem) {
    long long now = nowMs();
    if (now - throwItem.lastMs < kRhythmThrowMs)
        return false;
    throwItem.lastMs = now;
    return true;
}

// Mencatat arah lemparan yang sedang mulai dan memasang iramanya, jadi
// pengulangan pertama menunggu satu jendela daripada menumpuk pada lemparan
// bawaan.
void markThrow(void* own, bool up) {
    void* manager = readPtr(own, fieldManager);
    if (!manager)
        return;
    Throw& throwItem = throwFor(manager);
    throwItem.up = up;
    throwItem.lastMs = nowMs();
    throwItem.frameCharge = 0;
}

// Charge membuat penangan pelepasan tetap siap, jadi charge dibuang setelah
// lemparan sudah meninggalkan tangan dan pelepasan akan menambah lemparan kedua.
void clearCharge(void* manager) {
    void* state = readPtr(manager, fieldStateManager);
    void* dataThrow = readPtr(state, fieldThrowManager);
    void* charge = readPtr(dataThrow, fieldCharge);
    if (charge)
        writeBool(charge, fieldChargeActive, false);
}

// Lemparan mandiri memanggil satu entri yang menerima kekuatan, jadi entri yang
// sama dipakai ulang untuk pengulangan. Method info tersembunyinya adalah yang
// disimpan scan untuk entri itu, karena badan yang membacanya tidak boleh
// melihat nilai null.
void throwNow(void* own, bool up) {
    Target& entry = target(up ? T_FECES_ACTION_UP : T_FECES_ACTION_STD);
    if (entry.addr)
        reinterpret_cast<void (*)(void*, float, void*)>(entry.addr)(own, kPowerFull,
                                                                    entry.method);
}

// Charge lurus dimasuki dari setiap bidikan, dan charge melihat ke atas
// menggantikannya saat pemain membidik ke atas, jadi entri charge terakhir
// adalah arah yang harus diulang tombol yang ditahan. Hanya perubahan arah yang
// memasang ulang hitungan frame, jadi charge baru sempat menyelesaikan
// bidikannya tanpa menahan entri yang berulang.
void setDirectionCharge(void* own, bool up) {
    void* manager = readPtr(own, fieldManager);
    if (!manager || !isLocalPlayerHook(manager))
        return;
    Throw& throwItem = throwFor(manager);
    if (throwItem.up != up)
        throwItem.frameCharge = 0;
    throwItem.up = up;
}

bool fecesLocal(void* own) {
    void* manager = readPtr(own, fieldManager);
    return manager && isLocalPlayerHook(manager);
}

void hookChargeStd(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_FECES_CHARGE_STD).orig);
    if (!original)
        return;
    original(own, method);
    if (features().feces)
        setDirectionCharge(own, false);
}

// Charge melihat ke atas hanya punya satu pemanggil, yaitu sakelar input yang
// berjalan selama pemain membidik ke atas saat charge.
void hookChargeUp(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_FECES_CHARGE_UP).orig);
    if (!original)
        return;
    original(own, method);
    if (features().feces)
        setDirectionCharge(own, true);
}

// Id tangan kosong di akhir lemparan mengeluarkan kotoran dari tangan. Menahan
// perubahan itu membuat kotorannya tetap di tempat, jadi kotoran yang sama bisa
// terbang lagi, dan kekuatannya dipaksa supaya setiap lemparan terkuat.
void hookActionStd(void* own, float power, void* method) {
    auto original = reinterpret_cast<void (*)(void*, float, void*)>(target(T_FECES_ACTION_STD).orig);
    if (!original)
        return;
    if (!features().feces) {
        original(own, power, method);
        return;
    }
    if (!fecesLocal(own)) {
        original(own, power, method);
        return;
    }
    markThrow(own, false);
    setHoldHand(true);
    original(own, kPowerFull, method);
    setHoldHand(false);
}

// Lemparan melihat ke atas butuh perlakuan yang sama.
void hookActionUp(void* own, float power, void* method) {
    auto original = reinterpret_cast<void (*)(void*, float, void*)>(target(T_FECES_ACTION_UP).orig);
    if (!original)
        return;
    if (!features().feces) {
        original(own, power, method);
        return;
    }
    if (!fecesLocal(own)) {
        original(own, power, method);
        return;
    }
    markThrow(own, true);
    setHoldHand(true);
    original(own, kPowerFull, method);
    setHoldHand(false);
}

// Berjalan setiap frame selama aksi lempar ada di layar, dan jalur bawaan
// mengakhiri aksi itu begitu sprite mencapai step terakhirnya. Memulai lemparan
// setelahnya melempar lagi selama tombol ditahan.
void hookUpdateThrow(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_FECES_UPDATE).orig);
    if (!original)
        return;
    original(own, method);
    if (!features().feces)
        return;

    void* manager = readPtr(own, fieldManager);
    if (!manager || !isLocalPlayerHook(manager) || !isActionButtonPressed(manager))
        return;
    Throw& throwItem = throwFor(manager);
    if (rhythmReady(throwItem))
        throwNow(own, throwItem.up);
}

// Berjalan setiap frame state charge. Tombol yang ditahan langsung melempar
// daripada menunggu pelepasan, jadi lemparan pertama saat tombol ditahan jatuh
// sama seperti setiap pengulangan. Sakelar bidikan berjalan pada lintasan input
// berikutnya, jadi frame charge pertama hanya menghitung dan lemparan mulai
// satu frame kemudian dengan arah yang sudah mantap.
void hookUpdateCharge(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_FECES_CHARGE_UPDATE).orig);
    if (!original)
        return;
    original(own, method);
    if (!features().feces)
        return;

    void* manager = readPtr(own, fieldManager);
    if (!manager || !isLocalPlayerHook(manager) || !isActionButtonPressed(manager))
        return;
    Throw& throwItem = throwFor(manager);
    if (throwItem.frameCharge == 0) {
        throwItem.frameCharge = 1;
        return;
    }
    if (!rhythmReady(throwItem))
        return;
    clearCharge(manager);
    throwNow(own, throwItem.up);
}

bool findFieldFeces() {
    void* manager = classOf("Player__Manager");
    return findField(classOf("Player__State__Std__Action__Hh__Nature"), "m", fieldManager) &&
           findField(manager, "__state", fieldStateManager) &&
           findField(fieldClass(fieldStateManager), "_throw", fieldThrowManager) &&
           findField(fieldClass(fieldThrowManager), "charge", fieldCharge) &&
           findField(fieldClass(fieldCharge), "active", fieldChargeActive);
}

} // namespace

void installFecesHook() {
    if (!findFieldFeces()) {
        LOGE("Field kotoran tidak ditemukan; lemparan tetap vanilla");
        return;
    }
    LOGI("Field kotoran siap");

    installHook(target(T_FECES_CHARGE_UPDATE), reinterpret_cast<void*>(hookUpdateCharge));
    installHook(target(T_FECES_CHARGE_STD), reinterpret_cast<void*>(hookChargeStd));
    installHook(target(T_FECES_CHARGE_UP), reinterpret_cast<void*>(hookChargeUp));
    installHook(target(T_FECES_UPDATE), reinterpret_cast<void*>(hookUpdateThrow));
    installHook(target(T_FECES_ACTION_STD), reinterpret_cast<void*>(hookActionStd));
    installHook(target(T_FECES_ACTION_UP), reinterpret_cast<void*>(hookActionUp));
}
