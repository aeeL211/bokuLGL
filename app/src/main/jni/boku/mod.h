#pragma once

#include <jni.h>

// Called once from the menu's JNI_OnLoad, because this library may export only
// one JNI_OnLoad. The library is loaded in the game process, so the Unity
// activity already exists when this call happens.
void setupMod(JavaVM* vm, JNIEnv* env);
