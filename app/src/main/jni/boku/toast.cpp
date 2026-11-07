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

// A toast is one short line; longer names are cut on a character boundary and
// marked with an ellipsis so the message stays readable.
constexpr size_t kLimitName = 64;
constexpr char kEllipsis[] = "...";

// Toast.show() needs a Looper on the calling thread: the toast display is driven
// by messages posted to that Looper, so a thread without one cannot show it.
// Hook callbacks run on the game thread, which has the main Looper, so they show
// the toast directly; a mod worker thread does not, so it gets a short-lived
// thread that prepares its own Looper and keeps it running.
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
    LocalRef<jobject> toastObject(
        env, (toast && toast->create && toast->show && message && activity)
                 ? env->CallStaticObjectMethod(toast->klass, toast->create, activity.get(),
                                               message.get(), 0)
                 : nullptr);
    if (toastObject)
        env->CallVoidMethod(toastObject.get(), toast->show);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Toast failed: %s", text);
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

void* threadToast(void* argument) {
    char text[384];
    snprintf(text, sizeof(text), "%s", static_cast<const char*>(argument));
    free(argument);

    JNIEnv* env = nullptr;
    bool release = false;
    if (!attachJni(&env, &release))
        return nullptr;

    LocalRef<jclass> klassLooper(env, env->FindClass("android/os/Looper"));
    jmethodID prepare =
        klassLooper ? env->GetStaticMethodID(klassLooper.get(), "prepare", "()V") : nullptr;
    if (prepare)
        env->CallStaticVoidMethod(klassLooper.get(), prepare);
    if (env->ExceptionCheck())
        env->ExceptionClear();

    showOnScreen(env, text);

    // Keeps the toast's own messages flowing on older Android versions, where
    // the view is shown and hidden through handlers on its creating thread.
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

// The name file can come from any editor, so its contents are read as bytes
// first and never rejected over encoding. Valid UTF-8 is used as-is, a UTF-16
// byte order mark switches to UTF-16 decoding, and other bytes are read as
// Latin-1 so old files still show readable characters instead of an empty toast.
bool characterSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

// Returns the length of the UTF-8 sequence at s, or 0 when invalid.
size_t utf8Length(const char* s, size_t available) {
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
    if (len > available)
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

size_t encodeUtf8(uint32_t code, char* out) {
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
    size_t used;
    size_t countCharacter;
};

// Control characters carry no visible name, so they are skipped instead of
// failing the read. Returns false once the name is long enough to be cut.
bool addCharacter(BufferName& buf, uint32_t code) {
    if (code < 0x20 || code == 0x7F)
        return true;
    if (buf.countCharacter == kLimitName || buf.used + 4 + sizeof(kEllipsis) > buf.size)
        return false;
    buf.used += encodeUtf8(code, buf.out + buf.used);
    buf.countCharacter++;
    return true;
}

// Returns true when the name was cut before the last UTF-16 unit.
bool readUtf16(const unsigned char* s, size_t len, bool littleEndian, BufferName& buf) {
    for (size_t i = 0; i + 1 < len; i += 2) {
        uint32_t unit = littleEndian ? (s[i] | (s[i + 1] << 8)) : ((s[i] << 8) | s[i + 1]);
        uint32_t code = unit;
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            uint32_t low = 0;
            if (i + 3 < len) {
                const unsigned char* next = s + i + 2;
                low = littleEndian ? (next[0] | (next[1] << 8)) : ((next[0] << 8) | next[1]);
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

// Returns true when the name was cut before the last character. Invalid UTF-8
// is read byte by byte as Latin-1, which keeps Windows-1252 names readable.
bool readByte(const char* s, size_t len, BufferName& buf) {
    for (size_t i = 0; i < len;) {
        size_t order = utf8Length(s + i, len - i);
        uint32_t code = order ? readPointCode(s + i, order) : static_cast<unsigned char>(s[i]);
        i += order ? order : 1;
        if (!addCharacter(buf, code))
            return true;
    }
    return false;
}

// The name file belongs next to the installed game, but older builds kept it in
// the <package>.mods directory, which also becomes the game's own directory when
// a clone uses that name. Both are tried, so either file layout still works.
bool pathNameUser(char* out, size_t size, bool useDirMods) {
    char root[512];
    if (!rootStorageExternal(root, sizeof(root)))
        return false;
    char appPackage[192];
    if (!packageApplication(appPackage, sizeof(appPackage)))
        return false;
    snprintf(out, size, "%s/Android/data/%s%s/files/Setting/User_Name.txt", root, appPackage,
             useDirMods ? ".mods" : "");
    return true;
}

// Read when the toast is shown, not cached: the file is small, the toast appears
// once per session, and an edited name takes effect on the next launch.
bool readFileName(const char* path, char* out, size_t size) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        int error = errno;
        LOGE("User name file open failed: %s (%d %s)", path, error, strerror(error));
        return false;
    }
    char raw[512];
    ssize_t n = read(fd, raw, sizeof(raw) - 1);
    int error = errno;
    close(fd);
    if (n < 0) {
        LOGE("User name file read failed: %s (%d %s)", path, error, strerror(error));
        return false;
    }
    if (n == 0) {
        LOGE("User name file is empty: %s", path);
        return false;
    }

    size_t initial = 0;
    size_t end = static_cast<size_t>(n);
    while (initial < end && characterSpace(static_cast<unsigned char>(raw[initial])))
        initial++;
    while (end > initial && characterSpace(static_cast<unsigned char>(raw[end - 1])))
        end--;
    if (initial == end) {
        LOGE("User name file holds no name: %s", path);
        return false;
    }

    BufferName buf = {out, size, 0, 0};
    const auto* byte = reinterpret_cast<const unsigned char*>(raw + initial);
    size_t len = end - initial;
    bool truncated = false;
    if (len >= 2 && byte[0] == 0xFF && byte[1] == 0xFE)
        truncated = readUtf16(byte + 2, len - 2, true, buf);
    else if (len >= 2 && byte[0] == 0xFE && byte[1] == 0xFF)
        truncated = readUtf16(byte + 2, len - 2, false, buf);
    else
        truncated = readByte(raw + initial, len, buf);

    if (truncated) {
        size_t countPoint = sizeof(kEllipsis) - 1;
        memcpy(buf.out + buf.used, kEllipsis, countPoint);
        buf.used += countPoint;
    }
    buf.out[buf.used] = '\0';
    LOGI("User name loaded: %s", buf.out);
    return true;
}

bool readName(char* out, size_t size) {
    char path[900];
    if (!pathNameUser(path, sizeof(path), false)) {
        LOGE("User name file location unavailable");
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
    EnvScope scope;
    if (!scope)
        return;
    JNIEnv* env = scope.env();

    if (hasLooper(env)) {
        showOnScreen(env, text);
    } else {
        char* copy = strdup(text);
        if (copy) {
            pthread_t t;
            if (pthread_create(&t, nullptr, threadToast, copy) == 0) {
                pthread_detach(t);
            } else {
                free(copy);
            }
        }
    }
}

// The activation toast names the account from the mod manager file; a broken or
// missing file falls back to the plain enter message instead of failing.
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
