#include "hooks.h"

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Setiap baris detail akun menanyakan ini sebelum memutuskan apa yang ditampilkan.
// Menjawab ya membuat pilihan "sembunyikan" milik pemilik tidak menggantikan
// nilainya.
bool hookManager(void* self, int a, void* b, bool c, void* method) {
    if (!features().privacy) {
        auto original = reinterpret_cast<bool (*)(void*, int, void*, bool, void*)>(
            target(T_PRIVACY_MANAGER).orig);
        return original ? original(self, a, b, c, method) : false;
    }
    return true;
}

// Layar teman, blacklist, tambah teman, dan verifikasi membawa salinan
// pengecekan yang sama, jadi semuanya dijawab dengan cara yang sama.
template <TargetId id> bool hookRow(void* self, int a, void* method) {
    if (!features().privacy) {
        auto original = reinterpret_cast<bool (*)(void*, int, void*)>(target(id).orig);
        return original ? original(self, a, method) : false;
    }
    return true;
}

// Objek pengguna dan akun tamu menyimpan email serta nilai privasi yang
// menentukan boleh-tidaknya email itu dikirim dalam bentuk yang sama, jadi satu
// pencari field cukup untuk keduanya.
struct FieldEmail {
    Field email;
    Field privacy;
    Field emailPrivacy;
    Field terlihat;
};

FieldEmail userEmail = {};
FieldEmail emailGuest = {};
Field fieldSelectUser = {};
Field fieldManagerGuest = {};
Field guestListField = {};
Field fieldItemGuest = {};
Field guestSizeField = {};
Field guestKeyField = {};
Field fieldAccountGuest = {};
bool recordedContent = false;

using FnSet = void (*)(void*, void*, void*, void*, bool, void*);
using FnSetLocal = void (*)(void*, void*, void*, void*);
using FnPublic = void (*)(void*, void*, bool, void*, void*, void*, void*, void*, void*);
using FnPrivacy = void (*)(void*, void*, int, void*, void*);

// Kata yang dipakai game sendiri untuk sebuah nilai privasi email, jadi barisnya
// terbaca seperti layar pengaturan dan bukan kata buatan sendiri.
void* labelPrivacy(int terlihat) {
    auto fn = reinterpret_cast<void* (*)(int, void*)>(target(T_PRIVACY_LABEL).addr);
    return fn ? fn(terlihat, target(T_PRIVACY_LABEL).method) : nullptr;
}

// Email milik akun lain hanya ikut terkirim bila pemiliknya mengizinkan, jadi
// email yang disembunyikan membuat barisnya kosong. Label privasi milik pemilik
// ditampilkan sebagai gantinya.
void fillEmail(void* owner, const FieldEmail& field) {
    if (!features().privacy || !owner)
        return;
    void* email = readPtr(owner, field.email);
    if (email && lengthString(email) > 0)
        return;
    void* privacy = readPtr(owner, field.privacy);
    void* emailPrivacy = privacy ? readPtr(privacy, field.emailPrivacy) : nullptr;
    if (!emailPrivacy)
        return;
    void* label = labelPrivacy(readInt(emailPrivacy, field.terlihat));
    if (!label)
        return;
    setRef(owner, field.email, label);
    if (recordedContent)
        return;
    recordedContent = true;
    char text[64];
    if (toUtf8(label, text, sizeof(text)))
        LOGI("Label privasi email: %s", text);
}

bool findFieldEmail(FieldEmail& field, void* klass) {
    if (!klass)
        return false;
    return findField(klass, "email", field.email) && findField(klass, "privacy", field.privacy) &&
           findField(fieldClass(field.privacy), "email", field.emailPrivacy) &&
           findField(fieldClass(field.emailPrivacy), "visible", field.terlihat);
}

void fillUserEmail(void* refUser) {
    fillEmail(refUser ? *reinterpret_cast<void**>(refUser) : nullptr, userEmail);
}

void hookSet(void* own, void* refUser, void* uid, void* dokumen, bool timpa,
               void* method) {
    auto original = reinterpret_cast<FnSet>(target(T_USER_SET).orig);
    if (original)
        original(own, refUser, uid, dokumen, timpa, method);
    fillUserEmail(refUser);
}

// Ketiga penyetel lokal hanya berbeda pada tipe argumen keduanya.
void setLocal(TargetId id, void* own, void* refUser, void* data, void* method) {
    auto original = reinterpret_cast<FnSetLocal>(target(id).orig);
    if (original)
        original(own, refUser, data, method);
    fillUserEmail(refUser);
}

void hookSetString(void* own, void* refUser, void* data, void* method) {
    setLocal(T_USER_SET_LOCAL, own, refUser, data, method);
}

void hookSetArray(void* own, void* refUser, void* data, void* method) {
    setLocal(T_USER_SET_LOCAL_ARRAY, own, refUser, data, method);
}

