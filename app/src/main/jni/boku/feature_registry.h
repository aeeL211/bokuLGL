#pragma once

#include <jni.h>

// Nomor fitur adalah kontrak dengan Java: nilainya tertulis di daftar fitur dan
// dikirim kembali oleh Preferences.Changes. Konstanta ini hanya menamai angka
// yang sudah ada; urutannya tidak boleh berubah.
namespace feature_number {
constexpr jint kIap = 0;
constexpr jint kSlot = 1;
constexpr jint kInputText = 2;
constexpr jint kWordRude = 4;
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
constexpr jint kTeleportJalankan = 22;
constexpr jint kSpeed = 23;
constexpr jint kGravity = 24;
constexpr jint kResetSpeed = 25;
constexpr jint kResetGravity = 26;
constexpr jint kRangeSkin = 27;
constexpr jint kPecahan = 28;
constexpr jint kPermissionRoomOther = 29;
constexpr jint kValueSpeedInitial = 10;
constexpr jint kValueGravityInitial = 10;
} // namespace fitur_nomor

// Menulis satu perubahan dari menu ke state atomik. Nomor yang tidak dikenal
// diabaikan, sama seperti switch bawaan.
void applyFeatureChange(jint number, jboolean active, jint value);
