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

// The game rewrites UIInput.characterLimit (name 38, day 2, month 2, year 4)
// right before every edit, so both truncation points must override it:
// Insert(string) enforces the limit per keystroke, Validate(string) trims the
// value on Set/send.
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

// The keyboard converts this string to UTF-8 and walks its characters up to the
// length in the header. A string whose header no longer holds a sane length
// makes that walk run past the heap, so hand over a fresh empty string instead
// of letting the engine scan unmapped memory.
constexpr int32_t kLimitKeyboard = 4096;

void* stringEmpty() { return stringUtf16New(reinterpret_cast<const uint16_t*>(u""), 0); }

// The Length parameter is unused on the mobile path, but the Steam/gamepad path
// forwards it to its own text input. Zero means no character limit there.
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
    // Name the string the engine is about to touch, so a bad pointer shows up in
    // the log right before a crash, not only in the fault address.
    LOGI("Keyboard: s=%d type=%d text=%p length=%d", s, type, text, length);
    if (!text || length < 0 || length > kLimitKeyboard) {
        void* empty = stringEmpty();
        if (empty)
            text = empty;
    }
    original(self, s, type, text, 0, method);
}

// Name__Check trims the profile name to _name.length_Limit and falls back to
// the default when the result is empty. Raise that limit around the call so the
// trim passes through and the rest of the check still runs.
void hookCheckName(void* self, void** name, bool fix, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, void**, bool, void*)>(target(T_NAME_CHECK).orig);
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
        LOGE("Field UIInput.characterLimit not found; input limit stays vanilla");
    }

    void* klassAccount = classOf("Net__Account__Manager");
    if (findField(klassAccount, "_name", nameData) &&
        findField(fieldClass(nameData), "length_Limit", nameLimit)) {
        LOGI("Name limit field OK");
    } else {
        LOGE("Name limit field not found; profile name stays limited");
    }

    installHook(target(T_INPUT_VALIDATE), reinterpret_cast<void*>(hookValidate));
    installHook(target(T_INPUT_INSERT), reinterpret_cast<void*>(hookInsert));
    installHook(target(T_KEYBOARD_OPEN), reinterpret_cast<void*>(hookOpenKeyboard));
    installHook(target(T_NAME_CHECK), reinterpret_cast<void*>(hookCheckName));
}
