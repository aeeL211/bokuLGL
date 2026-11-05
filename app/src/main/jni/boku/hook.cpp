#include "hook.h"

#include "Dobby/dobby.h"
#include "log.h"

namespace {

int jumlahTerpasang = 0;

} // namespace

// Dobby tidak punya mode global yang harus dipilih sebelum hook pertama, jadi
// inisialisasi hanya menandai backend siap. Setiap DobbyHook tetap memeriksa
// hasilnya sendiri.
bool inisialisasiHook() {
    LOGI("Dobby siap");
    return true;
}

bool pasangHook(Target& entri, void* pengganti) {
    if (!entri.addr) {
        LOGE("Target tidak ditemukan: %s.%s", entri.cls, entri.name);
        return false;
    }
    // Dua berkas bisa menamai method yang sama, dan Dobby menolak alamat yang
    // sudah pernah di-hook. Target mengingat pemasangan pertamanya dan
    // melaporkan pengulangan sebagai terpasang, bukan menimpa trampoline yang
    // sudah dipanggil berkas pertama.
    if (entri.terpasang)
        return true;
    int kode = DobbyHook(entri.addr, reinterpret_cast<dobby_dummy_func_t>(pengganti),
                         reinterpret_cast<dobby_dummy_func_t*>(&entri.orig));
    if (kode != 0) {
        LOGE("Hook gagal: %s.%s (Dobby %d)", entri.cls, entri.name, kode);
        return false;
    }
    entri.terpasang = true;
    jumlahTerpasang++;
    return true;
}

int jumlahHookTerpasang() { return jumlahTerpasang; }

bool pasangHookProbe(void* alamat, void* pengganti, void** asli, const char* nama) {
    if (!alamat) {
        LOGE("Probe tidak ditemukan: %s", nama);
        return false;
    }
    int kode = DobbyHook(alamat, reinterpret_cast<dobby_dummy_func_t>(pengganti),
                         reinterpret_cast<dobby_dummy_func_t*>(asli));
    if (kode != 0) {
        LOGE("Probe gagal: %s (Dobby %d)", nama, kode);
        return false;
    }
    return true;
}
