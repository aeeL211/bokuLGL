#include "hooks.h"

#include <cstdint>
#include <ctime>

#include "feature_flags.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldManagerWeapon = {};
Field fieldWeaponCmn = {};
Field fieldManagerState = {};
Field fieldManagerCmn = {};
Field fieldCmnHh = {};
Field fieldState = {};
Field fieldCombo = {};
Field fieldRock = {};
Field fieldCharge = {};
// The runtime charge counter is the number of 32-bit steps compared by the
// buster id lookup against the full step from the configuration.
Field fieldStep = {};
Field fieldHhRock = {};
Field fieldConfigCharge = {};
Field fieldStepFull = {};

// A held button fires once per window. The vanilla pistol and buster both wait
// for a long action timer, and the buster can exhaust its fire interval before
// the end state starts, after which the held button stops firing.
constexpr long long kRhythmPistolMs = 80;
constexpr long long kActionBusterMs = 60;
constexpr long long kRhythmBusterMs = 100;

// Every player runs the same state machine, so the rhythm follows the player
// manager. A shared clock would make a guest reset the local button edge.
struct Rhythm {
    void* manager;
    long long shootPistolMs;
    long long shootBusterMs;
    bool held;
};
constexpr int kCountRhythm = 32;
Rhythm rhythmList[kCountRhythm] = {};
int rhythmNext = 0;

long long nowMs() {
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<long long>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

Rhythm& rhythmFor(void* manager) {
    Rhythm* slotEmpty = nullptr;
    for (Rhythm& rhythm : rhythmList) {
        if (rhythm.manager == manager)
            return rhythm;
        if (!rhythm.manager && !slotEmpty)
            slotEmpty = &rhythm;
    }

    Rhythm& slot = slotEmpty ? *slotEmpty : rhythmList[rhythmNext];
    rhythmNext = (rhythmNext + 1) % kCountRhythm;
    slot = {};
    slot.manager = manager;
    return slot;
}

// Returns true at most once per window, so a held button fires at a steady rate.
bool rhythmReady(long long& lastMs, long long rhythmMs) {
    long long now = nowMs();
    if (now - lastMs < rhythmMs)
        return false;
    lastMs = now;
    return true;
}

void* statePlayer(void* own, const Field& managerField) {
    void* manager = readPtr(own, managerField);
    return readPtr(manager, fieldState);
}

// The same weapon state machine also runs for guests inside the room.
// Every hook below must only speed up the local player, so the player manager
// is checked first and the vanilla path is used for guest bodies.
bool weaponLocal(void* manager) { return isLocalPlayerHook(manager); }

// The state update repeats the shot only while its combo flag is set, and that
// flag follows the held button. Writing it before the vanilla update makes both
// standing and moving styles repeat.
void alignCombo(void* manager) {
    void* state = readPtr(manager, fieldState);
    if (state)
        writeBool(state, fieldCombo, isActionButtonPressed(manager));
}

// The button press starts the vanilla shot, so the rhythm is counted from
// here and the first forced repeat lands one window later, not instantly.
void hookUpdateState(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_STATE_UPDATE).orig);
    if (!original)
        return;

    if (!features().shootAutomatic) {
        original(own, method);
        return;
    }

    void* manager = readPtr(own, fieldManagerState);
    if (!weaponLocal(manager)) {
        original(own, method);
        return;
    }
    Rhythm& rhythm = rhythmFor(manager);
    bool held = isActionButtonPressed(manager);
    if (held && !rhythm.held) {
        rhythm.shootPistolMs = nowMs();
        rhythm.shootBusterMs = rhythm.shootPistolMs;
    }
    rhythm.held = held;
    original(own, method);
}

// The pistol end state is a fixed timer. Ending it on the first update
// removes the pause between shots; the action phase still plays the muzzle and bullet.
bool hookEndPistol(void* own, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_GUN_END).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic || !weaponLocal(readPtr(own, fieldManagerCmn)))
        return original(own, method);
    return true;
}

// The vanilla pistol action only ends after the second frame plus the action
// timer. Answering yes on the short rhythm while the button is held shoots again
// before that timer lets the vanilla path through.
bool hookEndActionPistol(void* own, int image, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, int, void*)>(target(T_GUN_ACTION_END).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic)
        return original(own, image, method);

    void* manager = readPtr(own, fieldManagerCmn);
    if (!weaponLocal(manager))
        return original(own, image, method);
    Rhythm& rhythm = rhythmFor(manager);
    if (original(own, image, method)) {
        if (isActionButtonPressed(manager))
            rhythm.shootPistolMs = nowMs();
        return true;
    }
    if (!isActionButtonPressed(manager))
        return false;
    return rhythmReady(rhythm.shootPistolMs, kRhythmPistolMs);
}

// Keep the combo flag current during the pistol action, then run the vanilla update.
void hookUpdatePistol(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_GUN_UPDATE).orig);
    if (!original)
        return;

    void* manager = readPtr(own, fieldManagerWeapon);
    if (features().shootAutomatic && weaponLocal(manager))
        alignCombo(manager);
    original(own, method);
}

// The same alignment during the buster action.
void hookUpdateBuster(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_ROCK_UPDATE).orig);
    if (!original)
        return;

    void* manager = readPtr(own, fieldManagerWeapon);
    if (features().shootAutomatic && weaponLocal(manager))
        alignCombo(manager);
    original(own, method);
}

// Every style ends the buster action through this check, which waits for the
// long action timer. Ending it one action window after the shot starts
// keeps the muzzle frame and effects while dropping the rest of the wait.
bool hookEndActionBuster(void* own, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_ROCK_ACTION_END).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic)
        return original(own, method);

    void* manager = readPtr(own, fieldManagerCmn);
    if (!weaponLocal(manager))
        return original(own, method);
    Rhythm& rhythm = rhythmFor(manager);
    if (original(own, method))
        return true;
    if (!isActionButtonPressed(manager))
        return false;
    return nowMs() - rhythm.shootBusterMs >= kActionBusterMs;
}

