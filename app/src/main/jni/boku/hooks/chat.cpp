#include "hooks.h"

#include <cstdint>

#include "feature_flags.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

using FnReceive = void (*)(void*, void*, void*, void*, void*);
using FnChatRoom = void (*)(void*, void*, void*, void*);

// Every chat row is redrawn on each message, so long content is cut at this
// many UTF-16 units instead of being copied whole.
constexpr size_t kLimitCharacterRow = 512;
constexpr size_t kLimitByteName = 64;
constexpr size_t kLimitByteLog = 160;

Field fieldManager = {};
Field fieldChat = {};
Field fieldPlayer = {};
Field guestListField = {};
Field fieldItemGuest = {};
Field guestSizeField = {};
Field guestKeyField = {};
Field fieldAccountGuest = {};
Field fieldNameUser = {};
bool chatReady = false;

// The payload can carry control bytes, which would split one log line into
// two, so those bytes are flattened to spaces.
void flatten(char* text) {
    for (; *text; text++) {
        if (static_cast<unsigned char>(*text) < 0x20)
            *text = ' ';
    }
}

const char* utf8From(void* text, char* output, size_t size) {
    if (!toUtf8(text, output, size)) {
        output[0] = '\0';
        return output;
    }
    flatten(output);
    return output;
}

void recordMessagePrivate(void* user, void* receiver, void* content) {
    char textUser[kLimitByteName];
    char textReceiver[kLimitByteName];
    char contentText[kLimitByteLog];
    LOGI("Private message: %s -> %s: %s", utf8From(user, textUser, sizeof(textUser)),
         utf8From(receiver, textReceiver, sizeof(textReceiver)),
         utf8From(content, contentText, sizeof(contentText)));
}

// Private messages carry a key, not a display name. The guest list uses the
// same key, so the name is looked up there; an unknown key means the message
// came from outside this room and cannot be labeled.
void* guestName(void* own, void* key) {
    void* manager = readPtr(own, fieldManager);
    void* guestList = readPtr(manager, guestListField);
    void* item = readPtr(guestList, fieldItemGuest);
    if (!item)
        return nullptr;

    int32_t count = readInt(guestList, guestSizeField);
    int32_t capacity = lengthArray(item);
    if (count > capacity)
        count = capacity;

    char* data = arrayData(item);
    for (int32_t i = 0; i < count; i++) {
        void* guest = *reinterpret_cast<void**>(data + i * sizeof(void*));
        if (!guest || !sameString(readPtr(guest, guestKeyField), key))
            continue;

        void* account = readPtr(guest, fieldAccountGuest);
        void* name = readPtr(account, fieldNameUser);
        if (name && lengthString(name) > 0)
            return name;
    }
    return nullptr;
}

// The chat room already labels the sender, so only the receiver is placed in
// front of the content. The prefix is ASCII and the rest stays UTF-16.
void* privateRow(void* name, void* text) {
    const uint16_t* content = characterString(text);
    int32_t lengthContent = lengthString(text);
    if (!content || lengthContent <= 0)
        return nullptr;

    const uint16_t* characterName = name ? characterString(name) : nullptr;
    int32_t lengthName = characterName ? lengthString(name) : 0;
    bool named = lengthName > 0;

    uint16_t row[kLimitCharacterRow];
    size_t used = 0;
    for (const char* p = named ? "[PM -> " : "[PM] "; *p && used < kLimitCharacterRow; p++)
        row[used++] = static_cast<uint16_t>(*p);
    for (int32_t i = 0; named && i < lengthName && used < kLimitCharacterRow; i++)
        row[used++] = characterName[i];
    if (named) {
        for (const char* p = "] "; *p && used < kLimitCharacterRow; p++)
            row[used++] = static_cast<uint16_t>(*p);
    }
    for (int32_t i = 0; i < lengthContent && used < kLimitCharacterRow; i++)
        row[used++] = content[i];
    return stringUtf16New(row, static_cast<int32_t>(used));
}

// The receiver check inside the game compares this exact value, so the same
// call decides whether a message already has its own window. The game getter
// throws NullReferenceException while the photon room does not exist yet, so
// the key is only read through the safe path.
void* nameLocal(void* own) {
    void* manager = readPtr(own, fieldManager);
    return safePlayerKey(readPtr(manager, fieldPlayer));
}

// Private messages are broadcast to every client and only the receiver check
// drops them, so the text is already here. The message is recorded, then replayed
// into the chat room under the sender's name with the receiver in the prefix and
// without its own packet. The chat room resolves that name from the guest list,
// so a sender not in this room is skipped instead of being handed to the game.
void hookReceive(void* own, void* receiver, void* user, void* text, void* method) {
    auto original = reinterpret_cast<FnReceive>(target(T_GUEST_MSG_RECEIVE).orig);
    if (!features().privateChat) {
        if (original)
            original(own, receiver, user, text, method);
        return;
    }
    recordMessagePrivate(user, receiver, text);

    if (original)
        original(own, receiver, user, text, method);

    auto chatRoom = reinterpret_cast<FnChatRoom>(target(T_CHAT_MSG_RECEIVE).addr);
    if (!chatReady || !chatRoom)
        return;

    void* owned = nameLocal(own);
    if (owned && sameString(owned, receiver))
        return;

    void* pengirim = guestName(own, user);
    if (!pengirim) {
        char keyText[kLimitByteName];
        LOGI("Private message skipped, sender not in room: %s",
             utf8From(user, keyText, sizeof(keyText)));
        return;
    }

    void* chat = readPtr(own, fieldChat);
    void* row = privateRow(guestName(own, receiver), text);
    if (chat && row)
        chatRoom(chat, user, row, target(T_CHAT_MSG_RECEIVE).method);
}

bool findFieldGuest() {
    return findField(fieldClass(fieldManager), "guests", guestListField) &&
           findField(fieldClass(guestListField), "_items", fieldItemGuest) &&
           findField(fieldClass(guestListField), "_size", guestSizeField) &&
           findField(fieldElementClass(fieldItemGuest), "key", guestKeyField) &&
           findField(fieldElementClass(fieldItemGuest), "account", fieldAccountGuest) &&
           findField(fieldClass(fieldAccountGuest), "user_Name", fieldNameUser);
}

} // namespace

void installChatHook() {
    void* klassChat = classOf("Net__Multi__Guest__Chat");
    chatReady = findField(klassChat, "chat", fieldChat);
    bool readyNameOwn = findField(klassChat, "a", fieldManager) &&
                        findField(fieldClass(fieldManager), "player", fieldPlayer) &&
                        target(T_PLAYER_KEY).addr;
    bool readyGuest = findFieldGuest();

    if (chatReady && readyNameOwn)
        LOGI("Chat field ready");
    else if (!chatReady)
        LOGE("Chat field not found; private messages only go to the log");
    else
        LOGE("Own name not found; messages for this player appear twice");

    if (!readyGuest)
        LOGE("Guest name not found; private messages appear without the receiver name");

    installHook(target(T_GUEST_MSG_RECEIVE), reinterpret_cast<void*>(hookReceive));
}
