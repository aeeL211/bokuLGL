#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldDetail = {};
Field fieldPassword = {};
Field fieldStr = {};
Field fieldDataRoom = {};
Field fieldPasswordRoom = {};

// Sandi room hanya beberapa karakter. Yang lebih panjang dari ini adalah objek
// basi yang header-nya dipakai ulang, bukan string sungguhan, dan meneruskannya
// ke keyboard membuat Unity memindai melewati ujung heap.
constexpr int32_t kLimitPassword = 4096;

bool stringLayak(void* str) {
    if (!str)
        return false;
    int32_t length = lengthString(str);
    return length >= 0 && length <= kLimitPassword;
}

void* stringKosong() { return stringUtf16New(reinterpret_cast<const uint16_t*>(u""), 0); }

// Data room memiliki sandi room, jadi membacanya saat dibutuhkan menjaga setiap
// pemakai nilai itu pada objek hidup yang sama.
void* roomPassword(void* self) {
    void* detail = readPtr(self, fieldDetail);
    void* dataRoom = readPtr(detail, fieldDataRoom);
    return readPtr(dataRoom, fieldPasswordRoom);
}

// Term__Bottom_Button() adalah status aktif tombol OK: fungsi itu melaporkan
// apakah _password.str berbeda dari string kosong. Input__Ok() langsung kembali
// bila hasilnya false, dan tombolnya sendiri diredupkan, sehingga kotak kosong
// tidak pernah bisa dikirim dan perbandingannya tidak pernah tercapai. Dengan
// hook ini tombolnya selalu bisa dipakai; sandi room tetap diperiksa kode asli,
// yang dipenuhi okHook tepat sebelum perbandingan.
bool hookButton(void* self, void* method) {
    if (!features().roomPassword) {
        auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_PASSWORD_BUTTON).orig);
        return original ? original(self, method) : false;
    }
    return true;
}

// Input__Ok membandingkan _password.str dengan detail._room_Data.password dan
// baru setelah itu menutup keyboard serta memanggil Join__Start. Kotak tetap
// kosong, jadi objek string milik room itu sendiri dipasang untuk perbandingan:
// String.Equals mengembalikan true pada referensi yang sama sebelum membaca satu
// karakter pun, dan data room menjaga objeknya tetap hidup.
void hookOk(void* self, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_PASSWORD_OK).orig);
    if (!original)
        return;
    if (!features().roomPassword) {
        original(self, method);
        return;
    }
    void* value = readPtr(self, fieldPassword);
    void* password = roomPassword(self);
    if (value && stringLayak(password)) {
        setRef(value, fieldStr, password);
        // Perbandingan membaca field ini setelah fungsi asli berjalan, jadi
        // penulisan yang tidak mendarat adalah satu-satunya kegagalan yang harus
        // terlihat jelas di log berikutnya.
        void* saved = readPtr(value, fieldStr);
        if (saved == password)
            LOGI("Sandi: sandi room terpasang, panjang=%d", lengthString(password));
        else
            LOGE("Sandi: sandi room gagal ditulis, ada=%p seharusnya=%p", saved, password);
    } else {
        LOGE("Sandi: sandi room tidak ada, perbandingan berjalan apa adanya");
    }
    // Mencapai baris ini berarti perbandingannya gagal: jalur yang lolos
    // memanggil Join__Start sebagai tail call dan tidak pernah kembali.
    original(self, method);
    LOGI("Sandi: perbandingan gerbang gagal, hookOk kembali");
}

// Jalur keyboard meneruskan _password.str sebagai teks awal, jadi null di sana
// membatalkan sebelum field terbuka. _Start biasanya menulis string kosong;
// setelah join, panel bisa masih memegang string yang dipakai ulang. Sandi room
// dipasang kembali untuk kasus itu, karena objeknya dimiliki data room dan tetap
// hidup selama keyboard membacanya, sedangkan alokasi baru bisa dikumpulkan
// terlalu cepat.
void hookField(void* self, bool touch, void* method) {
    auto original = reinterpret_cast<void (*)(void*, bool, void*)>(target(T_PASSWORD_FIELD).orig);
    if (!original)
        return;
    if (!features().roomPassword) {
        original(self, touch, method);
        return;
    }
    void* value = readPtr(self, fieldPassword);
    void* str = value ? readPtr(value, fieldStr) : nullptr;
    // Panel menyerahkan string ini ke keyboard, jadi catat sebelum fungsi asli
    // berjalan: pointer basi di sini yang kemudian membuat engine tersedak.
    LOGI("Field sandi: nilai=%p str=%p panjang=%d", value, str, lengthString(str));
    if (value && !stringLayak(str)) {
        void* password = roomPassword(self);
        void* initial = stringLayak(password) ? password : stringKosong();
        if (initial)
            setRef(value, fieldStr, initial);
    }
    original(self, touch, method);
}

} // namespace

void installPasswordHook() {
    void* klassPassword = classOf("UI__Menu__Net__Multi__Join__Room_List__Detail__Password");
    if (klassPassword && findField(klassPassword, "detail", fieldDetail) &&
        findField(klassPassword, "_password", fieldPassword) &&
        findField(fieldClass(fieldPassword), "str", fieldStr) &&
        findField(fieldClass(fieldDetail), "_room_Data", fieldDataRoom) &&
        findField(fieldClass(fieldDataRoom), "password", fieldPasswordRoom)) {
        LOGI("Field sandi room OK");
        LOGI("Offset sandi: _password=%zu str=%zu room_Data=%zu room_password=%zu",
             fieldPassword.offset, fieldStr.offset, fieldDataRoom.offset, fieldPasswordRoom.offset);
    } else {
        LOGE("Field sandi room tidak ditemukan; room bersandi tetap manual");
    }

    installHook(target(T_PASSWORD_OK), reinterpret_cast<void*>(hookOk));
    installHook(target(T_PASSWORD_FIELD), reinterpret_cast<void*>(hookField));
    installHook(target(T_PASSWORD_BUTTON), reinterpret_cast<void*>(hookButton));
}
