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

void textFrom(void* text, char* keluaran, size_t size, const char* backup) {
    keluaran[0] = '\0';
    if (text && toUtf8(text, keluaran, size) && keluaran[0])
        return;
    snprintf(keluaran, size, "%s", backup);
}

void* pipeFrom(void* multi) {
    return readPtr(readPtr(multi, fieldTransferMulti), fieldPipeTransfer);
}

// Tombol tendang bawaan mengirim dua paket: tendangan membuat tamu menampilkan
// pemberitahuan, dan penghapusan tamu mengeluarkannya dari setiap daftar.
// Keduanya membawa kunci pengirim, jadi penggantian yang sama membuat klien
// menerimanya sebagai paket host. Tendangan sementara hanya memakai paket
// pertama; tendangan permanen memakai keduanya.
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

// Nama tamu dibaca dari akun guest, sumber yang sama dengan daftar pemain di
// room. Bila nama tidak terbaca, barisnya memakai kunci tamu supaya tetap bisa
// dipakai.
void copyName(void* guest, char* keluaran, size_t size, const char* backup) {
    void* account = fieldAccountGuest.field ? readPtr(guest, fieldAccountGuest) : nullptr;
    void* name = account && guestNameField.field ? readPtr(account, guestNameField) : nullptr;
    textFrom(name, keluaran, size, backup);
    // Baris menu memakai tab dan newline sebagai pemisah, jadi karakter kendali
    // di nama pemain diganti spasi.
    for (char* p = keluaran; *p; p++) {
        if (static_cast<unsigned char>(*p) < 0x20)
            *p = ' ';
    }
}

// Jumlah tamu di daftar hidup. Nol berarti belum ada room, dan pada keadaan itu
// kunci host serta kunci pemain tidak boleh ditanyakan ke game: kedua getter
// melempar NullReferenceException saat room belum terbentuk, dan eksepsi yang
// lolos dari hook mematikan proses.
int32_t guestsInRoom(void* managerGuest) {
    void* manager = readPtr(managerGuest, fieldManagerGuest);
    void* list = readPtr(manager, fieldGuestManager);
    void* item = readPtr(list, fieldItemGuest);
    int32_t count = item ? readInt(list, guestSizeField) : 0;
    int32_t kapasitas = item ? lengthArray(item) : 0;
    return count > 0 && count <= kapasitas ? count : 0;
}

// Salinan daftar pemain untuk menu, diperbarui di thread game. Host dan pemain
// sendiri ikut ditampilkan dengan perannya, tetapi menu hanya menawarkan tendang
// untuk tamu: menendang keduanya mengakhiri room.
void refreshList(void* managerGuest) {
    if (!player::serveList())
        return;

    void* manager = readPtr(managerGuest, fieldManagerGuest);
    void* list = readPtr(manager, fieldGuestManager);
    int32_t count = guestsInRoom(managerGuest);

    // Di luar room daftar kosong, dan kedua kunci tidak ditanyakan sama sekali.
    void* multi = count > 0 ? readPtr(manager, fieldMultiManager) : nullptr;
    void* host = multi ? hostKeyFrom(multi) : nullptr;
    void* own = count > 0 ? safePlayerKey(readPtr(manager, fieldPlayerManager)) : nullptr;

    player::Summary fresh[player::kMaxPlayer] = {};
    void* item = readPtr(list, fieldItemGuest);
    char* data = item ? arrayData(item) : nullptr;
    int terisi = 0;
    for (int32_t i = 0; data && i < count && terisi < player::kMaxPlayer; i++) {
        void* guest = *reinterpret_cast<void**>(data + i * sizeof(void*));
        if (!guest)
            continue;
        void* key = readPtr(guest, guestKeyField);
        textFrom(key, fresh[terisi].key, player::kCharacterKey, "");
        if (!fresh[terisi].key[0])
            continue;
        copyName(guest, fresh[terisi].name, player::kCharacterName, fresh[terisi].key);
        if (sameString(key, own))
            snprintf(fresh[terisi].role, sizeof(fresh[terisi].role), "kamu");
        else if (sameString(key, host))
            snprintf(fresh[terisi].role, sizeof(fresh[terisi].role), "host");
        else
            snprintf(fresh[terisi].role, sizeof(fresh[terisi].role), "tamu");
        terisi++;
    }

    player::storeList(fresh, terisi);
}

constexpr int kBurstHost = 24;

// Kick semua mengirim tendangan permanen ke setiap tamu pada satu detak. Host
// dan pemain sendiri dilewati supaya room tidak ikut ditutup.
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

    int terkirim = 0;
    for (int32_t i = 0; data && i < count; i++) {
        void* guest = *reinterpret_cast<void**>(data + i * sizeof(void*));
        if (!guest)
            continue;
        void* key = readPtr(guest, guestKeyField);
        if (!key || sameString(key, host) || sameString(key, own))
            continue;
        if (kickKey(multi, key, true, 1))
            terkirim++;
    }
    return terkirim;
}

