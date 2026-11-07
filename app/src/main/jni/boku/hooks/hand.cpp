#include "hooks.h"

#include <cstdint>

#include "hook_manager.h"
#include "log.h"
#include "targets.h"

namespace {

// Hh__Change menulis id pegang yang baru ke model pemain dan membangun ulang
// sprite tangan. Fitur yang menahan itemnya menaikkan flag ini di sekitar
// panggilan bawaan yang akan mengosongkan tangan, jadi perubahan itu menjadi
// tanpa efek.
thread_local bool holdHand = false;
// Id yang terakhir diketahui ada di tangan, disimpan supaya akhir makan bisa
// mengembalikan item yang sama setelah animasinya menyembunyikan sprite.
void* ownerHand = nullptr;
int32_t idHand = 0;
// Akhir makan meminta tangan kembali, tapi pembangunan ulang saat state makan
// masih aktif membangun pose makan yang tersembunyi. Pembangunan ulang menunggu
// akhir update pemain itu, saat state machine sudah melewati animasi makan.
bool refreshPending = false;
int remainingLogRefresh = 12;

void buildReloadHand(void* own) {
    auto hh = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_HH_CHANGE).orig);
    if (!hh)
        return;
    hh(own, idHand, target(T_HH_CHANGE).method);
    if (remainingLogRefresh > 0) {
        remainingLogRefresh--;
        LOGI("Tangan disegarkan: id=%d", idHand);
    }
}

// Membaca id yang benar-benar masih di tangan dari model pemain. Cache idTangan
// bisa sudah 0 bila fitur makanan sempat dimatikan dan game mengosongkan tangan,
// jadi nilai hidup ini yang dipakai saat perubahan tangan berikutnya ditahan.
int32_t idHandFromGame(void* own) {
    auto read = reinterpret_cast<int32_t (*)(void*, void*)>(target(T_HH_ID).addr);
    if (!read)
        return idHand;
    return read(own, target(T_HH_ID).method);
}

void hookChangeHh(void* own, int32_t id, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_HH_CHANGE).orig);
    if (!original)
        return;
    if (holdHand) {
        // Game hendak mengosongkan tangan. Id yang sekarang dipakai dibaca
        // sebelum perubahan itu hilang, lalu dipakai lagi oleh segarkanTangan.
        ownerHand = own;
        idHand = idHandFromGame(own);
        return;
    }
    original(own, id, method);
    // Hh__Change juga dijalankan untuk tamu di dalam room. Cache tangan hanya
    // boleh mengikuti pemain lokal, kalau tidak akhir makan tamu bisa menyusun
    // ulang tangan pemain sendiri.
    if (!isLocalPlayerHook(own))
        return;
    ownerHand = own;
    idHand = id;
}

void hookUpdate(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_PLAYER_UPDATE).orig);
    if (!original)
        return;
    original(own, method);
    onPlayerUpdate(own);
    if (!refreshPending || own != ownerHand)
        return;
    refreshPending = false;
    buildReloadHand(own);
}

} // namespace

void setHoldHand(bool hold) { holdHand = hold; }

void refreshHand() {
    if (!ownerHand)
        return;
    // Tanpa hook update tidak ada tempat aman untuk membangun ulang nanti, jadi
    // kembali ke saat akhir makan selesai.
    if (!target(T_PLAYER_UPDATE).orig) {
        buildReloadHand(ownerHand);
        return;
    }
    refreshPending = true;
}

void installHandHook() {
    installHook(target(T_HH_CHANGE), reinterpret_cast<void*>(hookChangeHh));
    installHook(target(T_PLAYER_UPDATE), reinterpret_cast<void*>(hookUpdate));
}
