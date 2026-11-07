#pragma once

#include <cstddef>

// Data shared between the game thread and the menu thread. The game thread
// writes a small copy here, the menu thread only reads that copy; no IL2CPP
// object ever crosses over.
namespace player {

constexpr int kMaxPlayer = 16;
constexpr int kLimitAction = 8;
constexpr std::size_t kCharacterKey = 40;
constexpr std::size_t kCharacterName = 96;

struct Summary {
    char name[kCharacterName];
    char key[kCharacterKey];
    char role[8];
};

struct Action {
    enum class Kind { Kick, KickPermanent, KickHost, KickAll, Copy, CopyAvatar };

    Kind type;
    char key[kCharacterKey];
    char name[kCharacterName];
};

// Written once by the room hook after the guest list field is found.
void setReady(bool ready);

// Player list update window. Returns true when the game thread may build a
// fresh copy now; the caller decides its contents.
bool serveList();
void storeList(const Summary* data, int count);

// Lines of "name<TAB>key<TAB>role", separated by newlines. Safe from the UI thread.
void copyList(char* output, std::size_t size);

// Menu action queue. Bounded queue that never blocks the UI thread.
bool enqueueKick(const char* key, const char* name, bool permanent);
bool enqueueKickHost(const char* name);
bool enqueueKickAll();
bool enqueueCopy(const char* key, const char* name);
bool enqueueCopyAvatar(const char* key);
bool takeAction(Action& exit);

// Window for updating the local player's position.
bool servePosition();
void storePosition(bool hasTile, float tileX, float tileY, float worldX, float worldY);

// Line "tileX<TAB>tileY<TAB>worldX<TAB>worldY"; the tile part is empty when the
// game does not provide a tile field.
void copyPosition(char* output, std::size_t size);

} // namespace player
