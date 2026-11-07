#pragma once

#include <jni.h>

// Holds the JNIEnv for one code block. A thread not yet attached to the VM is
// attached on construction and detached on destruction, so every exit path stays
// paired without manual handling.
class EnvScope {
public:
    EnvScope();
    ~EnvScope();

    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;

    JNIEnv* env() const { return env_; }
    explicit operator bool() const { return env_ != nullptr; }

private:
    JNIEnv* env_ = nullptr;
    bool release_ = false;
};

// JNI local references are always released, including on early exit paths or
// when a Java call throws an exception.
template <typename T> class LocalRef {
public:
    LocalRef() = default;

    LocalRef(JNIEnv* env, T ref) : env_(env), ref_(ref) {}

    ~LocalRef() {
        if (env_ && ref_)
            env_->DeleteLocalRef(ref_);
    }

    LocalRef(const LocalRef&) = delete;
    LocalRef& operator=(const LocalRef&) = delete;

    LocalRef(LocalRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
        other.env_ = nullptr;
        other.ref_ = nullptr;
    }

    LocalRef& operator=(LocalRef&& other) noexcept {
        if (this != &other) {
            reset();
            env_ = other.env_;
            ref_ = other.ref_;
            other.env_ = nullptr;
            other.ref_ = nullptr;
        }
        return *this;
    }

    T get() const { return ref_; }
    explicit operator bool() const { return ref_ != nullptr; }

    // Releases the reference without deleting it, for values returned to Java.
    T release() {
        T ref = ref_;
        ref_ = nullptr;
        return ref;
    }

    void reset() {
        if (env_ && ref_)
            env_->DeleteLocalRef(ref_);
        ref_ = nullptr;
    }

private:
    JNIEnv* env_ = nullptr;
    T ref_ = nullptr;
};

// Borrowed UTF-8 characters from a jstring. GetStringUTFChars and
// ReleaseStringUTFChars are always paired.
class StringUtf {
public:
    StringUtf(JNIEnv* env, jstring text) : env_(env), text_(text) {
        if (env_ && text_)
            content_ = env_->GetStringUTFChars(text_, nullptr);
    }

    ~StringUtf() {
        if (env_ && text_ && content_)
            env_->ReleaseStringUTFChars(text_, content_);
    }

    StringUtf(const StringUtf&) = delete;
    StringUtf& operator=(const StringUtf&) = delete;

    const char* get() const { return content_; }
    explicit operator bool() const { return content_ != nullptr; }

private:
    JNIEnv* env_ = nullptr;
    jstring text_ = nullptr;
    const char* content_ = nullptr;
};

// A leftover Java exception makes every following JNI call fail. All call sites
// clear it, and failures are logged to logcat with the same tag as native logs.
void clearException(JNIEnv* env, const char* context);
