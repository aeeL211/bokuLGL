#include "hooks.h"

#include "fitur.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field komponenUkuran = {};
Field tileManajer = {};
Field ukuranDuniaTile = {};
Field batasUkuranDunia = {};
Field multiUkuranDunia = {};
Field batasLebar = {};
Field batasTinggi = {};
Field presetTeratas = {};
Field lebarPreset = {};
Field tinggiPreset = {};
Field batasMulti = {};
bool batasSiap = false;
Field ledakManajer = {};
Field manajerZona = {};
Field zonaAktif = {};
bool zonaSiap = false;

// Host memegang izin build dan destroy dan menolak apa pun yang dicoba guest di
// luar itu, jadi kedua flag sekarang diteruskan apa adanya. Hanya flag simpan
// dunia yang masih dipaksa, karena host mengirimnya sekali di awal dan guest
// tidak mendapat kesempatan lain untuk mengaktifkannya.
void hookIzinMulai(void* self, bool build, bool destroy, bool simpan, void* method) {
    LOGI("Izin: mulai game build=%d destroy=%d", build, destroy);
    auto asli =
        reinterpret_cast<void (*)(void*, bool, bool, bool, void*)>(target(T_PERMISSION_START).orig);
    if (!asli)
        return;
    asli(self, build, destroy, fitur().simpanDunia ? true : simpan, method);
}

void hookIzinSimpan(void* self, bool izin, void* method) {
    LOGI("Izin: simpan dunia diterima");
    auto asli = reinterpret_cast<void (*)(void*, bool, void*)>(target(T_PERMISSION_SAVE).orig);
    if (!asli)
        return;
    asli(self, fitur().simpanDunia ? true : izin, method);
}

// Saat pemain menjadi tamu, host memutuskan sendiri apakah perubahan tile dan
// objek diterima. Kelima hook ini memaksa flag yang dipercaya host pada paket
// yang dikirim klien: Enforce untuk tile dan Across untuk objek. Flag lain
// diteruskan apa adanya.
void hookKirimTileAdd(void* self, float x, float y, int id, void* str, int font, bool eff,
                      bool enforce, bool callback, void* method) {
    auto asli =
        reinterpret_cast<void (*)(void*, float, float, int, void*, int, bool, bool, bool, void*)>(
            target(T_SEND_TILE_ADD).orig);
    if (!asli)
        return;
    asli(self, x, y, id, str, font, eff, fitur().izinRoomLain ? true : enforce, callback, method);
}

void hookKirimTileReplace(void* self, float rx, float ry, int rid, void* rstr, int rfont, float ax,
                          float ay, int aid, void* astr, int afont, bool eff, bool enforce,
                          bool callback, void* method) {
    auto asli =
        reinterpret_cast<void (*)(void*, float, float, int, void*, int, float, float, int, void*,
                                  int, bool, bool, bool, void*)>(target(T_SEND_TILE_REPLACE).orig);
    if (!asli)
        return;
    asli(self, rx, ry, rid, rstr, rfont, ax, ay, aid, astr, afont, eff,
         fitur().izinRoomLain ? true : enforce, callback, method);
}

void hookKirimTileRemove(void* self, float x, float y, int id, void* str, int effType, bool enforce,
                         bool destroy, bool callback, void* method) {
    auto asli =
        reinterpret_cast<void (*)(void*, float, float, int, void*, int, bool, bool, bool, void*)>(
            target(T_SEND_TILE_REMOVE).orig);
    if (!asli)
        return;
    asli(self, x, y, id, str, effType, fitur().izinRoomLain ? true : enforce, destroy, callback,
         method);
}

void hookKirimObjAdd(void* self, int z, int id, void* key, float x, float y, int dir, bool eff,
                     bool name, bool across, void* method) {
    auto asli = reinterpret_cast<void (*)(void*, int, int, void*, float, float, int, bool, bool,
                                          bool, void*)>(target(T_SEND_OBJ_ADD).orig);
    if (!asli)
        return;
    asli(self, z, id, key, x, y, dir, eff, name, fitur().izinRoomLain ? true : across, method);
}

void hookKirimObjRemove(void* self, int id, void* key, bool eff, bool name, bool across,
                        void* method) {
    auto asli = reinterpret_cast<void (*)(void*, int, void*, bool, bool, bool, void*)>(
        target(T_SEND_OBJ_REMOVE).orig);
    if (!asli)
        return;
    asli(self, id, key, eff, name, fitur().izinRoomLain ? true : across, method);
}

// Ledakan TNT tidak memakai paket Tile__Remove, melainkan Explosion_Range yang
// ditolak host saat tamu tidak punya izin destroy. Jalur admin di dalam
// Update__Range_ justru memanggil Update__Range__ di perangkat sendiri, dan
// dari sana game mengirim permintaan hapus tile serta objek yang sudah dipaksa
// oleh hook di atas. Saat fitur menyala, jalur lokal itu yang dipakai; di host
// dan single player hasilnya sama dengan bawaan.
void hookLedakRentang(void* self, float x, float y, int step, void* method) {
    auto asli = reinterpret_cast<void (*)(void*, float, float, int, void*)>(
        target(T_EXPLODE_RANGE).orig);
    if (!asli)
        return;
    Target& terapkan = target(T_EXPLODE_APPLY);
    if (!fitur().izinRoomLain || !zonaSiap || !terapkan.method) {
        asli(self, x, y, step, method);
        return;
    }
    void* zona = bacaPtr(bacaPtr(self, ledakManajer), manajerZona);
    if (!zona) {
        asli(self, x, y, step, method);
        return;
    }
    auto lokal = reinterpret_cast<void (*)(void*, int, float, float, int, void*)>(terapkan.method);
    lokal(self, bacaInt(zona, zonaAktif), x, y, step, terapkan.method);
}

