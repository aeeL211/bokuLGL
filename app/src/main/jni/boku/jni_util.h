#pragma once

#include <jni.h>

// Pemegang JNIEnv untuk satu blok kode. Thread yang belum terpasang ke VM akan
// dipasang saat konstruksi dan dilepas lagi saat destruksi, jadi setiap jalur
// keluar tetap berpasangan tanpa penanganan manual.
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

// Referensi lokal JNI selalu dibebaskan, termasuk saat ada jalur keluar lebih
// awal atau pemanggilan Java melempar eksepsi.
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

    // Melepas referensi tanpa menghapusnya, untuk nilai yang dikembalikan ke Java.
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

// Pinjaman karakter UTF-8 dari jstring. GetStringUTFChars dan
// ReleaseStringUTFChars selalu berpasangan.
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

// Eksepsi Java yang tertinggal membuat setiap panggilan JNI berikutnya gagal.
// Semua titik panggil membersihkannya, dan kegagalan dicatat ke logcat dengan
// tag yang sama seperti log native.
void clearException(JNIEnv* env, const char* context);
