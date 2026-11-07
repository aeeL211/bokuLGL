#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "targets.h"

namespace {

// Getar kamera murni visual, jadi kedua titik masuknya dibuang saat sakelar
// getar dimatikan. Puing tile punya sakelar sendiri.
void hookShake(void* self, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_SHAKE).orig);
    if (!features().shake && original)
        original(self, method);
}

void hookShakeNumber(void* self, int32_t duration, float power, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int32_t, float, void*)>(target(T_SHAKE_NUM).orig);
    if (!features().shake && original)
        original(self, duration, power, method);
}

// Puing tile dimunculkan saat tile pecah. Membuang pemunculannya membiarkan tile
// dan rekamannya utuh sementara serpihannya tidak ada.
void hookDebris(void* self, int32_t jenis, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_DEBRIS).orig);
    if (!features().pecahan && original)
        original(self, jenis, method);
}

void hookDebrisTile(void* self, float x, float y, int32_t jenis, void* data, void* method) {
    auto original = reinterpret_cast<void (*)(void*, float, float, int32_t, void*, void*)>(
        target(T_DEBRIS_TILE).orig);
    if (!features().pecahan && original)
        original(self, x, y, jenis, data, method);
}

} // namespace

void installEffectsHook() {
    installHook(target(T_SHAKE), reinterpret_cast<void*>(hookShake));
    installHook(target(T_SHAKE_NUM), reinterpret_cast<void*>(hookShakeNumber));
    installHook(target(T_DEBRIS), reinterpret_cast<void*>(hookDebris));
    installHook(target(T_DEBRIS_TILE), reinterpret_cast<void*>(hookDebrisTile));
}
