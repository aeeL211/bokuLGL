#include "hook_manager.h"

#include <shadowhook.h>

#include <atomic>
#include <mutex>

#include "log.h"

namespace {

std::atomic<int> countInstalled{0};
std::atomic<bool> hookReady{false};
// ShadowHook menyimpan hasil init-nya sendiri, jadi kegagalan di sini bersifat
// permanen. Diingat supaya hook berikutnya tidak mencoba lagi dan tidak
// membanjiri logcat dengan pesan yang sama.
std::atomic<bool> hookFail{false};

// Hanya satu thread yang boleh memanggil shadowhook_init. Flag di atas sudah
// menangani jalur cepat; kunci ini menutup balapan saat dua pemanggil pertama
// datang bersamaan.
std::mutex keyInit;

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
bool initializeHooks() {
    if (hookReady.load(std::memory_order_acquire))
        return true;
    if (hookFail.load(std::memory_order_acquire))
        return false;

    std::lock_guard<std::mutex> hold(keyInit);
    if (hookReady.load(std::memory_order_relaxed))
        return true;
    if (hookFail.load(std::memory_order_relaxed))
        return false;

    int code = shadowhook_init(SHADOWHOOK_MODE_MULTI, kDebugHook);
    if (code != SHADOWHOOK_ERRNO_OK) {
        hookFail.store(true, std::memory_order_release);
        LOGE("ShadowHook gagal diinisialisasi: %d %s", code, shadowhook_to_errmsg(code));
        return false;
    }
    hookReady.store(true, std::memory_order_release);
    LOGI("ShadowHook siap (%s, mode multi)", shadowhook_get_version());
    // Patch sh_linker.c melewati pemantau dlopen karena libshadowhook_nothing.so
    // tidak dipaketkan. Semua target berada di pustaka yang sudah dimuat dan
    // tidak pernah dilepas, jadi hook tidak bergantung pada pemantau itu.
    LOGI("Pemantau dlopen tidak dipakai");
    return true;
}

bool installHook(Target& entry, void* pengganti) {
    if (!entry.addr) {
        LOGE("Target tidak ditemukan: %s.%s", entry.cls, entry.name);
        return false;
    }
    // Dua berkas bisa menamai method yang sama. Target mengingat pemasangan
    // pertamanya supaya rantai multi tidak memanggil proxy yang sama dua kali.
    if (entry.installed)
        return true;
    if (!initializeHooks())
        return false;

    void* stub = shadowhook_hook_func_addr_2(entry.addr, pengganti, &entry.orig,
                                             SHADOWHOOK_HOOK_WITH_MULTI_MODE);
    if (!stub) {
        int code = shadowhook_get_errno();
        LOGE("Hook gagal: %s.%s (ShadowHook %d: %s)", entry.cls, entry.name, code,
             shadowhook_to_errmsg(code));
        return false;
    }
    entry.stub = stub;
    entry.installed = true;
    countInstalled.fetch_add(1, std::memory_order_relaxed);
    return true;
}

int countHookInstalled() { return countInstalled.load(std::memory_order_relaxed); }

bool installProbeHook(void* address, void* pengganti, void** original, const char* name) {
    if (!address) {
        LOGE("Probe tidak ditemukan: %s", name);
        return false;
    }
    if (!pengganti || !original) {
        LOGE("Probe tidak valid: %s", name);
        return false;
    }
    // Probe hanya dipasang sekali, jadi jangan merantai proxy yang sama dua kali.
    if (*original)
        return true;
    if (!initializeHooks())
        return false;

    void* stub =
        shadowhook_hook_func_addr_2(address, pengganti, original, SHADOWHOOK_HOOK_WITH_MULTI_MODE);
    if (!stub) {
        int code = shadowhook_get_errno();
        LOGE("Probe gagal: %s (ShadowHook %d: %s)", name, code, shadowhook_to_errmsg(code));
        return false;
    }
    return true;
}
