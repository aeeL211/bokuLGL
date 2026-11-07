#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "log.h"
#include "targets.h"

namespace {

// Field hari dan bulan terbuka dengan 2 karakter, tahun dengan 4.
int32_t hookLengthSpawn(void* self, int32_t fallback, void* method) {
    if (!features().inputText) {
        auto original =
            reinterpret_cast<int32_t (*)(void*, int32_t, void*)>(target(T_BIRTHDAY_LENGTH).orig);
        return original ? original(self, fallback, method) : fallback;
    }
    return INT32_MAX;
}

// Birthday__Refresh_ hanya menampilkan nilai bila V <= Limit. Batas bulan, hari,
// dan tahun bawaan semuanya dilonggarkan, jadi angka apa pun yang diketik tetap
// tampil di layar.
void spawnHook(void* self, int32_t v, int32_t i, int32_t limit, void* fallback, void* method) {
    LOGI("Tanggal lahir: v=%d i=%d batas=%d", v, i, limit);
    if (features().inputText && (limit == 12 || limit == 31 || limit == 9999))
        limit = INT32_MAX;
    auto original = reinterpret_cast<void (*)(void*, int32_t, int32_t, int32_t, void*, void*)>(
        target(T_BIRTHDAY_REFRESH).orig);
    if (!original)
        return;
    original(self, v, i, limit, fallback, method);
}

// Date_Check(ref Year, ref Month, ref Day) menolkan tahun di atas 9999 dan
// mengatur ulang hari yang tidak ada di bulan itu, yang membuat field tanggal
// lahir terbatas. Lewati hanya saat field tanggal lahir sendiri sedang dikirim,
// supaya pemanggil lain tetap memakai validasi tanggal bawaan.
thread_local bool changingSpawn = false;

void hookEndSpawn(void* self, void* text, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*, void*)>(target(T_BIRTHDAY_END).orig);
    if (!original)
        return;
    changingSpawn = features().inputText;
    original(self, text, method);
    changingSpawn = false;
}

void hookCheckDate(int32_t* year, int32_t* month, int32_t* day, void* method) {
    if (changingSpawn) {
        // Tidak ada yang dijepit di sini: hari dan bulan bebas memang tujuan
        // bypass ini, dan labelnya berupa teks, jadi tidak ada array yang
        // diindeks dengan nilai itu.
        LOGI("Tanggal lahir: pemeriksaan dilewati y=%d m=%d d=%d", year ? *year : -1,
             month ? *month : -1, day ? *day : -1);
        return;
    }
    auto original =
        reinterpret_cast<void (*)(int32_t*, int32_t*, int32_t*, void*)>(target(T_DATE_CHECK).orig);
    if (!original)
        return;
    original(year, month, day, method);
}

} // namespace

void installProfileHook() {
    installHook(target(T_BIRTHDAY_REFRESH), reinterpret_cast<void*>(spawnHook));
    installHook(target(T_BIRTHDAY_LENGTH), reinterpret_cast<void*>(hookLengthSpawn));
    installHook(target(T_BIRTHDAY_END), reinterpret_cast<void*>(hookEndSpawn));
    installHook(target(T_DATE_CHECK), reinterpret_cast<void*>(hookCheckDate));
}
