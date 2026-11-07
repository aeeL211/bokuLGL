#include "hooks.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "feature_flags.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "player_service.h"
#include "targets.h"

namespace {

// Vector2 is just two consecutive floats, written straight into the game struct field.
struct Vector2 {
    float x;
    float y;
};

using FnDetect = void (*)(void*, void**, int32_t, float, float, void*);
using FnPosCheck = void (*)(void*, void**, void*);

// Limits used by No Clip. The values sit far beyond any world size, so the
// collision check can no longer shrink them again.
constexpr float kLimitFree = 1.0e9f;
constexpr int32_t kLimitTeleport = 1024;

Field fieldActorPlayer = {};
Field localPlayerField = {};
Field fieldActorPhysics = {};
Field fieldDetectActor = {};

// Step limit used by the game's Speed_Limit. The value equals
// _Tile.pixel_Size - 1 and is also read by the camera as the maximum distance
// before moving, so the scaled speed result must stay within it.
Field fieldLimitSpeed = {};
Field fieldLimitSpeedValue = {};

// World height is used to translate menu Y into the game UI Y.
// Tile__Manager.Display__Y computes UI Y = tile Y - h/2, so
// tile Y = UI Y + h/2.
Field fieldActorTile = {};
Field worldSizeField = {};
Field fieldHeightWorld = {};

// The game's tile position lives in Physics.pos, separate from the Unity Transform.
// The Transform is only a visual rewritten every frame, so teleport writes the
// physics position together with all of its trail points.
Field fieldPhysics = {};
Field fieldPosition = {};
Field fieldWorld = {};
Field fieldTile = {};
Field fieldTrail = {};
Field fieldTrailPrev = {};
Field fieldTrailCheck = {};
Field fieldTrailNew = {};
Field fieldTrailEnd = {};
Field fieldFresh = {};
Field fieldSpeed = {};
Field fieldSpeedX = {};
Field fieldSpeedY = {};
Field fieldSpeedAdd = {};
Field fieldSpeedAddX = {};
Field fieldSpeedAddY = {};

// The initial teleport scale comes from the game tile size, with the player
// collider as fallback: size is the size in world units, while tile_Size
// is the number of tiles occupied.
Field fieldCollider = {};
Field fieldSizeCollider = {};
Field fieldSizeTile = {};
Field fieldSizeTileW = {};
Field fieldSizeTileH = {};
Field fieldSizePixel = {};
// The _Tile name is used by two different classes, so the class that has pixel_Size
// is bound once when the hook is installed. The full scan is not repeated every frame.
void* klassTile = nullptr;

// A tile can be a fractional Vector2 or a Vector2Int. The field type is read once
// when the hook is installed so writes do not guess the width of each field.
bool tileRound = false;

// Margin is the movement box recalculated every frame from the tiles around the
// body. The left, right, and top sides are opened, while the bottom is left
// alone so the floor is still stepped on.
Field fieldMargin = {};
Field fieldMarginTop = {};
Field fieldMarginLeft = {};
Field fieldMarginRight = {};

// Pos.tile and Pos.world use different coordinate spaces. The difference between
// them gives one scale valid for both axes, so teleport does not need to guess the
// Y zero point from the world/tile ratio, which can be off.
struct Calibration {
    bool ready;
    bool certain;
    bool fromMotion;
    float scale;
    int sample;
    float offsetX;
    float offsetY;
    bool hasReference;
    float referenceWorldX;
    float referenceWorldY;
    float referenceTileX;
    float referenceTileY;
};
Calibration calibration = {};

// Speed and gravity only change the local player physics. The game's original
// values are never overwritten permanently: the multiplier is installed for one
// game method call and then the values are restored.
int logSpeed = -1;
int logGravity = -1;
int logHeightWorld = -1;

struct VerifyTeleport {
    int remaining;
    float tileX;
    float tileY;
    Vector2 world;
};
VerifyTeleport verify = {};

bool localPlayer(void* manager) {
    void* global = readPtr(manager, fieldActorPlayer);
    return global && readPtr(global, localPlayerField) == manager;
}

bool readVector(void* object, const Field& field, Vector2& value) {
    if (!object || !field.field)
        return false;
    Field y = field;
    y.offset += sizeof(float);
    value.x = readFloat(object, field);
    value.y = readFloat(object, y);
    return std::isfinite(value.x) && std::isfinite(value.y);
}

void writeVector(void* object, Field field, const Vector2& value) {
    if (!object || !field.field)
        return;
    writeFloat(object, field, value.x);
    field.offset += sizeof(float);
    writeFloat(object, field, value.y);
}

bool readTile(void* position, Vector2& value) {
    if (!position || !fieldTile.field)
        return false;
    if (!tileRound)
        return readVector(position, fieldTile, value);
    Field y = fieldTile;
    y.offset += sizeof(int32_t);
    value.x = static_cast<float>(readInt(position, fieldTile));
    value.y = static_cast<float>(readInt(position, y));
    return std::isfinite(value.x) && std::isfinite(value.y);
}

// Rounded tiles must not use library rounding: the value is written to an int32
// field, so just shift by half toward the right direction before truncating.
int32_t roundTile(float value) {
    return static_cast<int32_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

void writeTile(void* position, float x, float y) {
    if (!position || !fieldTile.field)
        return;
    if (!tileRound) {
        Vector2 value = {x, y};
        writeVector(position, fieldTile, value);
        return;
    }
    writeInt(position, fieldTile, roundTile(x));
    Field fieldY = fieldTile;
    fieldY.offset += sizeof(int32_t);
    writeInt(position, fieldY, roundTile(y));
}

// World height in tiles. The game UI puts Y 0 at the world center, so
// UI Y = tile Y - h/2. If the field is not readable yet, 0 means no shift and
// tile Y is used as-is.
int32_t heightWorldTile(void* managerPlayer) {
    if (!managerPlayer || !fieldActorPlayer.field || !fieldActorTile.field ||
        !worldSizeField.field || !fieldHeightWorld.field)
        return 0;
    void* manager = readPtr(managerPlayer, fieldActorPlayer);
    void* tile = manager ? readPtr(manager, fieldActorTile) : nullptr;
    void* size = tile ? readPtr(tile, worldSizeField) : nullptr;
    int32_t height = size ? readInt(size, fieldHeightWorld) : 0;
    return height > 0 && height <= 100000 ? height : 0;
}

void storePosition(bool hasTile, const Vector2& tile, const Vector2& world) {
    player::storePosition(hasTile, tile.x, tile.y, world.x, world.y);
}

// Player movement gives a (tile, world) pair. Their ratio is the world units
// per tile, and the scale is the same for both axes. A new sample is used only
// after a move of at least four tiles so tile number rounding does not dominate
// the result.
bool addSample(float deltaTile, float deltaWorld, const char* axis) {
    if (fabsf(deltaTile) < 4.0f)
        return false;
    float candidate = deltaWorld / deltaTile;
    if (!std::isfinite(candidate) || candidate < 0.05f || candidate > 4096.0f)
        return false;
    if (!calibration.fromMotion) {
        calibration.scale = candidate;
        calibration.fromMotion = true;
        calibration.sample = 1;
        LOGI("Teleport: scale %.2f from %s motion", candidate, axis);
        return true;
    }
    calibration.scale = calibration.scale * 0.8f + candidate * 0.2f;
    calibration.sample++;
    if (calibration.sample % 8 == 0)
        LOGI("Teleport: scale %.2f from %d samples", calibration.scale, calibration.sample);
    return true;
}

// The initial teleport scale comes from the game tile size. If that field is not
// readable yet, the player collider is the fallback: size is the body size in
// world units, while tile_Size is the number of tiles occupied. Player motion
// replaces the collider guess on the first sample.
void tryScaleInitial(void* physics) {
    if (calibration.certain || !physics)
        return;

    // _Tile.pixel_Size is the size of one tile in world units and applies to
    // both axes, so its value does not depend on the player model.
    if (klassTile && fieldSizePixel.field) {
        void* staticFlag = dataFieldStatic(klassTile);
        int32_t pixels = staticFlag ? readInt(staticFlag, fieldSizePixel) : 0;
        if (pixels > 0) {
            calibration.scale = static_cast<float>(pixels);
            calibration.ready = true;
            calibration.certain = true;
            LOGI("Teleport: initial scale %.2f from pixel_Size", calibration.scale);
            return;
        }
    }

    if (calibration.ready)
        return;

    if (!fieldCollider.field || !fieldSizeCollider.field || !fieldSizeTile.field ||
        !fieldSizeTileW.field || !fieldSizeTileH.field)
        return;
    void* collider = readPtr(physics, fieldCollider);
    if (!collider)
        return;
    void* tileSize = readPtr(collider, fieldSizeTile);
    if (!tileSize)
        return;

    Vector2 size = {};
    if (!readVector(collider, fieldSizeCollider, size))
        return;
    float tileW = readFloat(tileSize, fieldSizeTileW);
    float tileH = readFloat(tileSize, fieldSizeTileH);

    float candidate = 0.0f;
    if (std::isfinite(tileW) && tileW > 0.01f && size.x > 0.01f)
        candidate = size.x / tileW;
    else if (std::isfinite(tileH) && tileH > 0.01f && size.y > 0.01f)
        candidate = size.y / tileH;
    if (!std::isfinite(candidate) || candidate < 0.05f || candidate > 4096.0f)
        return;

    calibration.scale = candidate;
    calibration.ready = true;
    LOGI("Teleport: initial scale %.2f from collider", candidate);
}

void updateCalibration(const Vector2& world, const Vector2& tile) {
    if (!calibration.hasReference) {
        calibration.referenceWorldX = world.x;
        calibration.referenceWorldY = world.y;
        calibration.referenceTileX = tile.x;
        calibration.referenceTileY = tile.y;
        float scale = calibration.ready ? calibration.scale : 1.0f;
        calibration.offsetX = world.x - tile.x * scale;
        calibration.offsetY = world.y - tile.y * scale;
        calibration.hasReference = true;
        return;
    }

    // The scale from pixel_Size is certain, so motion samples must not replace
    // it. The zero point is still updated for the path without a tile field.
    if (calibration.certain) {
        calibration.offsetX = world.x - tile.x * calibration.scale;
        calibration.offsetY = world.y - tile.y * calibration.scale;
        return;
    }

    float deltaTileX = tile.x - calibration.referenceTileX;
    float deltaTileY = tile.y - calibration.referenceTileY;
    if (fabsf(deltaTileX) < 0.5f && fabsf(deltaTileY) < 0.5f)
        return;

    bool accepted = false;
    if (fabsf(deltaTileX) >= fabsf(deltaTileY)) {
        accepted = addSample(deltaTileX, world.x - calibration.referenceWorldX, "X");
    } else {
        accepted = addSample(deltaTileY, world.y - calibration.referenceWorldY, "Y");
    }
    if (!accepted)
        return;

    calibration.ready = true;
    calibration.referenceWorldX = world.x;
    calibration.referenceWorldY = world.y;
    calibration.referenceTileX = tile.x;
    calibration.referenceTileY = tile.y;
    calibration.offsetX = world.x - tile.x * calibration.scale;
    calibration.offsetY = world.y - tile.y * calibration.scale;
}

Vector2 toWorld(float tileX, float tileY, const Vector2& currentWorld, const Vector2& tileNow,
                bool hasTileNow) {
    float scale = calibration.ready ? calibration.scale : 1.0f;
    // The current reference point cancels the fixed offset between tile and
    // world, so this formula stays correct even if the Y axis zero point was
    // never computed.
    if (hasTileNow) {
        Vector2 result = {currentWorld.x + (tileX - tileNow.x) * scale,
                          currentWorld.y + (tileY - tileNow.y) * scale};
        return result;
    }
    Vector2 result = {tileX * scale, tileY * scale};
    if (calibration.hasReference) {
        result.x = calibration.offsetX + tileX * scale;
        result.y = calibration.offsetY + tileY * scale;
    }
    return result;
}

bool writePositionPhysics(void* position, float tileX, float tileY, const Vector2& world) {
    if (!position || !fieldWorld.field)
        return false;

    writeVector(position, fieldWorld, world);
    writeTile(position, tileX, tileY);

    void* trail = readPtr(position, fieldTrail);
    if (trail) {
        writeVector(trail, fieldTrailPrev, world);
        writeVector(trail, fieldTrailCheck, world);
        writeVector(trail, fieldTrailNew, world);
        writeVector(trail, fieldTrailEnd, world);
    }
    // The next reference point is the teleport target, not the old point whose
    // distance can be thousands of tiles, so calibration does not use this jump
    // as a motion sample.
    calibration.hasReference = true;
    calibration.referenceWorldX = world.x;
    calibration.referenceWorldY = world.y;
    calibration.referenceTileX = tileX;
    calibration.referenceTileY = tileY;
    float scale = calibration.ready ? calibration.scale : 1.0f;
    calibration.offsetX = world.x - tileX * scale;
    calibration.offsetY = world.y - tileY * scale;
    return true;
}

void clearSpeed(void* physics) {
    void* speed = readPtr(physics, fieldSpeed);
    if (!speed)
        return;
    writeFloat(speed, fieldSpeedX, 0.0f);
    writeFloat(speed, fieldSpeedY, 0.0f);
    // The extra speed is also used by Pos__Check on the next frame, so any step
    // remaining before the teleport must be cleared as well.
    void* extra = readPtr(speed, fieldSpeedAdd);
    if (!extra)
        return;
    writeFloat(extra, fieldSpeedAddX, 0.0f);
    writeFloat(extra, fieldSpeedAddY, 0.0f);
}

// The game uses this flag to rebuild the tiles around the body, so the target
// tile is used on the next frame.
void refreshTile(void* physics) {
    if (fieldFresh.field)
        writeBool(physics, fieldFresh, true);
}

void verifyTeleport(void* position) {
    if (verify.remaining <= 0)
        return;
    verify.remaining--;
    if (verify.remaining > 0)
        return;

    Vector2 world = {};
    Vector2 tile = {};
    bool hasWorld = readVector(position, fieldWorld, world);
    bool hasTile = readTile(position, tile);
    if (!hasWorld) {
        LOGE("Teleport: position after teleport unreadable");
        return;
    }
    if (hasTile) {
        LOGI("Teleport: after tile=(%.2f, %.2f) world=(%.1f, %.1f) delta=(%.2f, %.2f)/"
             "(%.1f, %.1f)",
             tile.x, tile.y, world.x, world.y, tile.x - verify.tileX, tile.y - verify.tileY,
             world.x - verify.world.x, world.y - verify.world.y);
    } else {
        LOGI("Teleport: after world=(%.1f, %.1f) delta=(%.1f, %.1f)", world.x, world.y,
             world.x - verify.world.x, world.y - verify.world.y);
    }
}

void applyTeleport(void* manager) {
    TeleportRequest& request = teleportRequest();
    if (!request.waiting.exchange(false))
        return;

    void* physics = readPtr(manager, fieldPhysics);
    if (!physics) {
        LOGE("Teleport: player physics not found");
        return;
    }
    void* position = readPtr(physics, fieldPosition);
    if (!position) {
        LOGE("Teleport: physics position unreadable");
        return;
    }

    int32_t tileX = request.x.load();
    int32_t uiY = request.y.load();
    if (tileX < -kLimitTeleport || tileX > kLimitTeleport || uiY < -kLimitTeleport ||
        uiY > kLimitTeleport) {
        LOGE("Teleport rejected: X %d, Y UI %d outside limit -%d..%d", tileX, uiY, kLimitTeleport,
             kLimitTeleport);
        return;
    }
    // The menu uses the game UI Y: 0 is the world center, the same as
    // Tile__Manager.Display__Y (tile Y - h/2). Physics uses tile Y, so
    // UI Y is shifted by half the world height.
    int32_t tileY = uiY + heightWorldTile(manager) / 2;

    Vector2 oldWorld = {};
    Vector2 tileOld = {};
    bool hasWorld = readVector(position, fieldWorld, oldWorld);
    bool hasTile = readTile(position, tileOld);
    if (!hasWorld) {
        LOGE("Teleport rejected: world position unreadable");
        return;
    }
    tryScaleInitial(physics);
    if (hasTile) {
        LOGI("Teleport: before tile=(%.2f, %.2f) world=(%.1f, %.1f)", tileOld.x, tileOld.y,
             oldWorld.x, oldWorld.y);
    } else {
        LOGI("Teleport: before world=(%.1f, %.1f), tile unreadable", oldWorld.x, oldWorld.y);
    }

    Vector2 newWorld =
        toWorld(static_cast<float>(tileX), static_cast<float>(tileY), oldWorld, tileOld, hasTile);
    if (!std::isfinite(newWorld.x) || !std::isfinite(newWorld.y)) {
        LOGE("Teleport rejected: converted result is not a number");
        return;
    }
    if (!calibration.ready) {
        LOGI("Teleport: scale not calibrated, tile units used as-is");
    }

    LOGI("Teleport: target UI=(%d, %d) tile=(%d, %d) world=(%.1f, %.1f) scale=%.2f "
         "offset=(%.2f, %.2f)",
         tileX, uiY, tileX, tileY, newWorld.x, newWorld.y,
         calibration.ready ? calibration.scale : 1.0f, calibration.offsetX, calibration.offsetY);
    if (!writePositionPhysics(position, static_cast<float>(tileX), static_cast<float>(tileY),
                              newWorld)) {
        LOGE("Teleport: physics position cannot be written");
        return;
    }
    clearSpeed(physics);
    refreshTile(physics);

    verify.remaining = 3;
    verify.tileX = static_cast<float>(tileX);
    verify.tileY = static_cast<float>(tileY);
    verify.world = newWorld;
}

float getMultiple(int32_t value, float minimum, float maximum) {
    float result = static_cast<float>(value) / 10.0f;
    if (!std::isfinite(result))
        return 1.0f;
    if (result < minimum)
        return minimum;
    if (result > maximum)
        return maximum;
    return result;
}

// The game Speed_Limit clamps speed.add.x and speed.add.y to ±_speed.limit
// before Pos__Check. The mod scaling result uses the same limit so a step never
// crosses more than one tile; if it does, the border and the tiles in between
// are not updated in time and rendering breaks too.
float stepLimit(void* managerPhysics) {
    return readFloat(readPtr(managerPhysics, fieldLimitSpeed), fieldLimitSpeedValue);
}

float clampStep(float value, float limit) {
    if (!std::isfinite(limit) || limit <= 0.0f)
        return value;
    if (value > limit)
        return limit;
    if (value < -limit)
        return -limit;
    return value;
}

// Physics__Manager.a points to Manager, and Manager.player is the local player.
// Pos__Check is called for every physics body, so the hook only touches the body
// that matches the local player's Player__Manager.phy.
bool physicsOwnedPlayer(void* managerPhysics, void* physics) {
    void* global = readPtr(managerPhysics, fieldActorPhysics);
    void* player = readPtr(global, localPlayerField);
    return player && readPtr(player, fieldPhysics) == physics;
}

// Pos__Check only computes _world.check = pos.world + speed.add. Every player
// movement, including flying, swimming, and sliding, goes through this point,
// so the X speed and the downward Y component are scaled here and then restored.
// The upward component is left alone so jumps stay the same.
void hookPosCheck(void* own, void** physics, void* method) {
    auto original = reinterpret_cast<FnPosCheck>(target(T_POS_CHECK).orig);
    if (!original)
        return;

    float multiple = getMultiple(features().speedPlayer.load(), 0.5f, 5.0f);
    float ratio = getMultiple(features().gravityPlayer.load(), 0.0f, 3.0f);
    void* object = physics ? *physics : nullptr;
    if ((multiple == 1.0f && ratio == 1.0f) || !object || !physicsOwnedPlayer(own, object)) {
        original(own, physics, method);
        return;
    }

    void* speed = readPtr(object, fieldSpeed);
    void* extra = speed ? readPtr(speed, fieldSpeedAdd) : nullptr;
    if (!extra) {
        original(own, physics, method);
        return;
    }
    float originalX = readFloat(extra, fieldSpeedAddX);
    float originalY = readFloat(extra, fieldSpeedAddY);
    if (!std::isfinite(originalX) || !std::isfinite(originalY)) {
        original(own, physics, method);
        return;
    }

    float limit = stepLimit(own);
    if (multiple != 1.0f)
        writeFloat(extra, fieldSpeedAddX, clampStep(originalX * multiple, limit));
    // Only downward motion follows the gravity slider. The upward push from a
    // jump is left alone, so 0.0x makes the player float.
    if (ratio != 1.0f && originalY < 0.0f)
        writeFloat(extra, fieldSpeedAddY, clampStep(originalY * ratio, limit));

    original(own, physics, method);

    writeFloat(extra, fieldSpeedAddX, originalX);
    writeFloat(extra, fieldSpeedAddY, originalY);
}

// The tile movement box is used by the player when stepping, so the left, right,
// and top sides are opened as wide as possible. The bottom side is left alone so
// the floor is still stepped on. Only the entry owned by `clock` is changed:
// every _Set call recomputes that one entry, while the other entries are used by
// other physics phases and must keep their original movement box. Changing every
// entry keeps wall clipping active after the switch is turned off, because
// entries that are never recomputed keep the free values.
void freeWall(void* physics, int32_t clock) {
    void* margin = physics ? readPtr(physics, fieldMargin) : nullptr;
    if (!margin)
        return;
    int32_t count = lengthArray(margin);
    if (clock < 0 || clock >= count)
        return;
    char* data = arrayData(margin);
    void* entry = *reinterpret_cast<void**>(data + static_cast<size_t>(clock) * sizeof(void*));
    if (!entry)
        return;
    writeFloat(entry, fieldMarginTop, kLimitFree);
    writeFloat(entry, fieldMarginLeft, -kLimitFree);
    writeFloat(entry, fieldMarginRight, kLimitFree);
}

// Physics detection runs for every body in the world, so only the body that
// matches the local player physics is freed.
bool physicsPlayerOwn(void* detect, void** physics) {
    void* global = readPtr(detect, fieldDetectActor);
    void* manager = readPtr(global, localPlayerField);
    void* object = physics ? *physics : nullptr;
    return manager && object && readPtr(manager, fieldPhysics) == object;
}

// Installed right after the movement box is computed from the tiles and before
// the player uses it to step, so the wall disappears on the same frame.
void hookDetectPhysics(void* own, void** physics, int32_t clock, float posX, float posY,
                       void* method) {
    auto original = reinterpret_cast<FnDetect>(target(T_DETECT_SET).orig);
    if (!original)
        return;
    original(own, physics, clock, posX, posY, method);
    if (!features().noClip || !physicsPlayerOwn(own, physics))
        return;
    freeWall(physics ? *physics : nullptr, clock);
}

void checkTileType() {
    if (!fieldTile.field)
        return;
    void* klass = fieldClass(fieldTile);
    const char* name = klass ? apiIl2cpp().className(klass) : nullptr;
    tileRound = name && strstr(name, "Int") != nullptr;
    LOGI("Teleport: tile field %s (%s)", name ? name : "unnamed",
         tileRound ? "integer" : "fractional");
}

bool findFieldPhysics(void* managerClass) {
    if (!findField(managerClass, "phy", fieldPhysics))
        return false;
    void* klassPhysics = fieldClass(fieldPhysics);
    if (!findField(klassPhysics, "pos", fieldPosition) ||
        !findField(klassPhysics, "speed", fieldSpeed) ||
        !findField(klassPhysics, "refresh", fieldFresh) ||
        !findField(klassPhysics, "margin", fieldMargin))
        return false;

    void* klassPosition = fieldClass(fieldPosition);
    if (!findField(klassPosition, "world", fieldWorld) ||
        !findField(klassPosition, "_world", fieldTrail))
        return false;
    // Tile is the position physics uses to look for collisions. Without this
    // field teleport writes world only again and can be pulled back to the old tile.
    if (!findField(klassPosition, "tile", fieldTile))
        LOGE("Tile field not found, teleport uses world only");
    checkTileType();

    void* klassTrail = fieldClass(fieldTrail);
    if (!findField(klassTrail, "prev", fieldTrailPrev) ||
        !findField(klassTrail, "check", fieldTrailCheck) ||
        !findField(klassTrail, "new_", fieldTrailNew) ||
        !findField(klassTrail, "end", fieldTrailEnd))
        return false;

    void* klassSpeed = fieldClass(fieldSpeed);
    if (!findField(klassSpeed, "x", fieldSpeedX) || !findField(klassSpeed, "y", fieldSpeedY))
        return false;

    // add is used by Pos__Check to compute the next step. If this field is
    // missing, the speed hook is skipped but teleport and No Clip still run.
    if (!findField(klassSpeed, "add", fieldSpeedAdd) ||
        !findField(fieldClass(fieldSpeedAdd), "x", fieldSpeedAddX) ||
        !findField(fieldClass(fieldSpeedAdd), "y", fieldSpeedAddY)) {
        fieldSpeedAdd = {};
        fieldSpeedAddX = {};
        fieldSpeedAddY = {};
        LOGE("Field speed.add not found; player Speed and Gravity disabled");
    } else {
        LOGI("Extra speed field ready");
    }

    // The collider is only used to guess the initial teleport scale. If any of
    // the fields is missing, teleport still uses the scale from player motion.
    if (findField(klassPhysics, "collider", fieldCollider) &&
        findField(fieldClass(fieldCollider), "size", fieldSizeCollider) &&
        findField(fieldClass(fieldCollider), "tile_Size", fieldSizeTile) &&
        findField(fieldClass(fieldSizeTile), "w", fieldSizeTileW) &&
        findField(fieldClass(fieldSizeTile), "h", fieldSizeTileH)) {
        LOGI("Collider field ready");
    } else {
        fieldSizeCollider = {};
        fieldSizeTile = {};
        fieldSizeTileW = {};
        fieldSizeTileH = {};
        LOGE("Collider field incomplete; teleport initial scale skipped");
    }
    return true;
}

bool findFieldMargin() {
    void* klassMargin = fieldElementClass(fieldMargin);
    return findField(klassMargin, "top", fieldMarginTop) &&
           findField(klassMargin, "left", fieldMarginLeft) &&
           findField(klassMargin, "right", fieldMarginRight);
}

// Manager.tile._world_Size.h is the world height in tiles, the same source as
// Tile__Manager.Display__Y while the zone list is not ready. This field is used
// to translate the menu Y to the game UI Y; if it is missing, tile Y is used
// as-is and only a warning is logged.
bool findFieldWorld() {
    if (!findField(fieldClass(fieldActorPlayer), "tile", fieldActorTile) ||
        !findField(fieldClass(fieldActorTile), "_world_Size", worldSizeField) ||
        !findField(fieldClass(worldSizeField), "h", fieldHeightWorld)) {
        fieldActorTile = {};
        worldSizeField = {};
        fieldHeightWorld = {};
        LOGE("World height field incomplete; Y Point uses tile as-is");
        return false;
    }
    LOGI("World height field ready");
    return true;
}

} // namespace

// The local player comparison fields are resolved before the feature hooks are
// installed, so hooks that run for every player can already tell guest bodies
// apart from the first call. Also called by installPlayerHook.
void findLocalPlayerFields() {
    void* managerClass = classOf("Player__Manager");
    findField(managerClass, "a", fieldActorPlayer);
    findField(fieldClass(fieldActorPlayer), "player", localPlayerField);
}

// Comparison used by hooks that run on state owned by every player. While the
// comparison fields are not ready, the feature keeps running as before so it
// does not die silently when a field name changes.
bool isLocalPlayerHook(void* manager) {
    if (!manager)
        return false;
    if (!fieldActorPlayer.field || !localPlayerField.field)
        return true;
    return localPlayer(manager);
}

// Called at the end of the local player update, on the game thread, because
// game position and physics must not be touched from the menu thread.
void onPlayerUpdate(void* manager) {
    if (isLocalPlayerHook(manager))
        applyWorldOverride(manager);
    if (!fieldActorPlayer.field || !localPlayerField.field || !fieldPhysics.field)
        return;
    if (!localPlayer(manager))
        return;

    void* physics = readPtr(manager, fieldPhysics);
    void* position = physics ? readPtr(physics, fieldPosition) : nullptr;
    int32_t height = heightWorldTile(manager);
    if (height != logHeightWorld) {
        logHeightWorld = height;
        if (height > 0)
            LOGI("UI Y point: world height %d tiles, UI Y 0 at tile %d", height, height / 2);
        else
            LOGI("UI Y point: world height unreadable, tile Y used as-is");
    }
    if (position) {
        Vector2 world = {};
        Vector2 tile = {};
        bool hasWorld = readVector(position, fieldWorld, world);
        bool hasTile = readTile(position, tile);
        if (hasWorld && player::servePosition()) {
            Vector2 tileUi = tile;
            tileUi.y -= static_cast<float>(height) * 0.5f;
            storePosition(hasTile, tileUi, world);
        }
        if (hasWorld && hasTile) {
            tryScaleInitial(physics);
            updateCalibration(world, tile);
        }
        verifyTeleport(position);
    }

    applyTeleport(manager);

    int speedValue = features().speedPlayer.load();
    if (speedValue != logSpeed) {
        logSpeed = speedValue;
        LOGI("Player speed: %.1fx", getMultiple(speedValue, 0.5f, 5.0f));
    }
    int gravityValue = features().gravityPlayer.load();
    if (gravityValue != logGravity) {
        logGravity = gravityValue;
        LOGI("Player gravity: %.1fx", getMultiple(gravityValue, 0.0f, 3.0f));
    }
}

void installHookMotion() {
    // Pos__Check runs for every physics body, so Physics__Manager.a is needed to
    // recognize the local player. Speed and gravity use the same point, and the
    // step limit is read from the game's Physics__Manager._speed.limit.
    void* klassPhysicsManager = findClass("Physics__Manager");
    if (!findField(klassPhysicsManager, "a", fieldActorPhysics))
        LOGE("Physics manager field not found; player Speed and Gravity disabled");
    else if (!fieldSpeedAdd.field || !fieldSpeedAddX.field || !fieldSpeedAddY.field)
        LOGE("Field speed.add incomplete; player Speed and Gravity disabled");
    else {
        if (findField(klassPhysicsManager, "_speed", fieldLimitSpeed) &&
            findField(fieldClass(fieldLimitSpeed), "limit", fieldLimitSpeedValue))
            LOGI("Speed limit field ready");
        else {
            fieldLimitSpeed = {};
            fieldLimitSpeedValue = {};
            LOGE("Speed limit field not found; step follows the game limit only");
        }
        installHook(target(T_POS_CHECK), reinterpret_cast<void*>(hookPosCheck));
    }
}

void installPlayerHook() {
    findLocalPlayerFields();
    void* managerClass = classOf("Player__Manager");
    if (!fieldActorPlayer.field || !localPlayerField.field) {
        LOGE("Player field incomplete, Teleport, Speed, Gravity, and No Clip disabled");
        return;
    }
    if (!findFieldPhysics(managerClass)) {
        LOGE("Player physics field incomplete, Teleport, Speed, and No Clip disabled");
        return;
    }
    LOGI("Player field OK");

    findFieldWorld();

    klassTile = findKlassField("_Tile", "pixel_Size");
    if (klassTile && findField(klassTile, "pixel_Size", fieldSizePixel))
        LOGI("Tile size field ready");
    else
        LOGE("Field pixel_Size not found; teleport initial scale uses the collider");

    installHookMotion();

    void* klassDetect = findClass("Physics__Detect");
    bool readyWall = findField(klassDetect, "a", fieldDetectActor) && findFieldMargin();
    if (!readyWall || !target(T_DETECT_SET).addr) {
        LOGE("Wall clip field incomplete; No Clip disabled");
        return;
    }
    LOGI("Wall clip field ready");
    installHook(target(T_DETECT_SET), reinterpret_cast<void*>(hookDetectPhysics));
}