// Daftar room membuang setiap room yang luas dunianya di atas batas perangkat
// ini, dan panel join serta info memakai tes yang sama. Menjawab true membuat
// room itu tetap terdaftar berapa pun ukurannya.
bool hookUkuranRoom(void* self, int lebar, int tinggi, void* method) {
    if (!fitur().ukuranRoom) {
        auto asli = reinterpret_cast<bool (*)(void*, int, int, void*)>(target(T_ROOM_SIZE).orig);
        return asli ? asli(self, lebar, tinggi, method) : false;
    }
    return true;
}

// Halaman ukuran membaca max.w dan max.h sebagai batas slider, tetapi manajer
// tile menurunkan batas itu ke tier perangkat ini saat boot, jauh sebelum hook
// ada. Mengembalikan preset teratas milik game setiap kali halaman dibuka
// melonggarkan batas itu tanpa angka buatan sendiri, dan luas room mengikuti
// batas yang baru.
void naikkanBatasDunia(void* self) {
    if (!batasSiap)
        return;
    void* manajer = bacaPtr(self, komponenUkuran);
    void* tile = bacaPtr(manajer, tileManajer);
    void* dunia = bacaPtr(tile, ukuranDuniaTile);
    void* batas = bacaPtr(dunia, batasUkuranDunia);
    void* preset = bacaPtr(batas, presetTeratas);
    int32_t lebar = bacaInt(preset, lebarPreset);
    int32_t tinggi = bacaInt(preset, tinggiPreset);
    if (lebar <= 0 || tinggi <= 0)
        return;
    int32_t lebarLama = bacaInt(batas, batasLebar);
    int32_t tinggiLama = bacaInt(batas, batasTinggi);
    if (lebarLama >= lebar && tinggiLama >= tinggi)
        return;
    tulisInt(batas, batasLebar, lebar);
    tulisInt(batas, batasTinggi, tinggi);
    tulisInt(bacaPtr(dunia, multiUkuranDunia), batasMulti, lebar * tinggi);
    LOGI("Batas ukuran dunia naik dari %dx%d ke %dx%d", lebarLama, tinggiLama, lebar, tinggi);
}

// Halaman ini menyusun slider dan ukuran saat ini di dalam panggilan ini, jadi
// batas dinaikkan sebelum fungsi asli berjalan dan bukaan pertama sudah
// melihatnya.
void hookBatas(void* self, void* method) {
    if (fitur().ukuranRoom)
        naikkanBatasDunia(self);
    auto asli = reinterpret_cast<void (*)(void*, void*)>(target(T_WORLD_CAP).orig);
    if (asli)
        asli(self, method);
}

bool cariFieldUkuranDunia() {
    if (!cariField(kelasDari("UI__Menu__World__New__Setting__Size"), "a", komponenUkuran))
        return false;
    void* kelasManajer = kelasField(komponenUkuran);
    if (!cariField(kelasManajer, "tile", tileManajer))
        return false;
    void* kelasTile = kelasField(tileManajer);
    if (!cariField(kelasTile, "_world_Size", ukuranDuniaTile))
        return false;
    void* kelasDunia = kelasField(ukuranDuniaTile);
    if (!cariField(kelasDunia, "max", batasUkuranDunia) ||
        !cariField(kelasDunia, "multi", multiUkuranDunia))
        return false;
    void* kelasBatas = kelasField(batasUkuranDunia);
    if (!cariField(kelasBatas, "w", batasLebar) || !cariField(kelasBatas, "h", batasTinggi) ||
        !cariField(kelasBatas, "_3", presetTeratas))
        return false;
    void* kelasPreset = kelasField(presetTeratas);
    return cariField(kelasPreset, "w", lebarPreset) && cariField(kelasPreset, "h", tinggiPreset) &&
           cariField(kelasField(multiUkuranDunia), "max", batasMulti);
}

bool cariFieldZonaLedak() {
    if (!cariField(kelasDari("Tile__Explode"), "a", ledakManajer))
        return false;
    if (!cariField(kelasField(ledakManajer), "zone", manajerZona))
        return false;
    return cariField(kelasField(manajerZona), "current", zonaAktif);
}

} // namespace

void pasangHookMultiplayer() {
    pasangHook(target(T_PERMISSION_START), reinterpret_cast<void*>(hookIzinMulai));
    pasangHook(target(T_PERMISSION_SAVE), reinterpret_cast<void*>(hookIzinSimpan));
    pasangHook(target(T_ROOM_SIZE), reinterpret_cast<void*>(hookUkuranRoom));
    batasSiap = cariFieldUkuranDunia();
    LOGI("Field ukuran dunia %s", batasSiap ? "OK" : "tidak ada, batas tetap per perangkat");
    pasangHook(target(T_WORLD_CAP), reinterpret_cast<void*>(hookBatas));
    pasangHook(target(T_SEND_TILE_ADD), reinterpret_cast<void*>(hookKirimTileAdd));
    pasangHook(target(T_SEND_TILE_REPLACE), reinterpret_cast<void*>(hookKirimTileReplace));
    pasangHook(target(T_SEND_TILE_REMOVE), reinterpret_cast<void*>(hookKirimTileRemove));
    pasangHook(target(T_SEND_OBJ_ADD), reinterpret_cast<void*>(hookKirimObjAdd));
    pasangHook(target(T_SEND_OBJ_REMOVE), reinterpret_cast<void*>(hookKirimObjRemove));
    zonaSiap = cariFieldZonaLedak();
    LOGI("Field zona ledakan %s", zonaSiap ? "OK" : "tidak ada, ledakan tetap lewat host");
    pasangHook(target(T_EXPLODE_RANGE), reinterpret_cast<void*>(hookLedakRentang));
}
