#include "hooks.h"

#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

using FnKey = void* (*)(void*, void*);

// Host-specific packets pass through the receiver because their payload carries
// the sender key, and the receiver only compares that string. This replacement
// lives on the sender thread and is cleared once the packet leaves.
thread_local bool spoofActive = false;
thread_local void* keySpoof = nullptr;

Field fieldManagerPlayer = {};
Field fieldMultiManager = {};
Field fieldPhoton = {};

void* hookKey(void* own, void* method) {
    if (spoofActive && keySpoof)
        return keySpoof;
    auto original = reinterpret_cast<FnKey>(target(T_PLAYER_KEY).orig);
    return original ? original(own, method) : nullptr;
}

// Both key getters throw NullReferenceException while the room has no photon
// object yet, and an exception escaping a hook kills the process. This check
// uses fields instead of getters, so it cannot throw.
bool photonReady(void* multi) { return multi && fieldPhoton.field && readPtr(multi, fieldPhoton); }

} // namespace

// The host key is only read once the room's photon object exists. Callers
// outside a room receive nullptr and skip their packet.
void* hostKeyFrom(void* multi) {
    auto key = reinterpret_cast<FnKey>(target(T_HOST_KEY).addr);
    if (!photonReady(multi) || !key)
        return nullptr;
    return key(multi, target(T_HOST_KEY).method);
}

// Safe copy of Player__Manager.Key. Used by features that need the player's own
// key without risking an exception from a room that is not ready.
void* safePlayerKey(void* player) {
    auto key = reinterpret_cast<FnKey>(target(T_PLAYER_KEY).orig);
    void* manager = player ? readPtr(player, fieldManagerPlayer) : nullptr;
    if (!key || !photonReady(readPtr(manager, fieldMultiManager)))
        return nullptr;
    return key(player, target(T_PLAYER_KEY).method);
}

// Only valid until stopSpoof(), so callers wrap a single send call and the rest
// of the game keeps using the player's own key.
void startSpoof(void* multi) {
    keySpoof = hostKeyFrom(multi);
    spoofActive = keySpoof != nullptr;
}

void stopSpoof() {
    spoofActive = false;
    keySpoof = nullptr;
}

void installSpoofHook() {
    if (!target(T_PLAYER_KEY).addr || !target(T_HOST_KEY).addr) {
        LOGE("Sender spoof target not found");
        return;
    }
    bool complete = findField(classOf("Player__Manager"), "a", fieldManagerPlayer) &&
                   findField(fieldClass(fieldManagerPlayer), "multi", fieldMultiManager) &&
                   findField(fieldClass(fieldMultiManager), "photon", fieldPhoton);
    if (complete)
        LOGI("Player key fields ready");
    else
        LOGE("Player key fields incomplete; key read only when photon is available");
    installHook(target(T_PLAYER_KEY), reinterpret_cast<void*>(hookKey));
}
