#include "hooks.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "player_service.h"
#include "targets.h"
#include "toast.h"

namespace {

using FnKick = void (*)(void*, void*, void*);
using FnClear = void (*)(void*, void*, void*);

Field fieldGuestManager = {};
Field fieldMultiManager = {};
Field fieldPlayerManager = {};
Field fieldItemGuest = {};
Field guestSizeField = {};
Field guestKeyField = {};
Field fieldAccountGuest = {};
Field guestNameField = {};
Field fieldTransferMulti = {};
Field fieldPipeTransfer = {};
Field fieldManagerGuest = {};
std::atomic<bool> readyMenu{false};

void textFrom(void* text, char* output, size_t size, const char* backup) {
    output[0] = '\0';
    if (text && toUtf8(text, output, size) && output[0])
        return;
    snprintf(output, size, "%s", backup);
}

void* pipeFrom(void* multi) {
    return readPtr(readPtr(multi, fieldTransferMulti), fieldPipeTransfer);
}

// The vanilla kick button sends two packets: the kick makes the guest show
// a notification, and the guest removal drops it from every list.
// Both carry the sender key, so the same spoof makes the client
// accept them as host packets. A temporary kick uses only the first
// packet; a permanent kick uses both.
bool kickKey(void* multi, void* key, bool permanent, int repeat) {
    void* pipe = pipeFrom(multi);
    auto kick = reinterpret_cast<FnKick>(target(T_KICK_SEND).addr);
    auto remove = reinterpret_cast<FnClear>(target(T_GUEST_REMOVE_SEND).addr);
    if (!key || !pipe || !kick || repeat <= 0)
        return false;
    startSpoof(multi);
    for (int i = 0; i < repeat; i++) {
        kick(pipe, key, target(T_KICK_SEND).method);
        if (permanent && remove)
            remove(pipe, key, target(T_GUEST_REMOVE_SEND).method);
    }
    stopSpoof();
    return true;
}

// The guest name is read from the guest account, the same source as the player
// list in the room. When the name cannot be read, the row falls back to the guest
// key so it stays usable.
void copyName(void* guest, char* output, size_t size, const char* backup) {
    void* account = fieldAccountGuest.field ? readPtr(guest, fieldAccountGuest) : nullptr;
    void* name = account && guestNameField.field ? readPtr(account, guestNameField) : nullptr;
    textFrom(name, output, size, backup);
    // Menu rows use tabs and newlines as separators, so control characters
    // in a player name are replaced with spaces.
    for (char* p = output; *p; p++) {
        if (static_cast<unsigned char>(*p) < 0x20)
            *p = ' ';
    }
}

// Number of guests in the live list. Zero means the room does not exist yet, and
// in that state the host key and the player key must not be asked from the game:
// both getters throw NullReferenceException while the room is not formed, and an
// exception escaping a hook kills the process.
int32_t guestsInRoom(void* managerGuest) {
    void* manager = readPtr(managerGuest, fieldManagerGuest);
    void* list = readPtr(manager, fieldGuestManager);
    void* item = readPtr(list, fieldItemGuest);
    int32_t count = item ? readInt(list, guestSizeField) : 0;
    int32_t capacity = item ? lengthArray(item) : 0;
    return count > 0 && count <= capacity ? count : 0;
}

// Copy of the player list for the menu, refreshed on the game thread. The host
// and the local player are shown too, with their role, but the menu only offers a
// kick for guests: kicking either of them ends the room.
void refreshList(void* managerGuest) {
    if (!player::serveList())
        return;

    void* manager = readPtr(managerGuest, fieldManagerGuest);
    void* list = readPtr(manager, fieldGuestManager);
    int32_t count = guestsInRoom(managerGuest);

    // Outside a room the list is empty, and both keys are not queried at all.
    void* multi = count > 0 ? readPtr(manager, fieldMultiManager) : nullptr;
    void* host = multi ? hostKeyFrom(multi) : nullptr;
    void* own = count > 0 ? safePlayerKey(readPtr(manager, fieldPlayerManager)) : nullptr;

    player::Summary fresh[player::kMaxPlayer] = {};
    void* item = readPtr(list, fieldItemGuest);
    char* data = item ? arrayData(item) : nullptr;
    int filled = 0;
    for (int32_t i = 0; data && i < count && filled < player::kMaxPlayer; i++) {
        void* guest = *reinterpret_cast<void**>(data + i * sizeof(void*));
        if (!guest)
            continue;
        void* key = readPtr(guest, guestKeyField);
        textFrom(key, fresh[filled].key, player::kCharacterKey, "");
        if (!fresh[filled].key[0])
            continue;
        copyName(guest, fresh[filled].name, player::kCharacterName, fresh[filled].key);
        if (sameString(key, own))
            snprintf(fresh[filled].role, sizeof(fresh[filled].role), "kamu");
        else if (sameString(key, host))
            snprintf(fresh[filled].role, sizeof(fresh[filled].role), "host");
        else
            snprintf(fresh[filled].role, sizeof(fresh[filled].role), "tamu");
        filled++;
    }

    player::storeList(fresh, filled);
}

constexpr int kBurstHost = 24;

// Kick all sends a permanent kick to every guest in one tick. The host
// and the local player are skipped so the room is not closed as well.
int kickAllGuests(void* managerGuest) {
    int32_t count = guestsInRoom(managerGuest);
    if (count <= 0)
        return 0;

    void* manager = readPtr(managerGuest, fieldManagerGuest);
    void* list = readPtr(manager, fieldGuestManager);
    void* item = readPtr(list, fieldItemGuest);
    char* data = item ? arrayData(item) : nullptr;
    void* multi = readPtr(manager, fieldMultiManager);
    void* host = multi ? hostKeyFrom(multi) : nullptr;
    void* own = safePlayerKey(readPtr(manager, fieldPlayerManager));

    int sent = 0;
    for (int32_t i = 0; data && i < count; i++) {
        void* guest = *reinterpret_cast<void**>(data + i * sizeof(void*));
        if (!guest)
            continue;
        void* key = readPtr(guest, guestKeyField);
        if (!key || sameString(key, host) || sameString(key, own))
            continue;
        if (kickKey(multi, key, true, 1))
            sent++;
    }
    return sent;
}

void runAction(void* managerGuest, const player::Action& action) {
    char message[player::kCharacterName + 64];
    if (action.type == player::Action::Kind::Kick ||
        action.type == player::Action::Kind::KickPermanent) {
        // A leftover action from a previous room must not send packets: the host
        // key only exists while the room is running.
        if (guestsInRoom(managerGuest) <= 0) {
            LOGE("Menu kick skipped: room no longer exists");
            return;
        }
        bool permanent = action.type == player::Action::Kind::KickPermanent;
        void* multi = readPtr(readPtr(managerGuest, fieldManagerGuest), fieldMultiManager);
        bool sent = multi && kickKey(multi, stringFromUtf8(action.key), permanent, 1);
        if (sent) {
            snprintf(message, sizeof(message),
                     permanent ? "Kick permanen terkirim: %s" : "Kick sementara terkirim: %s",
                     action.name);
            LOGI("%s menu: %s", permanent ? "Permanent kick" : "Temporary kick", action.key);
        } else {
            snprintf(message, sizeof(message), "Kick gagal: %s", action.name);
            LOGE("Menu kick failed: %s", action.key);
        }
        showToast(message);
        return;
    }

    if (action.type == player::Action::Kind::KickHost) {
        if (guestsInRoom(managerGuest) <= 0) {
            LOGE("Host kick skipped: room no longer exists");
            return;
        }
        void* manager = readPtr(managerGuest, fieldManagerGuest);
        void* multi = readPtr(manager, fieldMultiManager);
        void* host = multi ? hostKeyFrom(multi) : nullptr;
        void* own = safePlayerKey(readPtr(manager, fieldPlayerManager));
        if (!multi || !host) {
            showToast("Kick host gagal: kunci host tidak terbaca");
            LOGE("Host kick failed: host key unreadable");
            return;
        }
        if (sameString(host, own)) {
            showToast("Kamu host di room ini");
            LOGI("Host kick skipped: the local player is the host");
            return;
        }
        // Repeated packets keep the host busy processing kicks for itself;
        // some builds eventually close the room on the host side.
        bool sent = kickKey(multi, host, true, kBurstHost);
        snprintf(message, sizeof(message), sent ? "Kick host dikirim: %d paket" : "Kick host gagal",
                 kBurstHost);
        if (sent)
            LOGI("Host kick: %d packets", kBurstHost);
        else
            LOGE("Host kick failed to send packets");
        showToast(message);
        return;
    }

    if (action.type == player::Action::Kind::KickAll) {
        if (guestsInRoom(managerGuest) <= 0) {
            showToast("Tidak ada tamu di room ini");
            LOGE("Kick all skipped: room no longer exists");
            return;
        }
        int count = kickAllGuests(managerGuest);
        snprintf(message, sizeof(message), "Kick semua: %d tamu", count);
        LOGI("Kick all: %d guests", count);
        showToast(message);
        return;
    }

    if (action.type == player::Action::Kind::CopyAvatar) {
        int slot = 0;
        CopyResult result = copyGuestPortrait(managerGuest, action.key, slot);
        switch (result) {
        case CopyResult::Saved:
            snprintf(message, sizeof(message), "Avatar %s tersimpan di slot %d", action.name, slot);
            break;
        case CopyResult::Exists:
            snprintf(message, sizeof(message), "Avatar %s sudah ada di slot %d", action.name, slot);
            break;
        case CopyResult::Missing:
            snprintf(message, sizeof(message), "Avatar %s belum tercatat, coba lagi", action.name);
            break;
        default:
            snprintf(message, sizeof(message), "Copy avatar %s gagal", action.name);
            break;
        }
        showToast(message);
        return;
    }

    int slot = 0;
    CopyResult result = copyGuestModel(managerGuest, action.key, action.name, slot);
    switch (result) {
    case CopyResult::Saved:
        snprintf(message, sizeof(message), "Skin %s tersimpan di slot %d", action.name, slot);
        break;
    case CopyResult::Exists:
        snprintf(message, sizeof(message), "Skin %s sudah ada di slot %d", action.name, slot);
        break;
    case CopyResult::Missing:
        snprintf(message, sizeof(message), "Skin %s belum tercatat, coba lagi", action.name);
        break;
    default:
        snprintf(message, sizeof(message), "Copy skin %s gagal", action.name);
        break;
    }
    showToast(message);
}

// One action per tick. A kick sends two packets, so two kicks in one
// frame only let the first through.
void tickAction(void* managerGuest) {
    player::Action action = {};
    if (!player::takeAction(action))
        return;
    runAction(managerGuest, action);
}

void hookUpdate(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_GUEST_UPDATE).orig);
    if (original)
        original(own, method);
    // Game calls that leave through the send path skip the matching stop,
    // and a leaked spoof makes every following key lookup answer with the
    // host key.
    stopSpoof();
    tickAction(own);
    refreshList(own);
}

