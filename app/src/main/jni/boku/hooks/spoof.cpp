#include "hooks.h"

#include "hook.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

using FnKunci = void* (*)(void*, void*);

// Paket khusus host lolos di penerima karena payload-nya membawa kunci pengirim,
// dan penerima hanya membandingkan string itu. Penggantian ini hidup di thread
// pengirim dan dibersihkan begitu paketnya pergi.
thread_local bool spoofAktif = false;
thread_local void* kunciSpoof = nullptr;

Field fieldManajerPemain = {};
Field fieldMultiManajer = {};
Field fieldPhoton = {};

void* hookKunci(void* sendiri, void* metode) {
    if (spoofAktif && kunciSpoof)
        return kunciSpoof;
    auto asli = reinterpret_cast<FnKunci>(target(T_PLAYER_KEY).orig);
    return asli ? asli(sendiri, metode) : nullptr;
}

// Kedua getter kunci melempar NullReferenceException selama room belum punya
// objek photon, dan eksepsi yang keluar dari hook mematikan proses. Pemeriksaan
// ini memakai field, bukan getter, jadi tidak bisa melempar apa pun.
bool photonSiap(void* multi) {
    return multi && fieldPhoton.field && bacaPtr(multi, fieldPhoton);
}

} // namespace

// Kunci host hanya dibaca selama objek photon room sudah ada. Pemanggil di luar
// room menerima nullptr dan melewati paketnya.
void* kunciHostDari(void* multi) {
    auto kunci = reinterpret_cast<FnKunci>(target(T_HOST_KEY).addr);
    if (!photonSiap(multi) || !kunci)
        return nullptr;
    return kunci(multi, target(T_HOST_KEY).method);
}

// Salinan aman dari Player__Manager.Key. Dipakai fitur yang butuh kunci pemain
// sendiri tanpa boleh memicu eksepsi room yang belum siap.
void* kunciPemainAman(void* pemain) {
    auto kunci = reinterpret_cast<FnKunci>(target(T_PLAYER_KEY).orig);
    void* manajer = pemain ? bacaPtr(pemain, fieldManajerPemain) : nullptr;
    if (!kunci || !photonSiap(bacaPtr(manajer, fieldMultiManajer)))
        return nullptr;
    return kunci(pemain, target(T_PLAYER_KEY).method);
}

// Hanya berlaku sampai hentiSpoof(), jadi pemanggil membungkus satu panggilan
// kirim saja dan sisa permainan tetap memakai kunci pemain sendiri.
void mulaiSpoof(void* multi) {
    kunciSpoof = kunciHostDari(multi);
    spoofAktif = kunciSpoof != nullptr;
}

void hentiSpoof() {
    spoofAktif = false;
    kunciSpoof = nullptr;
}

void pasangHookSpoof() {
    if (!target(T_PLAYER_KEY).addr || !target(T_HOST_KEY).addr) {
        LOGE("Target spoof pengirim tidak ditemukan");
        return;
    }
    bool lengkap = cariField(kelasDari("Player__Manager"), "a", fieldManajerPemain) &&
                   cariField(kelasField(fieldManajerPemain), "multi", fieldMultiManajer) &&
                   cariField(kelasField(fieldMultiManajer), "photon", fieldPhoton);
    if (lengkap)
        LOGI("Field kunci pemain siap");
    else
        LOGE("Field kunci pemain tidak lengkap; kunci dibaca hanya bila photon terbaca");
    pasangHook(target(T_PLAYER_KEY), reinterpret_cast<void*>(hookKunci));
}
