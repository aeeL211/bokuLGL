#include "toast.h"

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "jni_bridge.h"
#include "jni_cache.h"
#include "jni_util.h"
#include "log.h"

namespace {

// Toast hanya satu baris pendek; nama yang lebih panjang dipotong di batas
// karakter dan ditandai elipsis supaya pesannya tetap terbaca.
constexpr size_t kLimitName = 64;
constexpr char kElipsis[] = "...";

// Toast.show() butuh Looper di thread pemanggil: tampilan toast digerakkan oleh
// pesan yang dikirim ke Looper itu, jadi thread tanpa Looper tidak bisa
// menampilkannya. Callback hook berjalan di thread game, yang punya Looper
// utama, jadi mereka menampilkan toast langsung; thread pekerja mod tidak punya,
// jadi ia mendapat thread berumur pendek yang menyiapkan Looper-nya sendiri dan
// menjaganya tetap berjalan.
void showOnScreen(JNIEnv* env, const char* text) {
    LocalRef<jobject> activity;
    if (klassUnityPlayer()) {
        jfieldID fieldActivity =
            env->GetStaticFieldID(klassUnityPlayer(), "currentActivity", "Landroid/app/Activity;");
        if (fieldActivity)
            activity = LocalRef<jobject>(
                env, env->GetStaticObjectField(klassUnityPlayer(), fieldActivity));
        else
            clearException(env, "UnityPlayer.currentActivity");
    }

    const ToastClass* toast = cacheToast(env);
    LocalRef<jstring> message(env, env->NewStringUTF(text));
    LocalRef<jobject> objekToast(
        env,
        (toast && toast->create && toast->show && message && activity)
            ? env->CallStaticObjectMethod(toast->klass, toast->create, activity.get(), message.get(), 0)
            : nullptr);
    if (objekToast)
        env->CallVoidMethod(objekToast.get(), toast->show);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Toast gagal: %s", text);
    }
}

bool hasLooper(JNIEnv* env) {
    LocalRef<jclass> klassLooper(env, env->FindClass("android/os/Looper"));
    jmethodID methodLooper =
        klassLooper ? env->GetStaticMethodID(klassLooper.get(), "myLooper", "()Landroid/os/Looper;")
                    : nullptr;
    LocalRef<jobject> looper(
        env, methodLooper ? env->CallStaticObjectMethod(klassLooper.get(), methodLooper) : nullptr);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    return static_cast<bool>(looper);
}

void* threadToast(void* argumen) {
    char text[384];
    snprintf(text, sizeof(text), "%s", static_cast<const char*>(argumen));
    free(argumen);

    JNIEnv* env = nullptr;
    bool release = false;
    if (!lekatJni(&env, &release))
        return nullptr;

    LocalRef<jclass> klassLooper(env, env->FindClass("android/os/Looper"));
    jmethodID prepare =
        klassLooper ? env->GetStaticMethodID(klassLooper.get(), "prepare", "()V") : nullptr;
    if (prepare)
        env->CallStaticVoidMethod(klassLooper.get(), prepare);
    if (env->ExceptionCheck())
        env->ExceptionClear();

    showOnScreen(env, text);

    // Menjaga pesan milik toast tetap mengalir di versi Android lama, tempat
    // tampilannya ditampilkan dan disembunyikan lewat penangan di thread
    // pembuatnya.
    jmethodID loop =
        klassLooper ? env->GetStaticMethodID(klassLooper.get(), "loop", "()V") : nullptr;
    if (!loop) {
        if (release)
            findVmJava()->DetachCurrentThread();
        return nullptr;
    }
    env->CallStaticVoidMethod(klassLooper.get(), loop);
    return nullptr;
}

// Berkas nama bisa berasal dari editor mana pun, jadi isinya dibaca sebagai byte
// dulu dan tidak pernah ditolak karena encoding-nya. UTF-8 yang valid dipakai apa
// adanya, byte order mark UTF-16 mengalihkan ke dekode UTF-16, dan byte lain
// dibaca sebagai Latin-1 supaya berkas lama tetap menampilkan karakter yang
// terbaca, bukan toast kosong.
bool characterSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