// The moving style leaves the buster end state through this check, which gives
// up once the fire interval has passed. Answering yes on the rhythm keeps
// firing while the button is held.
bool hookEndToActionBuster(void* own, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_ROCK_END_TO_ACTION).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic)
        return original(own, method);

    void* manager = readPtr(own, fieldManagerCmn);
    if (!weaponLocal(manager))
        return original(own, method);
    Rhythm& rhythm = rhythmFor(manager);
    if (original(own, method)) {
        if (isActionButtonPressed(manager))
            rhythm.shootBusterMs = nowMs();
        return true;
    }
    if (!isActionButtonPressed(manager))
        return false;
    return rhythmReady(rhythm.shootBusterMs, kRhythmBusterMs);
}

// The standing buster re-fires inside its own end update. Holding that update
// until the rhythm is due and then starting the action keeps the same rate,
// and the vanilla path still runs whenever the button is released.
void hookUpdateEndBuster(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_ROCK_END_UPDATE).orig);
    if (!original)
        return;
    if (!features().shootAutomatic) {
        original(own, method);
        return;
    }

    auto directionEnd = reinterpret_cast<int32_t (*)(void*, void*)>(target(T_ROCK_END_DIR).addr);
    auto action = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_ROCK_ACTION).addr);
    void* manager = readPtr(own, fieldManagerWeapon);
    void* cmn = readPtr(own, fieldWeaponCmn);
    if (!directionEnd || !action || !cmn || !weaponLocal(manager) ||
        !isActionButtonPressed(manager)) {
        original(own, method);
        return;
    }

    Rhythm& rhythm = rhythmFor(manager);
    if (!rhythmReady(rhythm.shootBusterMs, kRhythmBusterMs))
        return;

    action(own, directionEnd(cmn, target(T_ROCK_END_DIR).method), target(T_ROCK_ACTION).method);
}

// The id lookup only returns a charged projectile when the runtime charge step
// matches the full step from the configuration. Writing that step before the
// lookup makes every shot use the strongest ammo.
int32_t hookIdBuster(void* own, void* method) {
    auto original = reinterpret_cast<int32_t (*)(void*, void*)>(target(T_ROCK_ID).orig);
    if (!original)
        return 0;
    if (!features().busterFull)
        return original(own, method);
    if (!weaponLocal(readPtr(own, fieldManagerCmn)))
        return original(own, method);

    void* state = statePlayer(own, fieldManagerCmn);
    void* rock = readPtr(state, fieldRock);
    void* charge = readPtr(rock, fieldCharge);
    void* hh = readPtr(own, fieldCmnHh);
    void* config = readPtr(hh, fieldHhRock);
    void* chargeConfig = readPtr(config, fieldConfigCharge);
    if (charge && chargeConfig)
        writeInt(charge, fieldStep, readInt(chargeConfig, fieldStepFull));

    return original(own, method);
}

bool findFieldWeapon() {
    void* cmn = classOf("Player__State__Cmn");
    void* weapon = classOf("Player__State__Std__Action__Hh__Weapon");
    void* managerState = classOf("Player__State");
    return findField(managerState, "m", fieldManagerState) &&
           findField(weapon, "m", fieldManagerWeapon) && findField(weapon, "cmn", fieldWeaponCmn) &&
           findField(cmn, "m", fieldManagerCmn) && findField(cmn, "chr_Hh", fieldCmnHh) &&
           findField(fieldClass(fieldManagerWeapon), "__state", fieldState) &&
           findField(fieldClass(fieldState), "combo", fieldCombo) &&
           findField(fieldClass(fieldState), "_rockman__Rock_Buster", fieldRock) &&
           findField(fieldClass(fieldRock), "charge", fieldCharge) &&
           findField(fieldClass(fieldCharge), "step", fieldStep) &&
           findField(fieldClass(fieldCmnHh), "_rockman__Rock_Buster", fieldHhRock) &&
           findField(fieldClass(fieldHhRock), "charge", fieldConfigCharge) &&
           findField(fieldClass(fieldConfigCharge), "step__Full", fieldStepFull);
}

} // namespace

void installWeaponHook() {
    if (!findFieldWeapon()) {
        LOGE("Weapon field not found; weapon stays vanilla");
        return;
    }
    LOGI("Weapon field ready");

    if (!target(T_ROCK_ACTION).addr || !target(T_ROCK_END_DIR).addr)
        LOGE("Buster re-fire method not found; standing buster stays vanilla");

    installHook(target(T_STATE_UPDATE), reinterpret_cast<void*>(hookUpdateState));
    installHook(target(T_GUN_END), reinterpret_cast<void*>(hookEndPistol));
    installHook(target(T_GUN_ACTION_END), reinterpret_cast<void*>(hookEndActionPistol));
    installHook(target(T_GUN_UPDATE), reinterpret_cast<void*>(hookUpdatePistol));
    installHook(target(T_ROCK_UPDATE), reinterpret_cast<void*>(hookUpdateBuster));
    installHook(target(T_ROCK_ACTION_END), reinterpret_cast<void*>(hookEndActionBuster));
    installHook(target(T_ROCK_END_UPDATE), reinterpret_cast<void*>(hookUpdateEndBuster));
    installHook(target(T_ROCK_END_TO_ACTION), reinterpret_cast<void*>(hookEndToActionBuster));
    installHook(target(T_ROCK_ID), reinterpret_cast<void*>(hookIdBuster));
}
