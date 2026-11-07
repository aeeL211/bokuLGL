#pragma once

#include <jni.h>

// Feature numbers are a contract with Java: the values are written in the
// feature list and sent back by Preferences.Changes. These constants only name
// numbers that already exist; their order must not change.
namespace feature_number {
constexpr jint kIap = 0;
constexpr jint kSlot = 1;
constexpr jint kInputText = 2;
constexpr jint kRudeWord = 4;
constexpr jint kSaveWorldAndRoom = 5;
constexpr jint kPasswordRoom = 6;
constexpr jint kPrivacy = 7;
constexpr jint kChatPrivate = 9;
constexpr jint kShootAutomatic = 11;
constexpr jint kBusterFull = 12;
constexpr jint kFeces = 13;
constexpr jint kFood = 14;
constexpr jint kShake = 15;
constexpr jint kKnockback = 16;
constexpr jint kNoClip = 19;
constexpr jint kTeleportX = 20;
constexpr jint kTeleportY = 21;
constexpr jint kTeleportRun = 22;
constexpr jint kSpeed = 23;
constexpr jint kGravity = 24;
constexpr jint kResetSpeed = 25;
constexpr jint kResetGravity = 26;
constexpr jint kRangeSkin = 27;
constexpr jint kDebris = 28;
constexpr jint kPermissionRoomOther = 29;
constexpr jint kAutoDump = 30;
constexpr jint kWorldLocal = 31;
constexpr jint kWorldBackground = 32;
constexpr jint kWorldPeriod = 33;
constexpr jint kWorldWeather = 34;
constexpr jint kValueSpeedInitial = 10;
constexpr jint kValueGravityInitial = 10;
} // namespace feature_number

// Writes one change from the menu into the atomic state. Unknown numbers are
// ignored, the same as the stock switch.
void applyFeatureChange(jint number, jboolean active, jint value);
