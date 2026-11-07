#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

using FnReceive = void (*)(void*, void*, void*, void*, void*);
using FnChatRoom = void (*)(void*, void*, void*, void*);

// Setiap baris chat digambar ulang pada tiap pesan, jadi isi yang panjang
// dipotong pada sekian unit UTF-16 ini daripada disalin utuh.
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

// Payload bisa membawa byte kontrol, yang akan membelah satu baris log menjadi
// dua, jadi byte itu diratakan menjadi spasi.
void ratakan(char* text) {
    for (; *text; text++) {
        if (static_cast<unsigned char>(*text) < 0x20)
            *text = ' ';
    }
}

const char* utf8From(void* text, char* keluaran, size_t size) {
    if (!toUtf8(text, keluaran, size)) {
        keluaran[0] = '\0';
        return keluaran;
    }
    ratakan(keluaran);
    return keluaran;
}

void recordMessagePrivate(void* user, void* receiver, void* content) {
    char textUser[kLimitByteName];
    char textReceiver[kLimitByteName];
    char contentText[kLimitByteLog];
    LOGI("Pesan pribadi: %s -> %s: %s", utf8From(user, textUser, sizeof(textUser)),
         utf8From(receiver, textReceiver, sizeof(textReceiver)),
         utf8From(content, contentText, sizeof(contentText)));
}

// Pesan pribadi membawa kunci, bukan nama tampilan. Daftar tamu memakai kunci
// yang sama, jadi namanya dicari di sana; kunci yang tidak dikenal berarti pesan
// itu datang dari luar room ini dan tidak bisa diberi label.
void* guestName(void* own, void* key) {
    void* manager = readPtr(own, fieldManager);
    void* guestList = readPtr(manager, guestListField);
    void* item = readPtr(guestList, fieldItemGuest);
    if (!item)
        return nullptr;

    int32_t count = readInt(guestList, guestSizeField);
    int32_t kapasitas = lengthArray(item);
    if (count > kapasitas)
        count = kapasitas;

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

// Room chat sudah memberi label pengirim, jadi hanya penerima yang ditaruh di
// depan isi. Prefiksnya ASCII dan sisanya tetap UTF-16.
void* privateRow(void* name, void* text) {
    const uint16_t* content = characterString(text);
    int32_t lengthContent = lengthString(text);
    if (!content || lengthContent <= 0)
        return nullptr;

    const uint16_t* characterName = name ? characterString(name) : nullptr;
    int32_t lengthName = characterName ? lengthString(name) : 0;
    bool bernama = lengthName > 0;

    uint16_t row[kLimitCharacterRow];
    size_t terpakai = 0;
    for (const char* p = bernama ? "[PM -> " : "[PM] "; *p && terpakai < kLimitCharacterRow; p++)
        row[terpakai++] = static_cast<uint16_t>(*p);
    for (int32_t i = 0; bernama && i < lengthName && terpakai < kLimitCharacterRow; i++)
        row[terpakai++] = characterName[i];
    if (bernama) {
        for (const char* p = "] "; *p && terpakai < kLimitCharacterRow; p++)
            row[terpakai++] = static_cast<uint16_t>(*p);
    }
    for (int32_t i = 0; i < lengthContent && terpakai < kLimitCharacterRow; i++)
        row[terpakai++] = content[i];
    return stringUtf16New(row, static_cast<int32_t>(terpakai));
}

// Pengecekan penerima di dalam game membandingkan nilai persis ini, jadi
// panggilan yang sama memutuskan apakah sebuah pesan sudah punya jendela
// sendiri. Getter game melempar NullReferenceException saat photon room belum
// ada, jadi kunci hanya dibaca lewat jalur aman.
void* nameLocal(void* own) {
    void* manager = readPtr(own, fieldManager);
    return safePlayerKey(readPtr(manager, fieldPlayer));
}

// Pesan pribadi disiarkan ke setiap klien dan hanya pengecekan penerima yang
// membuangnya, jadi teksnya sudah ada di sini. Pesan itu dicatat, lalu diputar
// ulang ke room chat atas nama pengirim dengan penerima di prefiks dan tanpa
// paket sendiri. Room chat menyelesaikan nama itu dari daftar tamu, jadi
// pengirim yang tidak ada di room ini dilewati daripada diserahkan ke game.
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
        LOGI("Pesan pribadi dilewati, pengirim tidak ada di room: %s",
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
        LOGI("Field chat siap");
    else if (!chatReady)
        LOGE("Field chat tidak ditemukan; pesan pribadi hanya masuk log");
    else
        LOGE("Nama sendiri tidak ditemukan; pesan untuk pemain ini muncul dua kali");

    if (!readyGuest)
        LOGE("Nama tamu tidak ditemukan; pesan pribadi tampil tanpa nama penerima");

    installHook(target(T_GUEST_MSG_RECEIVE), reinterpret_cast<void*>(hookReceive));
}
