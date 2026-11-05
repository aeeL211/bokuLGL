#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "hook.h"
#include "targets.h"

namespace {

// Ledakan memanggil ini untuk membuat pemain lokal terpental menjauh dari titik
// ledakan. Membuang panggilannya membuat pemain tetap berdiri sementara tile dan
// efeknya tetap pecah.
void hookTerpental(void* self, int32_t a, float b, float c, int32_t d, void* metode) {
    auto asli = reinterpret_cast<void (*)(void*, int32_t, float, float, int32_t, void*)>(
        target(T_TUMBLE_EXPLODE).orig);
    if (!fitur().terpental && asli)
        asli(self, a, b, c, d, metode);
}

} // namespace

void pasangHookTerpental() {
    pasangHook(target(T_TUMBLE_EXPLODE), reinterpret_cast<void*>(hookTerpental));
}
