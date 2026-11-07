#include "hook_manager.h"

#include <shadowhook.h>

#include <atomic>
#include <mutex>

#include "log.h"

namespace {

std::atomic<int> jumlahTerpasang{0};
std::atomic<bool> hookSiap{false};
// ShadowHook menyimpan hasil init-nya sendiri, jadi kegagalan di sini bersifat
// permanen. Diingat supaya hook berikutnya tidak mencoba lagi dan tidak
// membanjiri logcat dengan pesan yang sama.
std::atomic<bool> hookGagal{false};

// Hanya satu thread yang boleh memanggil shadowhook_init. Flag di atas sudah
// menangani jalur cepat; kunci ini menutup balapan saat dua pemanggil pertama
// datang bersamaan.
std::mutex kunciInit;

#ifdef BOKU_HOOK_DEBUG
constexpr bool kDebugHook = true;
#else
constexpr bool kDebugHook = false;
#endif

} // namespace

// ShadowHook harus diinisialisasi sekali sebelum hook pertama. Mode multi
// dipakai karena setiap proxy di sini selalu memanggil aslinya lewat
// Target::orig: biayanya sama dengan mode unique, tetapi dua pemasangan pada
// satu alamat tetap diterima dan dirantai, bukan gagal. Mode shared tidak
// dipakai karena proxy-nya wajib memakai SHADOWHOOK_CALL_PREV dan
// SHADOWHOOK_STACK_SCOPE.
bool inisialisasiHook() {
    if (hookSiap.load(std::memory_order_acquire))
        return true;
    if (hookGagal.load(std::memory_order_acquire))
        return false;

    std::lock_guard<std::mutex> tahan(kunciInit);
    if (hookSiap.load(std::memory_order_relaxed))
        return true;
    if (hookGagal.load(std::memory_order_relaxed))
        return false;

    int kode = shadowhook_init(SHADOWHOOK_MODE_MULTI, kDebugHook);
    if (kode != SHADOWHOOK_ERRNO_OK) {
        hookGagal.store(true, std::memory_order_release);
        LOGE("ShadowHook gagal diinisialisasi: %d %s", kode, shadowhook_to_errmsg(kode));
        return false;
    }
    hookSiap.store(true, std::memory_order_release);
    LOGI("ShadowHook siap (%s, mode multi)", shadowhook_get_version());
    // Patch sh_linker.c melewati pemantau dlopen karena libshadowhook_nothing.so
    // tidak dipaketkan. Semua target berada di pustaka yang sudah dimuat dan
    // tidak pernah dilepas, jadi hook tidak bergantung pada pemantau itu.
    LOGI("Pemantau dlopen tidak dipakai");
    return true;
}

bool pasangHook(Target& entri, void* pengganti) {
    if (!entri.addr) {
        LOGE("Target tidak ditemukan: %s.%s", entri.cls, entri.name);
        return false;
    }
    // Dua berkas bisa menamai method yang sama. Target mengingat pemasangan
    // pertamanya supaya rantai multi tidak memanggil proxy yang sama dua kali.
    if (entri.terpasang)
        return true;
    if (!inisialisasiHook())
        return false;

    void* stub = shadowhook_hook_func_addr_2(entri.addr, pengganti, &entri.orig,
                                             SHADOWHOOK_HOOK_WITH_MULTI_MODE);
    if (!stub) {
        int kode = shadowhook_get_errno();
        LOGE("Hook gagal: %s.%s (ShadowHook %d: %s)", entri.cls, entri.name, kode,
             shadowhook_to_errmsg(kode));
        return false;
    }
    entri.stub = stub;
    entri.terpasang = true;
    jumlahTerpasang.fetch_add(1, std::memory_order_relaxed);
    return true;
}

int jumlahHookTerpasang() { return jumlahTerpasang.load(std::memory_order_relaxed); }

bool pasangHookProbe(void* alamat, void* pengganti, void** asli, const char* nama) {
    if (!alamat) {
        LOGE("Probe tidak ditemukan: %s", nama);
        return false;
    }
    if (!pengganti || !asli) {
        LOGE("Probe tidak valid: %s", nama);
        return false;
    }
    // Probe hanya dipasang sekali, jadi jangan merantai proxy yang sama dua kali.
    if (*asli)
        return true;
    if (!inisialisasiHook())
        return false;

    void* stub =
        shadowhook_hook_func_addr_2(alamat, pengganti, asli, SHADOWHOOK_HOOK_WITH_MULTI_MODE);
    if (!stub) {
        int kode = shadowhook_get_errno();
        LOGE("Probe gagal: %s (ShadowHook %d: %s)", nama, kode, shadowhook_to_errmsg(kode));
        return false;
    }
    return true;
}
