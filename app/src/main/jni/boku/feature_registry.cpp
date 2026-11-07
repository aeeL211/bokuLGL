#include "feature_registry.h"

#include "features.h"

void applyFeatureChange(jint number, jboolean active, jint value) {
    Features& sakelar = features();
    switch (number) {
    case feature_number::kIap:
        sakelar.iap = active;
        break;
    case feature_number::kSlot:
        sakelar.slot = active;
        break;
    case feature_number::kInputText:
        sakelar.inputText = active;
        break;
    case feature_number::kWordRude:
        sakelar.wordRude = active;
        break;
    case feature_number::kSaveWorldAndRoom:
        sakelar.saveWorld = active;
        sakelar.roomSize = active;
        break;
    case feature_number::kPasswordRoom:
        sakelar.roomPassword = active;
        break;
    case feature_number::kPrivacy:
        sakelar.privacy = active;
        break;
    case feature_number::kChatPrivate:
        sakelar.privateChat = active;
        break;
    case feature_number::kShootAutomatic:
        sakelar.shootAutomatic = active;
        break;
    case feature_number::kBusterFull:
        sakelar.busterFull = active;
        break;
    case feature_number::kFeces:
        sakelar.feces = active;
        break;
    case feature_number::kFood:
        sakelar.food = active;
        break;
    case feature_number::kShake:
        sakelar.shake = active;
        break;
    case feature_number::kKnockback:
        sakelar.knockback = active;
        break;
    case feature_number::kNoClip:
        sakelar.noClip = active;
        break;
    case feature_number::kTeleportX:
        teleportRequest().x.store(value);
        break;
    case feature_number::kTeleportY:
        teleportRequest().y.store(value);
        break;
    case feature_number::kTeleportJalankan:
        teleportRequest().menunggu.store(true);
        break;
    case feature_number::kSpeed:
        sakelar.speedPlayer.store(value);
        break;
    case feature_number::kGravity:
        sakelar.gravityPlayer.store(value);
        break;
    case feature_number::kResetSpeed:
        sakelar.speedPlayer.store(feature_number::kValueSpeedInitial);
        break;
    case feature_number::kResetGravity:
        sakelar.gravityPlayer.store(feature_number::kValueGravityInitial);
        break;
    case feature_number::kRangeSkin:
        sakelar.skinRange.store(value);
        break;
    case feature_number::kPecahan:
        sakelar.pecahan = active;
        break;
    case feature_number::kPermissionRoomOther:
        sakelar.permissionRoomOther = active;
        break;
    default:
        break;
    }
}
