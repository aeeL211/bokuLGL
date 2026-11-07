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
    bool lepas_ = false;
};

// Referensi lokal JNI selalu dibebaskan, termasuk saat ada jalur keluar lebih
// awal atau pemanggilan Java melempar eksepsi.
template <typename T>
class LocalRef {
public:
    LocalRef() = default;

    LocalRef(JNIEnv* env, T ref) : env_(env), ref_(ref) {}

    ~LocalRef() {
        if (env_ && ref_)
            env_->DeleteLocalRef(ref_);
    }

    LocalRef(const LocalRef&) = delete;
    LocalRef& operator=(const LocalRef&) = delete;

    LocalRef(LocalRef&& lain) noexcept : env_(lain.env_), ref_(lain.ref_) {
        lain.env_ = nullptr;
        lain.ref_ = nullptr;
    }

    LocalRef& operator=(LocalRef&& lain) noexcept {
        if (this != &lain) {
            reset();
            env_ = lain.env_;
            ref_ = lain.ref_;
            lain.env_ = nullptr;
            lain.ref_ = nullptr;
        }
        return *this;
    }

    T get() const { return ref_; }
    explicit operator bool() const { return ref_ != nullptr; }

    // Melepas referensi tanpa menghapusnya, untuk nilai yang dikembalikan ke Java.
    T lepas() {
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
    StringUtf(JNIEnv* env, jstring teks) : env_(env), teks_(teks) {
        if (env_ && teks_)
            isi_ = env_->GetStringUTFChars(teks_, nullptr);
    }

    ~StringUtf() {
        if (env_ && teks_ && isi_)
            env_->ReleaseStringUTFChars(teks_, isi_);
    }

    StringUtf(const StringUtf&) = delete;
    StringUtf& operator=(const StringUtf&) = delete;

    const char* get() const { return isi_; }
    explicit operator bool() const { return isi_ != nullptr; }

private:
    JNIEnv* env_ = nullptr;
    jstring teks_ = nullptr;
    const char* isi_ = nullptr;
};

// Eksepsi Java yang tertinggal membuat setiap panggilan JNI berikutnya gagal.
// Semua titik panggil membersihkannya, dan kegagalan dicatat ke logcat dengan
// tag yang sama seperti log native.
void bersihkanEksepsi(JNIEnv* env, const char* konteks);
