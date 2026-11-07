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
Field presetTop = {};
Field widthPreset = {};
Field heightPreset = {};
Field limitMulti = {};
bool limitReady = false;
Field explodeManager = {};
Field managerZone = {};
Field zoneActive = {};
bool zoneReady = false;

// The host holds build and destroy permission and rejects anything guests try
// outside that, so both flags are now passed through as-is. Only the save world
// flag is still forced, because the host sends it once at the start and the guest
// gets no other chance to enable it.
void hookPermissionStart(void* self, bool build, bool destroy, bool save, void* method) {
    LOGI("Permission: game start build=%d destroy=%d", build, destroy);
    auto original =
        reinterpret_cast<void (*)(void*, bool, bool, bool, void*)>(target(T_PERMISSION_START).orig);
    if (!original)
        return;
    original(self, build, destroy, features().saveWorld ? true : save, method);
}

void hookPermissionSave(void* self, bool permission, void* method) {
    LOGI("Permission: save world accepted");
    auto original = reinterpret_cast<void (*)(void*, bool, void*)>(target(T_PERMISSION_SAVE).orig);
    if (!original)
        return;
    original(self, features().saveWorld ? true : permission, method);
}

// When the player becomes a guest, the host decides on its own whether tile and
// object changes are accepted. These five hooks force the flag the host trusts on
// the packets the client sends: Enforce for tiles and Across for objects. The
// other flags are passed through as-is.
void hookSendTileAdd(void* self, float x, float y, int id, void* str, int font, bool eff,
                     bool enforce, bool callback, void* method) {
    auto original =
        reinterpret_cast<void (*)(void*, float, float, int, void*, int, bool, bool, bool, void*)>(
            target(T_SEND_TILE_ADD).orig);
    if (!original)
        return;
    original(self, x, y, id, str, font, eff, features().permissionRoomOther ? true : enforce,
             callback, method);
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
    original(self, x, y, id, str, effType, features().permissionRoomOther ? true : enforce, destroy,
             callback, method);
}

void hookSendObjAdd(void* self, int z, int id, void* key, float x, float y, int dir, bool eff,
                    bool name, bool across, void* method) {
    auto original = reinterpret_cast<void (*)(void*, int, int, void*, float, float, int, bool, bool,
                                              bool, void*)>(target(T_SEND_OBJ_ADD).orig);
    if (!original)
        return;
    original(self, z, id, key, x, y, dir, eff, name, features().permissionRoomOther ? true : across,
             method);
}

void hookSendObjRemove(void* self, int id, void* key, bool eff, bool name, bool across,
                       void* method) {
    auto original = reinterpret_cast<void (*)(void*, int, void*, bool, bool, bool, void*)>(
        target(T_SEND_OBJ_REMOVE).orig);
    if (!original)
        return;
    original(self, id, key, eff, name, features().permissionRoomOther ? true : across, method);
}

// The TNT explosion does not use the Tile__Remove packet but Explosion_Range,
// which the host rejects when the guest has no destroy permission. The admin path
// inside Update__Range_ instead calls Update__Range__ on the local device, and
// from there the game sends the tile and object removal requests that the hooks
// above already force. When the feature is on, that local path is used; on the
// host and in single player the result matches vanilla.
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
    // addr is the code address of the method; method is only the MethodInfo that
    // is the last hidden argument. Calling the MethodInfo as a function would
    // jump into data and hang the game.
    auto local = reinterpret_cast<void (*)(void*, int, float, float, int, void*)>(apply.addr);
    local(self, readInt(zone, zoneActive), x, y, step, apply.method);
}

// The room list drops every room whose world size is above this device's limit,
// and the join panel and the info panel use the same test. Answering true keeps
// that room listed whatever its size.
bool hookSizeRoom(void* self, int width, int height, void* method) {
    if (!features().roomSize) {
        auto original =
            reinterpret_cast<bool (*)(void*, int, int, void*)>(target(T_ROOM_SIZE).orig);
        return original ? original(self, width, height, method) : false;
    }
    return true;
}

// The size page reads max.w and max.h as the slider limits, but the tile
// manager lowers that limit to this device tier at boot, long before the hook
// exists. Restoring the game's top preset each time the page opens loosens
// that limit without invented numbers, and the room area follows the new
// limit.
void raiseLimitWorld(void* self) {
    if (!limitReady)
        return;
    void* manager = readPtr(self, componentSize);
    void* tile = readPtr(manager, tileManager);
    void* world = readPtr(tile, worldSizeTile);
    void* limit = readPtr(world, worldSizeLimit);
    void* preset = readPtr(limit, presetTop);
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
    LOGI("World size limit raised from %dx%d to %dx%d", widthOld, heightOld, width, height);
}

// This page builds the slider and the current size inside this call, so the
// limit is raised before the original runs and the first open already
// sees it.
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
        !findField(klassLimit, "_3", presetTop))
        return false;
    void* klassPreset = fieldClass(presetTop);
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
    LOGI("World size field %s", limitReady ? "OK" : "missing, limit stays per device");
    installHook(target(T_WORLD_CAP), reinterpret_cast<void*>(hookLimit));
    installHook(target(T_SEND_TILE_ADD), reinterpret_cast<void*>(hookSendTileAdd));
    installHook(target(T_SEND_TILE_REPLACE), reinterpret_cast<void*>(hookSendTileReplace));
    installHook(target(T_SEND_TILE_REMOVE), reinterpret_cast<void*>(hookSendTileRemove));
    installHook(target(T_SEND_OBJ_ADD), reinterpret_cast<void*>(hookSendObjAdd));
    installHook(target(T_SEND_OBJ_REMOVE), reinterpret_cast<void*>(hookSendObjRemove));
    zoneReady = findFieldZoneExplode();
    LOGI("Explosion zone field %s",
         zoneReady ? "OK" : "missing, explosion still goes through host");
    installHook(target(T_EXPLODE_RANGE), reinterpret_cast<void*>(hookExplodeRange));
}
