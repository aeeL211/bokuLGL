#include "hooks.h"

#include <cstdint>

#include "hook_manager.h"
#include "log.h"
#include "targets.h"

namespace {

// Hh__Change writes the new hold id to the player model and rebuilds the hand
// sprite. A feature that keeps its item sets this flag around the vanilla call
// that would empty the hand, so that change has no effect.
thread_local bool holdHand = false;
// Last id known to be in the hand, kept so the end of eating can restore the
// same item after the animation hides the sprite.
void* ownerHand = nullptr;
int32_t idHand = 0;
// The end of eating asks for the hand back, but a rebuild while the eating state
// is still active builds the hidden eating pose. The rebuild waits for the end
// of that player update, when the state machine has passed the eating animation.
bool refreshPending = false;
int remainingLogRefresh = 12;

void buildReloadHand(void* own) {
    auto hh = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_HH_CHANGE).orig);
    if (!hh)
        return;
    hh(own, idHand, target(T_HH_CHANGE).method);
    if (remainingLogRefresh > 0) {
        remainingLogRefresh--;
        LOGI("Hand refreshed: id=%d", idHand);
    }
}

// Reads the id actually still in the hand from the player model. The hand id
// cache can already be 0 if the food feature was turned off and the game emptied
// the hand, so this live value is used when the next hand change is held.
int32_t idHandFromGame(void* own) {
    auto read = reinterpret_cast<int32_t (*)(void*, void*)>(target(T_HH_ID).addr);
    if (!read)
        return idHand;
    return read(own, target(T_HH_ID).method);
}

void hookChangeHh(void* own, int32_t id, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_HH_CHANGE).orig);
    if (!original)
        return;
    if (holdHand) {
        // The game is about to empty the hand. The id in use now is read before
        // that change is lost, and reused later by refreshHand.
        ownerHand = own;
        idHand = idHandFromGame(own);
        return;
    }
    original(own, id, method);
    // Hh__Change also runs for guests inside the room. The hand cache must only
    // follow the local player, otherwise a guest's end of eating can rebuild the
    // local player's hand.
    if (!isLocalPlayerHook(own))
        return;
    ownerHand = own;
    idHand = id;
}

void hookUpdate(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_PLAYER_UPDATE).orig);
    if (!original)
        return;
    original(own, method);
    onPlayerUpdate(own);
    if (!refreshPending || own != ownerHand)
        return;
    refreshPending = false;
    buildReloadHand(own);
}

} // namespace

void setHoldHand(bool hold) { holdHand = hold; }

void refreshHand() {
    if (!ownerHand)
        return;
    // Without the update hook there is no safe place to rebuild later, so fall
    // back to the moment eating ends.
    if (!target(T_PLAYER_UPDATE).orig) {
        buildReloadHand(ownerHand);
        return;
    }
    refreshPending = true;
}

void installHandHook() {
    installHook(target(T_HH_CHANGE), reinterpret_cast<void*>(hookChangeHh));
    installHook(target(T_PLAYER_UPDATE), reinterpret_cast<void*>(hookUpdate));
}
