#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "targets.h"

namespace {

// Camera shake is purely visual, so both entry points are dropped while the
// shake switch is off. Tile debris has its own switch.
void hookShake(void* self, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_SHAKE).orig);
    if (!features().shake && original)
        original(self, method);
}

void hookShakeNumber(void* self, int32_t duration, float power, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, int32_t, float, void*)>(target(T_SHAKE_NUM).orig);
    if (!features().shake && original)
        original(self, duration, power, method);
}

// Tile debris is spawned when a tile breaks. Dropping the spawn leaves the tile
// and its record intact while the fragments are gone.
void hookDebris(void* self, int32_t type, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_DEBRIS).orig);
    if (!features().debris && original)
        original(self, type, method);
}

void hookDebrisTile(void* self, float x, float y, int32_t type, void* data, void* method) {
    auto original = reinterpret_cast<void (*)(void*, float, float, int32_t, void*, void*)>(
        target(T_DEBRIS_TILE).orig);
    if (!features().debris && original)
        original(self, x, y, type, data, method);
}

} // namespace

void installEffectsHook() {
    installHook(target(T_SHAKE), reinterpret_cast<void*>(hookShake));
    installHook(target(T_SHAKE_NUM), reinterpret_cast<void*>(hookShakeNumber));
    installHook(target(T_DEBRIS), reinterpret_cast<void*>(hookDebris));
    installHook(target(T_DEBRIS_TILE), reinterpret_cast<void*>(hookDebrisTile));
}
