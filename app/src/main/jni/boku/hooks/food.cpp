#include "hooks.h"

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "targets.h"

namespace {

// Setiap metode akhir makan dimiliki state pemain, dan state itu menyimpan
// Player__Manager miliknya. Offset m berbeda per postur, jadi field dicari per
// target dan disimpan pada instansiasi template masing-masing.
template <TargetId id> Field& fieldPlayerEat() {
    static Field field = {};
    return field;
}

// Makanan hanya keluar dari tangan lewat ...__Eat__End miliknya sendiri, yang
// meminta id tangan kosong ke Player__Manager.Hh__Change. Setiap makanan dan
// postur punya salinan metode itu, jadi masing-masing menahan perubahan tangan
// dan potongannya tetap di tangan. Sushi, es krim, dan popsicle sengaja
// dibiarkan: satu gigitan menghabiskan item itu, lalu game menaruh pemain pada
// pose akhir yang mematikan lapisan tangan, sehingga potongan yang ditahan tetap
// tidak terlihat sampai pemain bergerak. Gingerbread dan hot cross bun
// mengakhiri gigitan terakhirnya pada pose yang masih menggambar tangan.
//
// Animasi makan menyembunyikan sprite yang dipegang tepat setelah perubahan
// tangan itu, jadi tangan disusun ulang di akhir update pemain tersebut, saat
// mesin state sudah melewati animasi makan. Tanpa itu makanannya masih di
// tangan tetapi tidak terlihat sampai pemain bergerak dan penyegaran state
// menyusunnya kembali.
//
// Tamu di dalam room menjalankan state yang sama di simulasi lokal, jadi hanya
// pemain lokal yang tangannya ditahan.
template <TargetId id> void hookEndEat(void* self, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(id).orig);
    if (!original)
        return;
    if (!features().food) {
        original(self, method);
        return;
    }
    const Field& playerField = fieldPlayerEat<id>();
    if (playerField.field && !isLocalPlayerHook(readPtr(self, playerField))) {
        original(self, method);
        return;
    }
    setHoldHand(true);
    original(self, method);
    setHoldHand(false);
    refreshHand();
}

template <TargetId id> void installEndEat() {
    findField(classOf(target(id).cls), "m", fieldPlayerEat<id>());
    installHook(target(id), reinterpret_cast<void*>(hookEndEat<id>));
}

} // namespace

void installFoodHook() {
    installEndEat<T_EAT_END_KNEEL>();
    installEndEat<T_EAT_END_SIT_F>();
    installEndEat<T_EAT_END_SIT_S>();
    installEndEat<T_EAT_END_FOOD>();
    installEndEat<T_EAT_END_BUN_KNEEL>();
    installEndEat<T_EAT_END_BUN_SIT_F>();
    installEndEat<T_EAT_END_BUN_SIT_S>();
    installEndEat<T_EAT_END_BUN_FOOD>();
}