// Mengembalikan panjang urutan UTF-8 di s, atau 0 bila tidak valid.
size_t utf8Length(const char* s, size_t tersedia) {
    unsigned char initial = static_cast<unsigned char>(s[0]);
    if (initial < 0x80)
        return 1;
    size_t len = 0;
    uint32_t code = 0;
    if ((initial & 0xE0) == 0xC0) {
        len = 2;
        code = initial & 0x1F;
    } else if ((initial & 0xF0) == 0xE0) {
        len = 3;
        code = initial & 0x0F;
    } else if ((initial & 0xF8) == 0xF0) {
        len = 4;
        code = initial & 0x07;
    } else {
        return 0;
    }
    if (len > tersedia)
        return 0;
    for (size_t i = 1; i < len; i++) {
        unsigned char next = static_cast<unsigned char>(s[i]);
        if ((next & 0xC0) != 0x80)
            return 0;
        code = (code << 6) | (next & 0x3F);
    }
    if ((len == 2 && code < 0x80) || (len == 3 && code < 0x800) || (len == 4 && code < 0x10000)) {
        return 0;
    }
    if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
        return 0;
    return len;
}

uint32_t readPointCode(const char* s, size_t len) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    if (len == 1)
        return p[0];
    if (len == 2)
        return ((p[0] & 0x1Fu) << 6) | (p[1] & 0x3Fu);
    if (len == 3)
        return ((p[0] & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
    return ((p[0] & 0x07u) << 18) | ((p[1] & 0x3Fu) << 12) | ((p[2] & 0x3Fu) << 6) | (p[3] & 0x3Fu);
}

size_t kodekanUtf8(uint32_t code, char* out) {
    if (code < 0x80) {
        out[0] = static_cast<char>(code);
        return 1;
    }
    if (code < 0x800) {
        out[0] = static_cast<char>(0xC0 | (code >> 6));
        out[1] = static_cast<char>(0x80 | (code & 0x3F));
        return 2;
    }
    if (code < 0x10000) {
        out[0] = static_cast<char>(0xE0 | (code >> 12));
        out[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (code & 0x3F));
        return 3;
    }
    out[0] = static_cast<char>(0xF0 | (code >> 18));
    out[1] = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
    out[2] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out[3] = static_cast<char>(0x80 | (code & 0x3F));
    return 4;
}

struct BufferName {
    char* out;
    size_t size;
    size_t terpakai;
    size_t countCharacter;
};

// Karakter kontrol tidak membawa nama yang terlihat, jadi dilewati, bukan
// menggagalkan pembacaan. Mengembalikan false begitu namanya cukup panjang untuk
// dipotong.
bool addCharacter(BufferName& buf, uint32_t code) {
    if (code < 0x20 || code == 0x7F)
        return true;
    if (buf.countCharacter == kLimitName || buf.terpakai + 4 + sizeof(kElipsis) > buf.size)
        return false;
    buf.terpakai += kodekanUtf8(code, buf.out + buf.terpakai);
    buf.countCharacter++;
    return true;
}

// Mengembalikan true bila nama dipotong sebelum unit UTF-16 terakhir.
bool readUtf16(const unsigned char* s, size_t len, bool littleEndian, BufferName& buf) {
    for (size_t i = 0; i + 1 < len; i += 2) {
        uint32_t unit = littleEndian ? (s[i] | (s[i + 1] << 8)) : ((s[i] << 8) | s[i + 1]);
        uint32_t code = unit;
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            uint32_t low = 0;
            if (i + 3 < len) {
                const unsigned char* next = s + i + 2;
                low = littleEndian ? (next[0] | (next[1] << 8))
                                      : ((next[0] << 8) | next[1]);
            }
            if (low >= 0xDC00 && low <= 0xDFFF) {
                code = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            } else {
                code = 0xFFFD;
            }
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            code = 0xFFFD;
        }
        if (!addCharacter(buf, code))
            return true;
    }
    return false;
}

// Mengembalikan true bila nama dipotong sebelum karakter terakhir. UTF-8 yang
// tidak valid dibaca byte demi byte sebagai Latin-1, yang menjaga nama
// Windows-1252 tetap terbaca.
bool readByte(const char* s, size_t len, BufferName& buf) {
    for (size_t i = 0; i < len;) {
        size_t urutan = utf8Length(s + i, len - i);
        uint32_t code = urutan ? readPointCode(s + i, urutan) : static_cast<unsigned char>(s[i]);
        i += urutan ? urutan : 1;
        if (!addCharacter(buf, code))
            return true;
    }
    return false;
}

