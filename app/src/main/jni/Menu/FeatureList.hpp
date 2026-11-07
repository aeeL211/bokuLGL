#pragma once

#include <jni.h>

// The feature and setting list read by the menu. Its contents are a contract
// with Java: every row is read as a number, type, and label, so the text must
// not change during a refactor.
jobjectArray featureList(JNIEnv* env);
jobjectArray settingList(JNIEnv* env);