void runAction(void* managerGuest, const player::Action& action) {
    char message[player::kCharacterName + 64];
    if (action.jenis == player::Action::Kind::Kick ||
        action.jenis == player::Action::Kind::KickPermanent) {
        // Aksi sisa dari room sebelumnya tidak boleh mengirim paket: kunci host
        // hanya ada selama room berjalan.
        if (guestsInRoom(managerGuest) <= 0) {
            LOGE("Tendang menu dilewati: room sudah tidak ada");
            return;
        }
        bool permanent = action.jenis == player::Action::Kind::KickPermanent;
        void* multi = readPtr(readPtr(managerGuest, fieldManagerGuest), fieldMultiManager);
        bool terkirim = multi && kickKey(multi, stringFromUtf8(action.key), permanent, 1);
        if (terkirim) {
            snprintf(message, sizeof(message),
                     permanent ? "Kick permanen terkirim: %s" : "Kick sementara terkirim: %s",
                     action.name);
            LOGI("%s menu: %s", permanent ? "Kick permanen" : "Kick sementara", action.key);
        } else {
            snprintf(message, sizeof(message), "Kick gagal: %s", action.name);
            LOGE("Kick menu gagal: %s", action.key);
        }
        showToast(message);
        return;
    }

    if (action.jenis == player::Action::Kind::KickHost) {
        if (guestsInRoom(managerGuest) <= 0) {
            LOGE("Kick host dilewati: room sudah tidak ada");
            return;
        }
        void* manager = readPtr(managerGuest, fieldManagerGuest);
        void* multi = readPtr(manager, fieldMultiManager);
        void* host = multi ? hostKeyFrom(multi) : nullptr;
        void* own = safePlayerKey(readPtr(manager, fieldPlayerManager));
        if (!multi || !host) {
            showToast("Kick host gagal: kunci host tidak terbaca");
            LOGE("Kick host gagal: kunci host tidak terbaca");
            return;
        }
        if (sameString(host, own)) {
            showToast("Kamu host di room ini");
            LOGI("Kick host dilewati: pemain sendiri adalah host");
            return;
        }
        // Paket berulang membuat host sibuk memproses tendangan untuk dirinya
        // sendiri; sebagian build akhirnya menutup room dari sisi host.
        bool terkirim = kickKey(multi, host, true, kBurstHost);
        snprintf(message, sizeof(message), terkirim ? "Kick host dikirim: %d paket" : "Kick host gagal",
                 kBurstHost);
        if (terkirim)
            LOGI("Kick host: %d paket", kBurstHost);
        else
            LOGE("Kick host gagal mengirim paket");
        showToast(message);
        return;
    }

    if (action.jenis == player::Action::Kind::KickAll) {
        if (guestsInRoom(managerGuest) <= 0) {
            showToast("Tidak ada tamu di room ini");
            LOGE("Kick semua dilewati: room sudah tidak ada");
            return;
        }
        int count = kickAllGuests(managerGuest);
        snprintf(message, sizeof(message), "Kick semua: %d tamu", count);
        LOGI("Kick semua: %d tamu", count);
        showToast(message);
        return;
    }

    int slot = 0;
    CopyResult result = copyGuestModel(managerGuest, action.key, action.name, slot);
    switch (result) {
    case CopyResult::Saved:
        snprintf(message, sizeof(message), "Skin %s disalin ke slot %d", action.name, slot);
        break;
    case CopyResult::Exists:
        snprintf(message, sizeof(message), "Skin %s sudah ada di slot %d", action.name, slot);
        break;
    case CopyResult::Missing:
        snprintf(message, sizeof(message), "Skin %s belum tercatat, coba lagi", action.name);
        break;
    default:
        snprintf(message, sizeof(message), "Salin skin %s gagal", action.name);
        break;
    }
    showToast(message);
}

// Satu aksi per detak. tendang mengirim dua paket, jadi dua tendangan dalam satu
// frame hanya akan meloloskan yang pertama.
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
    // Panggilan game yang keluar lewat jalur kirim akan melewati henti yang
    // berpasangan, dan penggantian yang bocor membuat setiap pencarian kunci
    // berikutnya dijawab dengan kunci host.
    stopSpoof();
    tickAction(own);
    refreshList(own);
}

// Field daftar tamu dipakai section Room dan tombol tendang, jadi dipisahkan
// dari field chat: perubahan pada salah satunya tidak mematikan yang lain.
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
    // Nama hanya dipakai daftar pemain di menu. Tanpa field ini daftar tetap
    // berjalan dengan kunci tamu sebagai label.
    if (findField(klassGuest, "account", fieldAccountGuest) &&
        findField(fieldClass(fieldAccountGuest), "user_Name", guestNameField))
        LOGI("Field nama tamu siap");
    else
        LOGE("Field nama tamu tidak ditemukan, memakai kunci tamu");
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
        LOGE("Field daftar pemain tidak ditemukan; section Room tidak aktif");
        return;
    }
    LOGI("Field daftar pemain siap");
    // Satu hook update tamu menggerakkan antrean aksi dan salinan daftar, jadi
    // hanya ada satu hook untuk seluruh section.
    installHook(target(T_GUEST_UPDATE), reinterpret_cast<void*>(hookUpdate));
}
