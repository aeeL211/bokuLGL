#include "hooks.h"

#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

using FnKey = void* (*)(void*, void*);

// Paket khusus host lolos di penerima karena payload-nya membawa kunci pengirim,
// dan penerima hanya membandingkan string itu. Penggantian ini hidup di thread
// pengirim dan dibersihkan begitu paketnya pergi.
thread_local bool spoofActive = false;
thread_local void* keySpoof = nullptr;

Field fieldManagerPlayer = {};
Field fieldMultiManager = {};
Field fieldPhoton = {};

void* hookKey(void* own, void* method) {
    if (spoofActive && keySpoof)
        return keySpoof;
    auto original = reinterpret_cast<FnKey>(target(T_PLAYER_KEY).orig);
    return original ? original(own, method) : nullptr;
}

// Kedua getter kunci melempar NullReferenceException selama room belum punya
// objek photon, dan eksepsi yang keluar dari hook mematikan proses. Pemeriksaan
// ini memakai field, bukan getter, jadi tidak bisa melempar apa pun.
bool photonReady(void* multi) { return multi && fieldPhoton.field && readPtr(multi, fieldPhoton); }

} // namespace

// Kunci host hanya dibaca selama objek photon room sudah ada. Pemanggil di luar
// room menerima nullptr dan melewati paketnya.
void* hostKeyFrom(void* multi) {
    auto key = reinterpret_cast<FnKey>(target(T_HOST_KEY).addr);
    if (!photonReady(multi) || !key)
        return nullptr;
    return key(multi, target(T_HOST_KEY).method);
}

// Salinan aman dari Player__Manager.Key. Dipakai fitur yang butuh kunci pemain
// sendiri tanpa boleh memicu eksepsi room yang belum siap.
void* safePlayerKey(void* player) {
    auto key = reinterpret_cast<FnKey>(target(T_PLAYER_KEY).orig);
    void* manager = player ? readPtr(player, fieldManagerPlayer) : nullptr;
    if (!key || !photonReady(readPtr(manager, fieldMultiManager)))
        return nullptr;
    return key(player, target(T_PLAYER_KEY).method);
}

// Hanya berlaku sampai hentiSpoof(), jadi pemanggil membungkus satu panggilan
// kirim saja dan sisa permainan tetap memakai kunci pemain sendiri.
void startSpoof(void* multi) {
    keySpoof = hostKeyFrom(multi);
    spoofActive = keySpoof != nullptr;
}

void stopSpoof() {
    spoofActive = false;
    keySpoof = nullptr;
}

void installSpoofHook() {
    if (!target(T_PLAYER_KEY).addr || !target(T_HOST_KEY).addr) {
        LOGE("Target spoof pengirim tidak ditemukan");
        return;
    }
    bool lengkap = findField(classOf("Player__Manager"), "a", fieldManagerPlayer) &&
                   findField(fieldClass(fieldManagerPlayer), "multi", fieldMultiManager) &&
                   findField(fieldClass(fieldMultiManager), "photon", fieldPhoton);
    if (lengkap)
        LOGI("Field kunci pemain siap");
    else
        LOGE("Field kunci pemain tidak lengkap; kunci dibaca hanya bila photon terbaca");
    installHook(target(T_PLAYER_KEY), reinterpret_cast<void*>(hookKey));
}
