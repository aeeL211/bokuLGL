#include "hooks.h"

#include <cstdint>
#include <cstring>

#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Every version label the player can open is filled from one of these methods:
// the About page, the title screen, and the multiplayer help page. The suffix is
// appended to the built-in text, so this build is marked modded without hiding
// the version the game reports.
constexpr char kModSuffix[] = " (modded)";

// Version strings are short; anything longer means something unexpected at the
// end, so it is left as is rather than copied raw.
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

// Returns the built-in text with kAkhiranMod appended, or the built-in text
// itself when it already carries the suffix, is empty, or is too long for a
// version label.
void* markVersion(void* fallback, const char* label) {
    if (!fallback || !characterString(fallback))
        return fallback;
    if (hasText(fallback, kModSuffix))
        return fallback;

    int32_t length = lengthString(fallback);
    const uint16_t* character = characterString(fallback);
    if (!character || length <= 0 || length > kLimitVersion)
        return fallback;

    uint16_t buffer[kLimitVersion + sizeof(kModSuffix)];
    int32_t used = 0;
    for (int32_t i = 0; i < length; i++)
        buffer[used++] = character[i];
    for (size_t i = 0; i + 1 < sizeof(kModSuffix); i++)
        buffer[used++] = static_cast<uint16_t>(kModSuffix[i]);

    void* marked = stringUtf16New(buffer, used);
    if (!marked)
        return fallback;
    LOGI("%s: version suffix appended, length=%d", label, used);
    return marked;
}

HookVersion originalFrom(TargetId id) { return reinterpret_cast<HookVersion>(target(id).orig); }

void* hookAbout(void* self, void* method) {
    HookVersion original = originalFrom(T_ABOUT_VERSION);
    return original ? markVersion(original(self, method), "About") : nullptr;
}

void* hookTitle(void* self, void* method) {
    HookVersion original = originalFrom(T_TITLE_VERSION);
    return original ? markVersion(original(self, method), "Title") : nullptr;
}

void* hookHelp(void* self, void* method) {
    HookVersion original = originalFrom(T_HELP_VERSION);
    return original ? markVersion(original(self, method), "Help") : nullptr;
}

} // namespace

void installAboutHook() {
    installHook(target(T_ABOUT_VERSION), reinterpret_cast<void*>(hookAbout));
    installHook(target(T_TITLE_VERSION), reinterpret_cast<void*>(hookTitle));
    installHook(target(T_HELP_VERSION), reinterpret_cast<void*>(hookHelp));
}
