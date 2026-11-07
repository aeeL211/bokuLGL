#pragma once

#include <cstddef>

void installIapHook();
void installMultiplayerHook();
void installChatHook();
void installSensitiveHook();
void installTextInputHook();
void installProfileHook();
void installPasswordHook();
void installSkinRangeHook();
void installAboutHook();
void installFoodHook();
void installSkinHook();
void installPrivacyHook();
void installWeaponHook();
void installEffectsHook();
void installKnockbackHook();
void installFecesHook();
void installHandHook();
void installSlotHook();
void installSpoofHook();
void installRoomHook();
void installPlayerHook();

// True when this player manager is the local player. Used by hooks that run for
// every player so a feature never touches a guest body. While the comparison
// fields are not ready the result is true, so a feature never dies silently.
bool isLocalPlayerHook(void* manager);

// Finds the two local player comparison fields. Called before the feature hooks
// are installed, then repeated by installPlayerHook.
void findLocalPlayerFields();

// Called from the player update hook, on the game thread. Handles Teleport and
// No Clip for the local player only.
void onPlayerUpdate(void* manager);

// Packets only the host receives are sent with the room's host key, and the
// replacement lives for exactly one send call.
void startSpoof(void* multi);
void stopSpoof();
void* hostKeyFrom(void* multi);

// The player's own key, read only after the room has a photon object. Outside a
// room the result is nullptr, because the game getter throws
// NullReferenceException.
void* safePlayerKey(void* player);

// True while the interaction button is held for this player.
bool isActionButtonPressed(void* manager);
void findActionFields();

// Raised by features that keep their item in hand even though the stock call
// would empty the hand slot.
void setHoldHand(bool hold);

// Restores the last held id to the hand after the eating animation hides its
// sprite, so a held food item stays visible.
void refreshHand();

// Result of copying one guest's skin. A slot is only filled for Saved and Exists.
enum class CopyResult { Failed, Saved, Exists, Missing };

// Writes a guest model into the first empty slot, along with its owner's name.
// The model comes from the Guest__Manager.Add/Init__Model cache, then from the
// guest's live model when the cache is not filled. The caller only passes the
// key and the guest name. Called on the game thread.
CopyResult copyGuestModel(void* managerGuest, const char* key, const char* name, int& slot);

// The player list copy, action queue, and local player position live in
// boku/player_service.h. The room hooks only fill that copy, and the menu only
// reads it.
