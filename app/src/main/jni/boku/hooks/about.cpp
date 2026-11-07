#include "hooks.h"

#include <cstdint>
#include <cstring>

#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Setiap label versi yang bisa dibuka pemain diisi dari salah satu metode ini:
// halaman Tentang, layar judul, dan halaman bantuan multiplayer. Akhiran
// ditambahkan ke teks bawaan, jadi build ini ditandai modded tanpa
// menyembunyikan versi yang dilaporkan game.
constexpr char kAkhiranMod[] = " (modded)";

// String versi itu pendek; yang lebih panjang berarti ada sesuatu yang tidak
// terduga di ujungnya, jadi dibiarkan apa adanya daripada disalin mentah.
constexpr int32_t kLimitVersion = 240;

using HookVersion = void* (*)(void*, void*);

bool hasText(void* str, const char* needle) {
    const uint16_t* character = characterString(str);
    int32_t length = lengthString(str);
    size_t sizeNeedle = strlen(needle);
    if (!character || static_cast<size_t>(length) < sizeNeedle)
        return false;
    for (int32_t i = 0; i + static_cast<int32_t>(sizeNeedle) <= length; i++) {
        size_t j = 0;
        while (j < sizeNeedle && character[i + j] == static_cast<uint16_t>(needle[j]))
            j++;
        if (j == sizeNeedle)
            return true;
    }
    return false;
}

// Mengembalikan teks bawaan dengan kAkhiranMod ditambahkan, atau teks bawaan itu
// sendiri bila sudah membawa akhiran, kosong, atau terlalu panjang untuk sebuah
// label versi.
void* markVersion(void* fallback, const char* label) {
    if (!fallback || !characterString(fallback))
        return fallback;
    if (hasText(fallback, kAkhiranMod))
        return fallback;

    int32_t length = lengthString(fallback);
    const uint16_t* character = characterString(fallback);
    if (!character || length <= 0 || length > kLimitVersion)
        return fallback;

    uint16_t buffer[kLimitVersion + sizeof(kAkhiranMod)];
    int32_t terpakai = 0;
    for (int32_t i = 0; i < length; i++)
        buffer[terpakai++] = character[i];
    for (size_t i = 0; i + 1 < sizeof(kAkhiranMod); i++)
        buffer[terpakai++] = static_cast<uint16_t>(kAkhiranMod[i]);

    void* ditandai = stringUtf16New(buffer, terpakai);
    if (!ditandai)
        return fallback;
    LOGI("%s: akhiran versi ditambahkan, panjang=%d", label, terpakai);
    return ditandai;
}

HookVersion originalFrom(TargetId id) { return reinterpret_cast<HookVersion>(target(id).orig); }

void* hookAbout(void* self, void* method) {
    HookVersion original = originalFrom(T_ABOUT_VERSION);
    return original ? markVersion(original(self, method), "Tentang") : nullptr;
}

void* hookTitle(void* self, void* method) {
    HookVersion original = originalFrom(T_TITLE_VERSION);
    return original ? markVersion(original(self, method), "Judul") : nullptr;
}

void* hookHelp(void* self, void* method) {
    HookVersion original = originalFrom(T_HELP_VERSION);
    return original ? markVersion(original(self, method), "Bantuan") : nullptr;
}

} // namespace

void installAboutHook() {
    installHook(target(T_ABOUT_VERSION), reinterpret_cast<void*>(hookAbout));
    installHook(target(T_TITLE_VERSION), reinterpret_cast<void*>(hookTitle));
    installHook(target(T_HELP_VERSION), reinterpret_cast<void*>(hookHelp));
}
