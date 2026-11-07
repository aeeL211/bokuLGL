#include "hooks.h"

#include <cstdint>

#include "feature_flags.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldManager = {};

// An explosion calls this to knock players away from the blast point. Dropping
// the call keeps the player standing while the tiles and effects still break.
// This state is also owned by guests inside the room, so only the local player's
// body is left without the knockback.
void hookKnockback(void* self, int32_t a, float b, float c, int32_t d, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int32_t, float, float, int32_t, void*)>(
        target(T_TUMBLE_EXPLODE).orig);
    if (!original)
        return;

    void* manager = readPtr(self, fieldManager);
    if (!features().knockback || (manager && !isLocalPlayerHook(manager))) {
        original(self, a, b, c, d, method);
        return;
    }
}

} // namespace

void installKnockbackHook() {
    if (!findField(classOf("Player__State__Std__Air"), "m", fieldManager))
        LOGE("Air player state field not found; knockback still applies to all players");
    installHook(target(T_TUMBLE_EXPLODE), reinterpret_cast<void*>(hookKnockback));
}
