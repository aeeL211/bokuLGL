#include <jni.h>
#include <pthread.h>
#include <unistd.h>

#include "Utils.hpp"
#include "hook_manager.h"
#include "hooks/hooks.h"
#include "il2cpp_api.h"
#include "info.h"
#include "jni_bridge.h"
#include "log.h"
#include "mod.h"
#include "targets.h"
#include "toast.h"

#if !defined(__aarch64__)
#error "Hanya arm64-v8a yang didukung"
#endif

namespace {

void* pekerja(void*) {
    LOGI("Pekerja mulai");
    // JNI_OnLoad berjalan setelah konstruktor kembali, jadi kelas activity bisa
    // belum tersimpan saat thread ini mulai.
    for (int i = 0; i < 10 && !kelasUnityPlayer(); i++)
        usleep(100000);
    catatLingkungan();
    if (!inisialisasiTarget())
        return nullptr;

    // Menu menanyakan IsGameLibLoaded() sebelum menampilkan daftar fitur, jadi
    // penantian ini sekaligus menandai bahwa libil2cpp.so sudah tersedia.
    for (int i = 0; i < 120 && !isLibraryLoaded("libil2cpp.so"); i++) {
        sleep(1);
    }
    sleep(2);

    if (!ikatApi()) {
        LOGE("API IL2CPP tidak ditemukan");
        return nullptr;
    }
    // ShadowHook memasang penangan SIGSEGV/SIGBUS sendiri untuk proteksi saat
    // menulis kode. Penangan crash mod dipasang lebih dulu, lalu ShadowHook
    // diinisialisasi setelah Unity selesai memasang penangannya, sehingga
    // rantainya bytesig -> log mod -> penangan Unity. Probe dipasang setelah
    // tabel modul yang dipakai untuk menyelesaikannya dipotret.
    pasangLogCrash();
    // Kegagalan inisialisasi tidak menghentikan mod: setiap hook akan mencatat
    // kegagalannya sendiri, fitur yang hanya membaca field tetap berjalan, dan
    // permainan dibiarkan pada perilaku vanilla.
    if (inisialisasiHook())
        pasangProbeCrash();
    // libil2cpp dan pustaka pendampingnya selesai dimuat sebelum titik ini,
    // jadi tabel modul dipotret ulang supaya frame crash nanti bisa dinamai.
    segarkanModul();
    LOGI("API IL2CPP terikat");

    void* domain = nullptr;
    for (int i = 0; i < 60 && !(domain = apiIl2cpp().domain()); i++) {
        sleep(1);
    }
    if (!domain) {
        LOGE("Domain IL2CPP belum siap");
        return nullptr;
    }

    LOGI("Domain siap, scan dimulai");
    apiIl2cpp().attach(domain);
    int ditemukan = scan();
    LOGI("Scan selesai: %d dari %d metode ditemukan", ditemukan, T_COUNT);

    pasangHookIap();
    pasangHookMultiplayer();
    pasangHookChat();
    pasangHookSensitif();
    pasangHookInputTeks();
    pasangHookProfil();
    pasangHookSandi();
    pasangHookRentangSkin();
    pasangHookTentang();
    cariFieldPemainLokal();
    pasangHookTangan();
    pasangHookMakanan();
    pasangHookPrivasi();
    cariFieldAksi();
    pasangHookSenjata();
    pasangHookEfek();
    pasangHookTerpental();
    pasangHookKotoran();
    pasangHookSkin();
    pasangHookSlot();
    pasangHookSpoof();
    pasangHookRoom();
    pasangHookPemain();
    segarkanModul();
    LOGI("Hook terpasang: %d", jumlahHookTerpasang());
    toastMasuk();
    return nullptr;
}

} // namespace

void siapkanMod(JavaVM* vm, JNIEnv* env) {
    setelVmJava(vm);
    if (env)
        simpanKelasUnityPlayer(env);
    bukaFileLog();
}

__attribute__((constructor)) static void saatMuat() {
    bukaFileLog();
    LOGI("Library dimuat");
    pthread_t t;
    if (pthread_create(&t, nullptr, pekerja, nullptr) == 0) {
        pthread_detach(t);
    } else {
        LOGE("Thread gagal dibuat");
    }
}