void hookSetList(void* own, void* refUser, void* data, void* method) {
    setLocal(T_USER_SET_LOCAL_LIST, own, refUser, data, method);
}

// Manajer tidak menyimpan akun sendiri: setiap tamu di daftar room membawa satu,
// dengan kunci berupa teks yang sama dengan nama di paket profil. Akunnya karena
// itu dicari di daftar tersebut, dan kunci dari luar room tidak menyisakan apa
// pun untuk diisi.
void* guestAccount(void* own, void* user) {
    void* manager = readPtr(own, fieldManagerGuest);
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
        if (!guest || !sameString(readPtr(guest, guestKeyField), user))
            continue;
        return readPtr(guest, fieldAccountGuest);
    }
    return nullptr;
}

// Paket profil membangun akun tamu dan pesan privasi bisa mengganti emailnya
// lagi, jadi kedua jalur berakhir pada pengisian yang sama.
void hookPublic(void* own, void* user, bool login, void* uid, void* email, void* date,
              void* portrait, void* perkenalan, void* method) {
    auto original = reinterpret_cast<FnPublic>(target(T_GUEST_COMMON).orig);
    if (original)
        original(own, user, login, uid, email, date, portrait, perkenalan, method);
    fillEmail(guestAccount(own, user), emailGuest);
}

void hookEmailPrivacy(void* own, void* user, int terlihat, void* email, void* method) {
    auto original = reinterpret_cast<FnPrivacy>(target(T_GUEST_PRIVACY_EMAIL).orig);
    if (original)
        original(own, user, terlihat, email, method);
    fillEmail(guestAccount(own, user), emailGuest);
}

bool findFieldGuest() {
    void* klassManagerGuest = classOf("Guest__Manager");
    if (!findField(klassManagerGuest, "a", fieldManagerGuest))
        return false;
    return findField(fieldClass(fieldManagerGuest), "guests", guestListField) &&
           findField(fieldClass(guestListField), "_items", fieldItemGuest) &&
           findField(fieldClass(guestListField), "_size", guestSizeField) &&
           findField(fieldElementClass(fieldItemGuest), "key", guestKeyField) &&
           findField(fieldElementClass(fieldItemGuest), "account", fieldAccountGuest) &&
           findFieldEmail(emailGuest, fieldClass(fieldAccountGuest));
}

bool findFieldUser() {
    void* manager = classOf("Net__Account__Manager");
    return findField(manager, "user_Select", fieldSelectUser) &&
           findFieldEmail(userEmail, fieldClass(fieldSelectUser));
}

} // namespace

void installPrivacyHook() {
    if (findFieldUser())
        LOGI("Field privasi siap");
    else
        LOGE("Field privasi tidak ditemukan; email yang disembunyikan tetap kosong");
    if (!findFieldGuest())
        LOGE("Field akun tamu tidak ditemukan; email di dalam room tetap kosong");

    const TargetId setter[] = {T_USER_SET, T_USER_SET_LOCAL, T_USER_SET_LOCAL_ARRAY,
                                 T_USER_SET_LOCAL_LIST};
    int ditemukan = 0;
    for (TargetId id : setter)
        ditemukan += target(id).addr ? 1 : 0;
    LOGI("Penyetel email: %d dari 4", ditemukan);

    installHook(target(T_PRIVACY_MANAGER), reinterpret_cast<void*>(hookManager));
    installHook(target(T_PRIVACY_BLACKLIST),
               reinterpret_cast<void*>(hookRow<T_PRIVACY_BLACKLIST>));
    installHook(target(T_PRIVACY_ADD_FRIEND),
               reinterpret_cast<void*>(hookRow<T_PRIVACY_ADD_FRIEND>));
    installHook(target(T_PRIVACY_FRIEND_PROFILE),
               reinterpret_cast<void*>(hookRow<T_PRIVACY_FRIEND_PROFILE>));
    installHook(target(T_PRIVACY_VERIFY), reinterpret_cast<void*>(hookRow<T_PRIVACY_VERIFY>));
    installHook(target(T_PRIVACY_VERIFY_EMAIL),
               reinterpret_cast<void*>(hookRow<T_PRIVACY_VERIFY_EMAIL>));
    installHook(target(T_USER_SET), reinterpret_cast<void*>(hookSet));
    installHook(target(T_USER_SET_LOCAL), reinterpret_cast<void*>(hookSetString));
    installHook(target(T_USER_SET_LOCAL_ARRAY), reinterpret_cast<void*>(hookSetArray));
    installHook(target(T_USER_SET_LOCAL_LIST), reinterpret_cast<void*>(hookSetList));
    installHook(target(T_GUEST_COMMON), reinterpret_cast<void*>(hookPublic));
    installHook(target(T_GUEST_PRIVACY_EMAIL), reinterpret_cast<void*>(hookEmailPrivacy));
}
