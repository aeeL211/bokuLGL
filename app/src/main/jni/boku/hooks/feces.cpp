#include "hooks.h"

#include <cstdint>
#include <ctime>

#include "feature_flags.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Throw entries scale their speed with this value, so the top of
// the range is the strongest throw the feces can produce.
constexpr float kPowerFull = 1.0f;

// The vanilla throw fills charge while the button is held and only releases
// when the button is let go, so a held button needs its own trigger. The window
// matches the pistol and buster repeat.
constexpr long long kRhythmThrowMs = 180;

Field fieldManager = {};
Field fieldStateManager = {};
Field fieldThrowManager = {};
Field fieldCharge = {};
Field fieldChargeActive = {};

// Each player throws from its own state object, so the rhythm, the last throw
// direction, and the charge frame follow the player manager.
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
    Throw* slotEmpty = nullptr;
    for (Throw& throwItem : throwList) {
        if (throwItem.manager == manager)
            return throwItem;
        if (!throwItem.manager && !slotEmpty)
            slotEmpty = &throwItem;
    }

    Throw& slot = slotEmpty ? *slotEmpty : throwList[throwNext];
    throwNext = (throwNext + 1) % kCountThrow;
    slot = {};
    slot.manager = manager;
    return slot;
}

// True at most once per window, so a held button throws at a steady rate.
bool rhythmReady(Throw& throwItem) {
    long long now = nowMs();
    if (now - throwItem.lastMs < kRhythmThrowMs)
        return false;
    throwItem.lastMs = now;
    return true;
}

// Records the direction of the throw now starting and arms the rhythm, so the
// first repeat waits one window instead of stacking on the vanilla throw.
void markThrow(void* own, bool up) {
    void* manager = readPtr(own, fieldManager);
    if (!manager)
        return;
    Throw& throwItem = throwFor(manager);
    throwItem.up = up;
    throwItem.lastMs = nowMs();
    throwItem.frameCharge = 0;
}

// Charge keeps the release handler armed, so charge is cleared after
// the throw has left the hand and the release would add a second throw.
void clearCharge(void* manager) {
    void* state = readPtr(manager, fieldStateManager);
    void* dataThrow = readPtr(state, fieldThrowManager);
    void* charge = readPtr(dataThrow, fieldCharge);
    if (charge)
        writeBool(charge, fieldChargeActive, false);
}

// A standalone throw calls one entry that takes the power, so the same
// entry is reused for the repeat. Its hidden method info is the one the
// scan stored for that entry, because the body that reads it must not
// see a null value.
void throwNow(void* own, bool up) {
    Target& entry = target(up ? T_FECES_ACTION_UP : T_FECES_ACTION_STD);
    if (entry.addr)
        reinterpret_cast<void (*)(void*, float, void*)>(entry.addr)(own, kPowerFull, entry.method);
}

// Straight charge is entered from every aim, and the upward charge
// replaces it when the player aims up, so the last charge entry is the
// direction the held button must repeat. Only a direction change
// re-arms the frame count, so a fresh charge can finish its aim
// without holding back the repeating entry.
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

// The upward charge has a single caller, the input switch that runs while
// the player aims up during charge.
void hookChargeUp(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_FECES_CHARGE_UP).orig);
    if (!original)
        return;
    original(own, method);
    if (features().feces)
        setDirectionCharge(own, true);
}

// The bare hand id at the end of a throw removes the feces from the hand.
// Holding back that change keeps the feces in place, so the same feces can
// fly again, and the power is forced so every throw is the strongest.
void hookActionStd(void* own, float power, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, float, void*)>(target(T_FECES_ACTION_STD).orig);
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

// The upward throw needs the same treatment.
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

// Runs every frame while the throw action is on screen, and the vanilla path
// ends that action once the sprite reaches its last step. Starting a throw
// after that throws again while the button is held.
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

// Runs every frame of the charge state. A held button throws immediately
// instead of waiting for release, so the first throw when the button is held
// lands the same as every repeat. The aim switch runs on the next input
// pass, so the first charge frame only counts and the throw starts
// one frame later with a direction that is already settled.
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
        LOGE("Feces field not found; throw stays vanilla");
        return;
    }
    LOGI("Feces field ready");

    installHook(target(T_FECES_CHARGE_UPDATE), reinterpret_cast<void*>(hookUpdateCharge));
    installHook(target(T_FECES_CHARGE_STD), reinterpret_cast<void*>(hookChargeStd));
    installHook(target(T_FECES_CHARGE_UP), reinterpret_cast<void*>(hookChargeUp));
    installHook(target(T_FECES_UPDATE), reinterpret_cast<void*>(hookUpdateThrow));
    installHook(target(T_FECES_ACTION_STD), reinterpret_cast<void*>(hookActionStd));
    installHook(target(T_FECES_ACTION_UP), reinterpret_cast<void*>(hookActionUp));
}
