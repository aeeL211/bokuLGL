#include "feature_registry.h"

#include "feature_flags.h"

void applyFeatureChange(jint number, jboolean active, jint value) {
    Features& switchFlag = features();
    switch (number) {
    case feature_number::kIap:
        switchFlag.iap = active;
        break;
    case feature_number::kSlot:
        switchFlag.slot = active;
        break;
    case feature_number::kInputText:
        switchFlag.inputText = active;
        break;
    case feature_number::kRudeWord:
        switchFlag.rudeWord = active;
        break;
    case feature_number::kSaveWorldAndRoom:
        switchFlag.saveWorld = active;
        switchFlag.roomSize = active;
        break;
    case feature_number::kPasswordRoom:
        switchFlag.roomPassword = active;
        break;
    case feature_number::kPrivacy:
        switchFlag.privacy = active;
        break;
    case feature_number::kChatPrivate:
        switchFlag.privateChat = active;
        break;
    case feature_number::kShootAutomatic:
        switchFlag.shootAutomatic = active;
        break;
    case feature_number::kBusterFull:
        switchFlag.busterFull = active;
        break;
    case feature_number::kFeces:
        switchFlag.feces = active;
        break;
    case feature_number::kFood:
        switchFlag.food = active;
        break;
    case feature_number::kShake:
        switchFlag.shake = active;
        break;
    case feature_number::kKnockback:
        switchFlag.knockback = active;
        break;
    case feature_number::kNoClip:
        switchFlag.noClip = active;
        break;
    case feature_number::kTeleportX:
        teleportRequest().x.store(value);
        break;
    case feature_number::kTeleportY:
        teleportRequest().y.store(value);
        break;
    case feature_number::kTeleportRun:
        teleportRequest().waiting.store(true);
        break;
    case feature_number::kSpeed:
        switchFlag.speedPlayer.store(value);
        break;
    case feature_number::kGravity:
        switchFlag.gravityPlayer.store(value);
        break;
    case feature_number::kResetSpeed:
        switchFlag.speedPlayer.store(feature_number::kValueSpeedInitial);
        break;
    case feature_number::kResetGravity:
        switchFlag.gravityPlayer.store(feature_number::kValueGravityInitial);
        break;
    case feature_number::kRangeSkin:
        switchFlag.skinRange.store(value);
        break;
    case feature_number::kDebris:
        switchFlag.debris = active;
        break;
    case feature_number::kPermissionRoomOther:
        switchFlag.permissionRoomOther = active;
        break;
    case feature_number::kAutoDump:
        switchFlag.autoDump = active;
        break;
    case feature_number::kWorldLocal:
        switchFlag.worldLocal = active;
        break;
    case feature_number::kWorldBackground:
        switchFlag.worldBackground.store(value);
        break;
    case feature_number::kWorldPeriod:
        switchFlag.worldPeriod.store(value);
        break;
    case feature_number::kWorldWeather:
        switchFlag.worldWeather.store(value);
        break;
    default:
        break;
    }
}
