#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "hook.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldManajer = {};

// Ledakan memanggil ini untuk membuat pemain terpental menjauh dari titik
// ledakan. Membuang panggilannya membuat pemain tetap berdiri sementara tile dan
// efeknya tetap pecah. State ini juga dimiliki tamu di dalam room, jadi hanya
// badan pemain lokal yang dibiarkan tanpa pantulan.
void hookTerpental(void* self, int32_t a, float b, float c, int32_t d, void* metode) {
    auto asli = reinterpret_cast<void (*)(void*, int32_t, float, float, int32_t, void*)>(
        target(T_TUMBLE_EXPLODE).orig);
    if (!asli)
        return;

    void* manajer = bacaPtr(self, fieldManajer);
    if (!fitur().terpental || (manajer && !pemainLokalHook(manajer))) {
        asli(self, a, b, c, d, metode);
        return;
    }
}

} // namespace

void pasangHookTerpental() {
    if (!cariField(kelasDari("Player__State__Std__Air"), "m", fieldManajer))
        LOGE(
            "Field pemain state udara tidak ditemukan; Terpental tetap berlaku untuk semua pemain");
    pasangHook(target(T_TUMBLE_EXPLODE), reinterpret_cast<void*>(hookTerpental));
}
