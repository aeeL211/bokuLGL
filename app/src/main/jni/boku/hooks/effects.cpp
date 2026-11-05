#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "hook.h"
#include "targets.h"

namespace {

// Getar kamera murni visual, jadi kedua titik masuknya dibuang saat sakelar
// getar dimatikan. Puing tile punya sakelar sendiri.
void hookGetar(void* self, void* metode) {
    auto asli = reinterpret_cast<void (*)(void*, void*)>(target(T_SHAKE).orig);
    if (!fitur().getar && asli)
        asli(self, metode);
}

void hookGetarAngka(void* self, int32_t durasi, float kekuatan, void* metode) {
    auto asli = reinterpret_cast<void (*)(void*, int32_t, float, void*)>(target(T_SHAKE_NUM).orig);
    if (!fitur().getar && asli)
        asli(self, durasi, kekuatan, metode);
}

// Puing tile dimunculkan saat tile pecah. Membuang pemunculannya membiarkan tile
// dan rekamannya utuh sementara serpihannya tidak ada.
void hookPuing(void* self, int32_t jenis, void* metode) {
    auto asli = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_DEBRIS).orig);
    if (!fitur().pecahan && asli)
        asli(self, jenis, metode);
}

void hookPuingTile(void* self, float x, float y, int32_t jenis, void* data, void* metode) {
    auto asli = reinterpret_cast<void (*)(void*, float, float, int32_t, void*, void*)>(
        target(T_DEBRIS_TILE).orig);
    if (!fitur().pecahan && asli)
        asli(self, x, y, jenis, data, metode);
}

} // namespace

void pasangHookEfek() {
    pasangHook(target(T_SHAKE), reinterpret_cast<void*>(hookGetar));
    pasangHook(target(T_SHAKE_NUM), reinterpret_cast<void*>(hookGetarAngka));
    pasangHook(target(T_DEBRIS), reinterpret_cast<void*>(hookPuing));
    pasangHook(target(T_DEBRIS_TILE), reinterpret_cast<void*>(hookPuingTile));
}