// The guest list fields are used by the Room section and the kick button, so they
// are kept separate from the chat fields: a change in one does not disable the other.
bool findFieldMenu() {
    if (!findField(classOf("Guest__Manager"), "a", fieldManagerGuest))
        return false;
    void* managerClass = fieldClass(fieldManagerGuest);
    if (!findField(managerClass, "guests", fieldGuestManager) ||
        !findField(managerClass, "multi", fieldMultiManager) ||
        !findField(managerClass, "player", fieldPlayerManager))
        return false;

    void* klassList = fieldClass(fieldGuestManager);
    if (!findField(klassList, "_items", fieldItemGuest) ||
        !findField(klassList, "_size", guestSizeField))
        return false;
    void* klassGuest = fieldElementClass(fieldItemGuest);
    if (!findField(klassGuest, "key", guestKeyField))
        return false;
    // The name is only used by the player list in the menu. Without this field the
    // list still works with the guest key as its label.
    if (findField(klassGuest, "account", fieldAccountGuest) &&
        findField(fieldClass(fieldAccountGuest), "user_Name", guestNameField))
        LOGI("Guest name field ready");
    else
        LOGE("Guest name field not found, using the guest key");
    if (!findField(fieldClass(fieldMultiManager), "transfer", fieldTransferMulti))
        return false;
    return findField(fieldClass(fieldTransferMulti), "pipe", fieldPipeTransfer);
}

} // namespace

void installRoomHook() {
    readyMenu = findFieldMenu() && target(T_GUEST_UPDATE).addr && target(T_KICK_SEND).addr &&
                target(T_HOST_KEY).addr && target(T_PLAYER_KEY).orig;
    player::setReady(readyMenu);
    if (!readyMenu) {
        LOGE("Player list field not found; Room section inactive");
        return;
    }
    LOGI("Player list field ready");
    // One guest update hook drives the action queue and the list copy, so there
    // is only one hook for the whole section.
    installHook(target(T_GUEST_UPDATE), reinterpret_cast<void*>(hookUpdate));
}