// Berkas nama seharusnya berada di samping game yang terpasang, tetapi build
// lebih lama menyimpannya di direktori <package>.mods, yang juga menjadi
// direktori game sendiri saat klon memakai nama itu. Keduanya dicoba, jadi kedua
// tata letak berkas tetap terbaca.
bool pathNameUser(char* out, size_t size, bool useDirMods) {
    char root[512];
    if (!rootStorageExternal(root, sizeof(root)))
        return false;
    char packet[192];
    if (!packetApplication(packet, sizeof(packet)))
        return false;
    snprintf(out, size, "%s/Android/data/%s%s/files/Setting/User_Name.txt", root, packet,
             useDirMods ? ".mods" : "");
    return true;
}

// Dibaca saat toast ditampilkan, bukan disimpan di cache: berkasnya kecil, toast
// hanya tampil sekali per sesi, dan nama yang diubah terpakai pada peluncuran
// berikutnya.
bool readFileName(const char* path, char* out, size_t size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        int galat = errno;
        LOGE("Berkas nama pengguna gagal dibuka: %s (%d %s)", path, galat, strerror(galat));
        return false;
    }
    char mentah[512];
    ssize_t n = read(fd, mentah, sizeof(mentah) - 1);
    int galat = errno;
    close(fd);
    if (n < 0) {
        LOGE("Berkas nama pengguna gagal dibaca: %s (%d %s)", path, galat, strerror(galat));
        return false;
    }
    if (n == 0) {
        LOGE("Berkas nama pengguna kosong: %s", path);
        return false;
    }

    size_t initial = 0;
    size_t end = static_cast<size_t>(n);
    while (initial < end && characterSpace(static_cast<unsigned char>(mentah[initial])))
        initial++;
    while (end > initial && characterSpace(static_cast<unsigned char>(mentah[end - 1])))
        end--;
    if (initial == end) {
        LOGE("Berkas nama pengguna tidak berisi nama: %s", path);
        return false;
    }

    BufferName buf = {out, size, 0, 0};
    const auto* byte = reinterpret_cast<const unsigned char*>(mentah + initial);
    size_t len = end - initial;
    bool dipotong = false;
    if (len >= 2 && byte[0] == 0xFF && byte[1] == 0xFE)
        dipotong = readUtf16(byte + 2, len - 2, true, buf);
    else if (len >= 2 && byte[0] == 0xFE && byte[1] == 0xFF)
        dipotong = readUtf16(byte + 2, len - 2, false, buf);
    else
        dipotong = readByte(mentah + initial, len, buf);

    if (dipotong) {
        size_t countPoint = sizeof(kElipsis) - 1;
        memcpy(buf.out + buf.terpakai, kElipsis, countPoint);
        buf.terpakai += countPoint;
    }
    buf.out[buf.terpakai] = '\0';
    LOGI("Nama pengguna dimuat: %s", buf.out);
    return true;
}

bool readName(char* out, size_t size) {
    char path[900];
    if (!pathNameUser(path, sizeof(path), false)) {
        LOGE("Lokasi berkas nama pengguna tidak tersedia");
        return false;
    }
    if (readFileName(path, out, size))
        return true;
    if (!pathNameUser(path, sizeof(path), true))
        return false;
    return readFileName(path, out, size);
}

} // namespace

void showToast(const char* text) {
    EnvScope lingkup;
    if (!lingkup)
        return;
    JNIEnv* env = lingkup.env();

    if (hasLooper(env)) {
        showOnScreen(env, text);
    } else {
        char* salinan = strdup(text);
        if (salinan) {
            pthread_t t;
            if (pthread_create(&t, nullptr, threadToast, salinan) == 0) {
                pthread_detach(t);
            } else {
                free(salinan);
            }
        }
    }
}

// Toast aktivasi menyebut akun dari berkas manajer mod; berkas yang rusak atau
// hilang jatuh ke pesan masuk biasa, bukan gagal.
void toastEnter() {
    char name[320];
    if (!readName(name, sizeof(name))) {
        showToast("MODS: Masuk");
        return;
    }
    char text[384];
    snprintf(text, sizeof(text), "MODS: Masuk sebagai %s", name);
    showToast(text);
}
