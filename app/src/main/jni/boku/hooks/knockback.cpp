#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldManager = {};

// Ledakan memanggil ini untuk membuat pemain terpental menjauh dari titik
// ledakan. Membuang panggilannya membuat pemain tetap berdiri sementara tile dan
// efeknya tetap pecah. State ini juga dimiliki tamu di dalam room, jadi hanya
// badan pemain lokal yang dibiarkan tanpa pantulan.
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
        LOGE(
            "Field pemain state udara tidak ditemukan; Terpental tetap berlaku untuk semua pemain");
    installHook(target(T_TUMBLE_EXPLODE), reinterpret_cast<void*>(hookKnockback));
}
