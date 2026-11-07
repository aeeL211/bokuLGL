#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "targets.h"

namespace {

// Every text check in the game is decided here: the word list is consulted
// first, and when the caller also asks for a record, that record is created.
// Answering false skips both, so chat lines, names, and profile text pass.
bool hookSensitive(void* self, void* text, int32_t language, bool record, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*, int32_t, bool, void*)>(
        target(T_SENSITIVE_WORD).orig);
    if (!features().rudeWord)
        return original ? original(self, text, language, record, method) : false;
    return false;
}

} // namespace

void installSensitiveHook() {
    installHook(target(T_SENSITIVE_WORD), reinterpret_cast<void*>(hookSensitive));
}
