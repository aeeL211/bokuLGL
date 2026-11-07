#include "hooks.h"

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "targets.h"

namespace {

// Every end-of-eating method is owned by a player state, and that state holds
// its own Player__Manager. The m offset differs per posture, so the field is
// looked up per target and stored in each template instantiation.
template <TargetId id> Field& fieldPlayerEat() {
    static Field field = {};
    return field;
}

// Food only leaves the hand through its own ...__Eat__End, which asks
// Player__Manager.Hh__Change for an empty hand id. Every food and posture has a
// copy of that method, so each one holds the hand change and the piece stays in
// the hand. Sushi, ice cream, and popsicle are left alone on purpose: one bite
// finishes that item, then the game puts the player in an end pose that hides
// the hand layer, so a held piece stays invisible until the player moves.
// Gingerbread and hot cross bun end their last bite on a pose that still draws
// the hand.
//
// The eating animation hides the held sprite right after that hand change, so
// the hand is rebuilt at the end of that player update, when the state machine
// has passed the eating animation. Without it the food is still in the hand but
// invisible until the player moves and the state refresh rebuilds it.
//
// Guests inside the room run the same state in the local simulation, so only the
// local player's hand is held.
template <TargetId id> void hookEndEat(void* self, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(id).orig);
    if (!original)
        return;
    if (!features().food) {
        original(self, method);
        return;
    }
    const Field& playerField = fieldPlayerEat<id>();
    if (playerField.field && !isLocalPlayerHook(readPtr(self, playerField))) {
        original(self, method);
        return;
    }
    setHoldHand(true);
    original(self, method);
    setHoldHand(false);
    refreshHand();
}

template <TargetId id> void installEndEat() {
    findField(classOf(target(id).cls), "m", fieldPlayerEat<id>());
    installHook(target(id), reinterpret_cast<void*>(hookEndEat<id>));
}

} // namespace

void installFoodHook() {
    installEndEat<T_EAT_END_KNEEL>();
    installEndEat<T_EAT_END_SIT_F>();
    installEndEat<T_EAT_END_SIT_S>();
    installEndEat<T_EAT_END_FOOD>();
    installEndEat<T_EAT_END_BUN_KNEEL>();
    installEndEat<T_EAT_END_BUN_SIT_F>();
    installEndEat<T_EAT_END_BUN_SIT_S>();
    installEndEat<T_EAT_END_BUN_FOOD>();
}
