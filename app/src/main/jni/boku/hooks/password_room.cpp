#include "hooks.h"

#include <cstdint>

#include "feature_flags.h"
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

// A room password is only a few characters. Anything longer is a stale object
// whose header got reused, not a real string, and passing it to the keyboard
// makes Unity scan past the end of the heap.
constexpr int32_t kLimitPassword = 4096;

bool stringEligible(void* str) {
    if (!str)
        return false;
    int32_t length = lengthString(str);
    return length >= 0 && length <= kLimitPassword;
}

void* stringEmpty() { return stringUtf16New(reinterpret_cast<const uint16_t*>(u""), 0); }

// The room data owns the room password, so reading it on demand keeps every
// consumer of that value on the same live object.
void* roomPassword(void* self) {
    void* detail = readPtr(self, fieldDetail);
    void* dataRoom = readPtr(detail, fieldDataRoom);
    return readPtr(dataRoom, fieldPasswordRoom);
}

// Term__Bottom_Button() reports whether the OK button is enabled: it returns
// whether _password.str differs from the empty string. Input__Ok() returns right
// away on false and the button itself is dimmed, so an empty box can never be
// sent and the comparison is never reached. This hook keeps the button always
// usable; the original code still checks the room password, which okHook fills
// in right before the comparison.
bool hookButton(void* self, void* method) {
    if (!features().roomPassword) {
        auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_PASSWORD_BUTTON).orig);
        return original ? original(self, method) : false;
    }
    return true;
}

// Input__Ok compares _password.str against detail._room_Data.password and only
// then closes the keyboard and calls Join__Start. The box stays empty, so the
// room's own string object is installed for the comparison: String.Equals
// returns true on the same reference before reading a single character, and the
// room data keeps that object alive.
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
    if (value && stringEligible(password)) {
        setRef(value, fieldStr, password);
        // The comparison reads this field after the original runs, so a write
        // that did not land is the only failure that must be obvious below.
        void* saved = readPtr(value, fieldStr);
        if (saved == password)
            LOGI("Password: room password installed, length=%d", lengthString(password));
        else
            LOGE("Password: room password write failed, have=%p expected=%p", saved, password);
    } else {
        LOGE("Password: no room password, comparison runs as-is");
    }
    // Reaching this line means the comparison failed: the passing path calls
    // Join__Start as a tail call and never returns.
    original(self, method);
    LOGI("Password: gate comparison failed, hookOk returns");
}

// The keyboard path forwards _password.str as the initial text, so a null there
// aborts before the field opens. _Start normally writes an empty string; after a
// join the panel can still hold a reused string. The room password is installed
// again for that case, because the room data owns it and keeps it alive while
// the keyboard reads it, whereas a fresh allocation can be collected too early.
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
    // The panel hands this string to the keyboard, so log it before the original
    // runs: a stale pointer here is what later chokes the engine.
    LOGI("Password field: value=%p str=%p length=%d", value, str, lengthString(str));
    if (value && !stringEligible(str)) {
        void* password = roomPassword(self);
        void* initial = stringEligible(password) ? password : stringEmpty();
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
        LOGI("Room password field OK");
        LOGI("Password offsets: _password=%zu str=%zu room_Data=%zu room_password=%zu",
             fieldPassword.offset, fieldStr.offset, fieldDataRoom.offset, fieldPasswordRoom.offset);
    } else {
        LOGE("Room password field not found; password rooms stay manual");
    }

    installHook(target(T_PASSWORD_OK), reinterpret_cast<void*>(hookOk));
    installHook(target(T_PASSWORD_FIELD), reinterpret_cast<void*>(hookField));
    installHook(target(T_PASSWORD_BUTTON), reinterpret_cast<void*>(hookButton));
}
