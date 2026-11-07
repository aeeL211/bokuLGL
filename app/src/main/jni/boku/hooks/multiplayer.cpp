#include "hooks.h"

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field componentSize = {};
Field tileManager = {};
Field worldSizeTile = {};
Field worldSizeLimit = {};
Field multiSizeWorld = {};
Field widthLimit = {};
Field heightLimit = {};
Field presetTeratas = {};
Field widthPreset = {};
Field heightPreset = {};
Field limitMulti = {};
bool limitReady = false;
Field explodeManager = {};
Field managerZone = {};
Field zoneActive = {};
bool zoneReady = false;

// Host memegang izin build dan destroy dan menolak apa pun yang dicoba guest di
// luar itu, jadi kedua flag sekarang diteruskan apa adanya. Hanya flag simpan
// dunia yang masih dipaksa, karena host mengirimnya sekali di awal dan guest
// tidak mendapat kesempatan lain untuk mengaktifkannya.
void hookPermissionStart(void* self, bool build, bool destroy, bool save, void* method) {
    LOGI("Izin: mulai game build=%d destroy=%d", build, destroy);
    auto original =
        reinterpret_cast<void (*)(void*, bool, bool, bool, void*)>(target(T_PERMISSION_START).orig);
    if (!original)
        return;
    original(self, build, destroy, features().saveWorld ? true : save, method);
}

void hookPermissionSave(void* self, bool permission, void* method) {
    LOGI("Izin: simpan dunia diterima");
    auto original = reinterpret_cast<void (*)(void*, bool, void*)>(target(T_PERMISSION_SAVE).orig);
    if (!original)
        return;
    original(self, features().saveWorld ? true : permission, method);
}

// Saat pemain menjadi tamu, host memutuskan sendiri apakah perubahan tile dan
// objek diterima. Kelima hook ini memaksa flag yang dipercaya host pada paket
// yang dikirim klien: Enforce untuk tile dan Across untuk objek. Flag lain
// diteruskan apa adanya.
void hookSendTileAdd(void* self, float x, float y, int id, void* str, int font, bool eff,
                      bool enforce, bool callback, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, float, float, int, void*, int, bool, bool, bool, void*)>(
            target(T_SEND_TILE_ADD).orig);
    if (!original)
        return;
    original(self, x, y, id, str, font, eff, features().permissionRoomOther ? true : enforce, callback, method);
}

void hookSendTileReplace(void* self, float rx, float ry, int rid, void* rstr, int rfont, float ax,
                          float ay, int aid, void* astr, int afont, bool eff, bool enforce,
                          bool callback, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, float, float, int, void*, int, float, float, int, void*,
                                  int, bool, bool, bool, void*)>(target(T_SEND_TILE_REPLACE).orig);
    if (!original)
        return;
    original(self, rx, ry, rid, rstr, rfont, ax, ay, aid, astr, afont, eff,
         features().permissionRoomOther ? true : enforce, callback, method);
}

void hookSendTileRemove(void* self, float x, float y, int id, void* str, int effType, bool enforce,
                         bool destroy, bool callback, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, float, float, int, void*, int, bool, bool, bool, void*)>(
            target(T_SEND_TILE_REMOVE).orig);
    if (!original)
        return;
    original(self, x, y, id, str, effType, features().permissionRoomOther ? true : enforce, destroy, callback,
         method);
}

void hookSendObjAdd(void* self, int z, int id, void* key, float x, float y, int dir, bool eff,
                     bool name, bool across, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int, int, void*, float, float, int, bool, bool,
                                          bool, void*)>(target(T_SEND_OBJ_ADD).orig);
    if (!original)
        return;
    original(self, z, id, key, x, y, dir, eff, name, features().permissionRoomOther ? true : across, method);
}

void hookSendObjRemove(void* self, int id, void* key, bool eff, bool name, bool across,
                        void* method) {
    auto original = reinterpret_cast<void (*)(void*, int, void*, bool, bool, bool, void*)>(
        target(T_SEND_OBJ_REMOVE).orig);
    if (!original)
        return;
    original(self, id, key, eff, name, features().permissionRoomOther ? true : across, method);
}

// Ledakan TNT tidak memakai paket Tile__Remove, melainkan Explosion_Range yang
// ditolak host saat tamu tidak punya izin destroy. Jalur admin di dalam
// Update__Range_ justru memanggil Update__Range__ di perangkat sendiri, dan
// dari sana game mengirim permintaan hapus tile serta objek yang sudah dipaksa
// oleh hook di atas. Saat fitur menyala, jalur lokal itu yang dipakai; di host
// dan single player hasilnya sama dengan bawaan.
void hookExplodeRange(void* self, float x, float y, int step, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, float, float, int, void*)>(target(T_EXPLODE_RANGE).orig);
    if (!original)
        return;
    Target& apply = target(T_EXPLODE_APPLY);
    if (!features().permissionRoomOther || !zoneReady || !apply.addr || !apply.method) {
        original(self, x, y, step, method);
        return;
    }
    void* zone = readPtr(readPtr(self, explodeManager), managerZone);
    if (!zone) {
        original(self, x, y, step, method);
        return;
    }
    // addr adalah alamat kode method; method hanya MethodInfo yang menjadi
    // argumen tersembunyi terakhir. Memanggil MethodInfo sebagai fungsi akan
    // melompat ke data dan membuat game hang.
    auto local = reinterpret_cast<void (*)(void*, int, float, float, int, void*)>(apply.addr);
    local(self, readInt(zone, zoneActive), x, y, step, apply.method);
}

