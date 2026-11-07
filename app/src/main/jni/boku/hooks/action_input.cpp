#include "hooks.h"

#include "il2cpp_api.h"
#include "log.h"

namespace {

Field fieldInput = {};
Field fieldManagerInput = {};
Field fieldAction = {};
Field fieldHold = {};

} // namespace

// The interaction button is held while the input object stores an action.
bool isActionButtonPressed(void* manager) {
    void* input = readPtr(manager, fieldInput);
    void* managerInput = readPtr(input, fieldManagerInput);
    void* action = readPtr(managerInput, fieldAction);
    return action && readBool(action, fieldHold);
}

// Shared by every feature that repeats its action while the button is held.
void findActionFields() {
    void* managerClass = classOf("Player__Manager");
    bool ok = findField(managerClass, "input", fieldInput) &&
              findField(fieldClass(fieldInput), "input", fieldManagerInput) &&
              findField(fieldClass(fieldManagerInput), "action", fieldAction) &&
              findField(fieldClass(fieldAction), "keep", fieldHold);
    if (ok)
        LOGI("Field input action OK");
    else
        LOGE("Field input action not found; held button repeat stays off");
}
