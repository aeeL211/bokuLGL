#include "hooks.h"

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Every account detail row asks this before deciding what to display. Answering
// yes means the owner's "hidden" choice no longer overrides the value.
bool hookManager(void* self, int a, void* b, bool c, void* method) {
    if (!features().privacy) {
        auto original = reinterpret_cast<bool (*)(void*, int, void*, bool, void*)>(
            target(T_PRIVACY_MANAGER).orig);
        return original ? original(self, a, b, c, method) : false;
    }
    return true;
}

// The friends, blacklist, add friend, and verification screens carry a copy of
// the same check, so all of them are answered the same way.
template <TargetId id> bool hookRow(void* self, int a, void* method) {
    if (!features().privacy) {
        auto original = reinterpret_cast<bool (*)(void*, int, void*)>(target(id).orig);
        return original ? original(self, a, method) : false;
    }
    return true;
}

// The user object and the guest account store the email and the privacy value
// that decides whether that email may be sent in the same shape, so one field
// finder covers both.
struct FieldEmail {
    Field email;
    Field privacy;
    Field emailPrivacy;
    Field visible;
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

// The word the game itself uses for an email privacy value, so the row reads
// like the settings screen instead of an invented word.
void* labelPrivacy(int visible) {
    auto fn = reinterpret_cast<void* (*)(int, void*)>(target(T_PRIVACY_LABEL).addr);
    return fn ? fn(visible, target(T_PRIVACY_LABEL).method) : nullptr;
}

// Another account's email is only sent when its owner allows it, so a hidden
// email leaves the row empty. The owner's privacy label is shown instead.
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
    void* label = labelPrivacy(readInt(emailPrivacy, field.visible));
    if (!label)
        return;
    setRef(owner, field.email, label);
    if (recordedContent)
        return;
    recordedContent = true;
    char text[64];
    if (toUtf8(label, text, sizeof(text)))
        LOGI("Email privacy label: %s", text);
}

bool findFieldEmail(FieldEmail& field, void* klass) {
    if (!klass)
        return false;
    return findField(klass, "email", field.email) && findField(klass, "privacy", field.privacy) &&
           findField(fieldClass(field.privacy), "email", field.emailPrivacy) &&
           findField(fieldClass(field.emailPrivacy), "visible", field.visible);
}

void fillUserEmail(void* refUser) {
    fillEmail(refUser ? *reinterpret_cast<void**>(refUser) : nullptr, userEmail);
}

void hookSet(void* own, void* refUser, void* uid, void* document, bool overwrite,
               void* method) {
    auto original = reinterpret_cast<FnSet>(target(T_USER_SET).orig);
    if (original)
        original(own, refUser, uid, document, overwrite, method);
    fillUserEmail(refUser);
}

// The three local setters only differ in the type of their second argument.
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

// The manager does not hold an account itself: every guest in the room list
// carries one, keyed by the same text used as the name in the profile packet.
// The account is therefore looked up in that list, and a key from outside the
// room leaves nothing to fill.
void* guestAccount(void* own, void* user) {
    void* manager = readPtr(own, fieldManagerGuest);
    void* guestList = readPtr(manager, guestListField);
    void* item = readPtr(guestList, fieldItemGuest);
    if (!item)
        return nullptr;

    int32_t count = readInt(guestList, guestSizeField);
    int32_t capacity = lengthArray(item);
    if (count > capacity)
        count = capacity;

    char* data = arrayData(item);
    for (int32_t i = 0; i < count; i++) {
        void* guest = *reinterpret_cast<void**>(data + i * sizeof(void*));
        if (!guest || !sameString(readPtr(guest, guestKeyField), user))
            continue;
        return readPtr(guest, fieldAccountGuest);
    }
    return nullptr;
}

// The profile packet builds the guest account and a privacy message can replace
// its email again, so both paths end in the same fill.
void hookPublic(void* own, void* user, bool login, void* uid, void* email, void* date,
              void* portrait, void* introduction, void* method) {
    auto original = reinterpret_cast<FnPublic>(target(T_GUEST_COMMON).orig);
    if (original)
        original(own, user, login, uid, email, date, portrait, introduction, method);
    fillEmail(guestAccount(own, user), emailGuest);
}

void hookEmailPrivacy(void* own, void* user, int visible, void* email, void* method) {
    auto original = reinterpret_cast<FnPrivacy>(target(T_GUEST_PRIVACY_EMAIL).orig);
    if (original)
        original(own, user, visible, email, method);
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
        LOGI("Privacy fields ready");
    else
        LOGE("Privacy fields not found; hidden emails stay empty");
    if (!findFieldGuest())
        LOGE("Guest account fields not found; emails inside the room stay empty");

    const TargetId setter[] = {T_USER_SET, T_USER_SET_LOCAL, T_USER_SET_LOCAL_ARRAY,
                                 T_USER_SET_LOCAL_LIST};
    int found = 0;
    for (TargetId id : setter)
        found += target(id).addr ? 1 : 0;
    LOGI("Email setters: %d of 4", found);

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