// Daftar room membuang setiap room yang luas dunianya di atas batas perangkat
// ini, dan panel join serta info memakai tes yang sama. Menjawab true membuat
// room itu tetap terdaftar berapa pun ukurannya.
bool hookSizeRoom(void* self, int width, int height, void* method) {
    if (!features().roomSize) {
        auto original = reinterpret_cast<bool (*)(void*, int, int, void*)>(target(T_ROOM_SIZE).orig);
        return original ? original(self, width, height, method) : false;
    }
    return true;
}

// Halaman ukuran membaca max.w dan max.h sebagai batas slider, tetapi manajer
// tile menurunkan batas itu ke tier perangkat ini saat boot, jauh sebelum hook
// ada. Mengembalikan preset teratas milik game setiap kali halaman dibuka
// melonggarkan batas itu tanpa angka buatan sendiri, dan luas room mengikuti
// batas yang baru.
void raiseLimitWorld(void* self) {
    if (!limitReady)
        return;
    void* manager = readPtr(self, componentSize);
    void* tile = readPtr(manager, tileManager);
    void* world = readPtr(tile, worldSizeTile);
    void* limit = readPtr(world, worldSizeLimit);
    void* preset = readPtr(limit, presetTeratas);
    int32_t width = readInt(preset, widthPreset);
    int32_t height = readInt(preset, heightPreset);
    if (width <= 0 || height <= 0)
        return;
    int32_t widthOld = readInt(limit, widthLimit);
    int32_t heightOld = readInt(limit, heightLimit);
    if (widthOld >= width && heightOld >= height)
        return;
    writeInt(limit, widthLimit, width);
    writeInt(limit, heightLimit, height);
    writeInt(readPtr(world, multiSizeWorld), limitMulti, width * height);
    LOGI("Batas ukuran dunia naik dari %dx%d ke %dx%d", widthOld, heightOld, width, height);
}

// Halaman ini menyusun slider dan ukuran saat ini di dalam panggilan ini, jadi
// batas dinaikkan sebelum fungsi asli berjalan dan bukaan pertama sudah
// melihatnya.
void hookLimit(void* self, void* method) {
    if (features().roomSize)
        raiseLimitWorld(self);
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_WORLD_CAP).orig);
    if (original)
        original(self, method);
}

bool findFieldSizeWorld() {
    if (!findField(classOf("UI__Menu__World__New__Setting__Size"), "a", componentSize))
        return false;
    void* managerClass = fieldClass(componentSize);
    if (!findField(managerClass, "tile", tileManager))
        return false;
    void* klassTile = fieldClass(tileManager);
    if (!findField(klassTile, "_world_Size", worldSizeTile))
        return false;
    void* klassWorld = fieldClass(worldSizeTile);
    if (!findField(klassWorld, "max", worldSizeLimit) ||
        !findField(klassWorld, "multi", multiSizeWorld))
        return false;
    void* klassLimit = fieldClass(worldSizeLimit);
    if (!findField(klassLimit, "w", widthLimit) || !findField(klassLimit, "h", heightLimit) ||
        !findField(klassLimit, "_3", presetTeratas))
        return false;
    void* klassPreset = fieldClass(presetTeratas);
    return findField(klassPreset, "w", widthPreset) && findField(klassPreset, "h", heightPreset) &&
           findField(fieldClass(multiSizeWorld), "max", limitMulti);
}

bool findFieldZoneExplode() {
    if (!findField(classOf("Tile__Explode"), "a", explodeManager))
        return false;
    if (!findField(fieldClass(explodeManager), "zone", managerZone))
        return false;
    return findField(fieldClass(managerZone), "current", zoneActive);
}

} // namespace

void installMultiplayerHook() {
    installHook(target(T_PERMISSION_START), reinterpret_cast<void*>(hookPermissionStart));
    installHook(target(T_PERMISSION_SAVE), reinterpret_cast<void*>(hookPermissionSave));
    installHook(target(T_ROOM_SIZE), reinterpret_cast<void*>(hookSizeRoom));
    limitReady = findFieldSizeWorld();
    LOGI("Field ukuran dunia %s", limitReady ? "OK" : "tidak ada, batas tetap per perangkat");
    installHook(target(T_WORLD_CAP), reinterpret_cast<void*>(hookLimit));
    installHook(target(T_SEND_TILE_ADD), reinterpret_cast<void*>(hookSendTileAdd));
    installHook(target(T_SEND_TILE_REPLACE), reinterpret_cast<void*>(hookSendTileReplace));
    installHook(target(T_SEND_TILE_REMOVE), reinterpret_cast<void*>(hookSendTileRemove));
    installHook(target(T_SEND_OBJ_ADD), reinterpret_cast<void*>(hookSendObjAdd));
    installHook(target(T_SEND_OBJ_REMOVE), reinterpret_cast<void*>(hookSendObjRemove));
    zoneReady = findFieldZoneExplode();
    LOGI("Field zona ledakan %s", zoneReady ? "OK" : "tidak ada, ledakan tetap lewat host");
    installHook(target(T_EXPLODE_RANGE), reinterpret_cast<void*>(hookExplodeRange));
}
