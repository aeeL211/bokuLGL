#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"
#include "toast.h"

namespace {

Field fieldUi = {};
void* klassIap = nullptr;

void* uiIap() {
    if (!klassIap || !fieldUi.field)
        return nullptr;
    void* base = dataFieldStatic(klassIap);
    if (!base)
        return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(base) + fieldUi.offset);
}

// Skip Play Billing entirely: hand the product id to the game's own completion
// path, the same one the store callback would call. UI__Iap._Update then grants
// the candy on the next frame.
void hookBuy(int32_t productId, void* method) {
    if (!features().iap) {
        auto original = reinterpret_cast<void (*)(int32_t, void*)>(target(T_BUY).orig);
        if (original)
            original(productId, method);
        return;
    }
    void* ui = uiIap();
    void* done = target(T_PURCHASE_COMPLETE).addr;
    if (ui && done && target(T_PURCHASE_COMPLETE).method) {
        LOGI("Free purchase: product=%d", productId);
        auto call = reinterpret_cast<void (*)(void*, int32_t, void*)>(done);
        call(ui, productId, target(T_PURCHASE_COMPLETE).method);
        showToast("MODS: Pembelian berhasil disimulasikan");
        return;
    }
    LOGE("Free purchase: Iap.ui unavailable, falling back to the store");
    auto original = reinterpret_cast<void (*)(int32_t, void*)>(target(T_BUY).orig);
    if (!original)
        return;
    original(productId, method);
}

// Shared gate for tiles, models, and melodies.
bool hookOpenKey(void* self, int32_t id, bool* checked, bool* unlock, void* method) {
    if (!features().iap) {
        auto original = reinterpret_cast<bool (*)(void*, int32_t, bool*, bool*, void*)>(
            target(T_UNLOCK_CHECK).orig);
        return original ? original(self, id, checked, unlock, method) : false;
    }
    if (checked)
        *checked = true;
    if (unlock)
        *unlock = true;
    return true;
}

} // namespace

void installIapHook() {
    klassIap = classOf("Iap");
    if (!findField(klassIap, "ui", fieldUi)) {
        LOGE("Field Iap.ui not found; free purchase uses the store");
    }

    installHook(target(T_BUY), reinterpret_cast<void*>(hookBuy));
    installHook(target(T_UNLOCK_CHECK), reinterpret_cast<void*>(hookOpenKey));

    if (!target(T_PURCHASE_COMPLETE).addr) {
        LOGE("Target not found: UI__Iap.Purchase__Complete");
    }
}
