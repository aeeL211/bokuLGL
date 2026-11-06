#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "hook.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldAkun = {};
Field fieldPerkenalan = {};
Field fieldPanjang = {};

// Introduce__Set memotong teks sesuai _introduce.length sebelum menyimpannya.
// Batas itu hanya dinaikkan selama panggilan bawaan, lalu dikembalikan, supaya
// fitur input teks tidak tertinggal aktif ketika sakelarnya dimatikan.
void hookPerkenalan(void* self, void* text, void* method) {
    auto asli = reinterpret_cast<void (*)(void*, void*, void*)>(target(T_INTRODUCE_SET).orig);
    if (!asli)
        return;

    void* perkenalan = nullptr;
    int32_t panjangAsli = 0;
    if (fitur().inputTeks && fieldAkun.field && fieldPerkenalan.field && fieldPanjang.field) {
        void* akun = bacaPtr(self, fieldAkun);
        perkenalan = bacaPtr(akun, fieldPerkenalan);
        if (perkenalan) {
            panjangAsli = bacaInt(perkenalan, fieldPanjang);
            tulisInt(perkenalan, fieldPanjang, INT32_MAX);
        }
    }

    asli(self, text, method);

    if (perkenalan)
        tulisInt(perkenalan, fieldPanjang, panjangAsli);
}

bool cariFieldAkun() {
    return cariField(kelasDari("Net__Account__My_Account__Detail"), "account", fieldAkun) &&
           cariField(kelasField(fieldAkun), "_introduce", fieldPerkenalan) &&
           cariField(kelasField(fieldPerkenalan), "length", fieldPanjang);
}

} // namespace

void pasangHookAkun() {
    if (cariFieldAkun()) {
        LOGI("Field akun OK");
    } else {
        LOGE("Field akun tidak ditemukan; perkenalan tetap terbatas");
    }

    pasangHook(target(T_INTRODUCE_SET), reinterpret_cast<void*>(hookPerkenalan));
}
