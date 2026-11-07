#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field limitInput = {};
Field nameData = {};
Field nameLimit = {};

// Game menulis ulang UIInput.characterLimit (nama 38, hari 2, bulan 2, tahun 4)
// tepat sebelum setiap edit, jadi kedua titik pemotongan harus menimpanya:
// Insert(string) menegakkan batas per ketukan, Validate(string) memangkas nilai
// saat Set/kirim.
void* hookValidate(void* self, void* value, void* method) {
    auto original = reinterpret_cast<void* (*)(void*, void*, void*)>(target(T_INPUT_VALIDATE).orig);
    if (!original)
        return value;
    if (features().inputText)
        writeInt(self, limitInput, INT32_MAX);
    return original(self, value, method);
}

void hookInsert(void* self, void* text, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*, void*)>(target(T_INPUT_INSERT).orig);
    if (!original)
        return;
    if (features().inputText)
        writeInt(self, limitInput, INT32_MAX);
    original(self, text, method);
}

// Keyboard mengubah string ini ke UTF-8 dan menelusuri karakternya sampai
// panjang di header-nya. String yang header-nya tidak lagi menyimpan panjang
// wajar membuat penelusuran itu melewati heap, jadi serahkan string kosong baru
// alih-alih membiarkan engine memindai memori yang tidak terpetakan.
constexpr int32_t kLimitKeyboard = 4096;

void* stringKosong() { return stringUtf16New(reinterpret_cast<const uint16_t*>(u""), 0); }

// Parameter Length tidak terpakai di jalur mobile, tetapi jalur Steam/gamepad
// meneruskannya ke input teks miliknya sendiri. Nol berarti tanpa batas karakter
// di sana.
void hookOpenKeyboard(void* self, int32_t s, int32_t type, void* text, int32_t lengthRequested,
                      void* method) {
    auto original = reinterpret_cast<void (*)(void*, int32_t, int32_t, void*, int32_t, void*)>(
        target(T_KEYBOARD_OPEN).orig);
    if (!original)
        return;
    if (!features().inputText) {
        original(self, s, type, text, lengthRequested, method);
        return;
    }
    int32_t length = lengthString(text);
    // Menyebut string yang akan diubah engine, supaya pointer buruk muncul di log
    // tepat sebelum crash, bukan hanya di alamat kesalahan.
    LOGI("Keyboard: s=%d type=%d text=%p panjang=%d", s, type, text, length);
    if (!text || length < 0 || length > kLimitKeyboard) {
        void* kosong = stringKosong();
        if (kosong)
            text = kosong;
    }
    original(self, s, type, text, 0, method);
}

// Name__Check memangkas nama profil sesuai _name.length_Limit dan kembali ke
// nilai bawaan bila hasilnya kosong. Naikkan batas itu di sekitar panggilan
// supaya pemangkasannya lolos dan sisa pemeriksaannya tetap berjalan.
void hookCheckName(void* self, void** name, bool fix, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void**, bool, void*)>(target(T_NAME_CHECK).orig);
    if (!original)
        return;
    if (!features().inputText) {
        original(self, name, fix, method);
        return;
    }
    void* data = readPtr(self, nameData);
    int32_t saved = 0;
    if (data) {
        saved = readInt(data, nameLimit);
        writeInt(data, nameLimit, INT32_MAX);
    }
    original(self, name, fix, method);
    if (data)
        writeInt(data, nameLimit, saved);
}

} // namespace

void installTextInputHook() {
    if (!findField(classOf("UIInput"), "characterLimit", limitInput)) {
        LOGE("Field UIInput.characterLimit tidak ditemukan; batas input tetap bawaan");
    }

    void* klassAccount = classOf("Net__Account__Manager");
    if (findField(klassAccount, "_name", nameData) &&
        findField(fieldClass(nameData), "length_Limit", nameLimit)) {
        LOGI("Batas nama field OK");
    } else {
        LOGE("Batas nama field tidak ditemukan; nama profil tetap terbatas");
    }

    installHook(target(T_INPUT_VALIDATE), reinterpret_cast<void*>(hookValidate));
    installHook(target(T_INPUT_INSERT), reinterpret_cast<void*>(hookInsert));
    installHook(target(T_KEYBOARD_OPEN), reinterpret_cast<void*>(hookOpenKeyboard));
    installHook(target(T_NAME_CHECK), reinterpret_cast<void*>(hookCheckName));
}
