#include "log.h"

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>

#include "hook_manager.h"
#include "jni_bridge.h"
#include "jni_util.h"

#ifdef BOKU_LOG

namespace {

// Each session writes its own file in the app's external files directory,
// namely Android/data/<package>/files/bokumods-1.log, bokumods-2.log and so
// on. The path comes from Context.getExternalFilesDir() via
// UnityPlayer.currentActivity, so the package name is never hardcoded and a
// renamed package still works.
std::atomic<int> fdLog{-1};

// The first two callers (the library constructor and JNI_OnLoad) may try to
// open the file concurrently. This lock ensures only one session file is
// created; the fdLog check inside it handles the caller that arrives
// later.
std::mutex keyOpen;

// Path of the file opened by this session, kept so a command can print it.
char pathSession[600] = {};

// The activity, and with it the log directory, only exists a few seconds after
// the process starts. Lines from that window are buffered here and flushed when
// the file finally opens, so the start of the session is not lost.
constexpr int kLimitPending = 32;
constexpr int kLengthPending = 220;
char pending[kLimitPending][kLengthPending] = {};
std::atomic<int> countPending{0};

// The worker thread, JNI_OnLoad and the constructor all write logs before the
// file exists, so the pending ring needs its own lock. The signal handler never
// takes it: it writes straight to fdLog, which is already open by the time a
// crash matters.
std::atomic_flag flagPending = ATOMIC_FLAG_INIT;

void keyPending() {
    while (flagPending.test_and_set(std::memory_order_acquire)) {
    }
}

void openKeyPending() { flagPending.clear(std::memory_order_release); }

void formatTime(char* out, size_t size, const char* pattern) {
    std::time_t now = std::time(nullptr);
    std::tm tm = {};
    localtime_r(&now, &tm);
    std::strftime(out, size, pattern, &tm);
}

// localtime_r and strftime compute the time zone on every line, yet several
// lines usually share the same second. A thread-local cache makes that
// computation happen once per second, without cross-thread locking.
const char* timeNow() {
    thread_local std::time_t second = 0;
    thread_local char text[16] = {};
    std::time_t now = std::time(nullptr);
    if (now != second || !text[0]) {
        std::tm tm = {};
        localtime_r(&now, &tm);
        std::strftime(text, sizeof(text), "%H:%M:%S", &tm);
        second = now;
    }
    return text;
}

// INFO is padded to five characters so the message column lines up with
// ERROR.
const char* nameLevel(char level) { return level == 'E' ? "ERROR" : "INFO "; }

const char* signalName(int sig) {
    switch (sig) {
    case SIGSEGV:
        return "SIGSEGV";
    case SIGBUS:
        return "SIGBUS";
    case SIGILL:
        return "SIGILL";
    case SIGABRT:
        return "SIGABRT";
    default:
        return "UNKNOWN";
    }
}

void writeRow(const char* row, int n) {
    // Re-check under the lock: the file opener can install fdLog between the
    // earlier check and this store, and a line left behind after the last
    // flush would never reach the file.
    keyPending();
    int fd = fdLog;
    if (fd >= 0) {
        openKeyPending();
        write(fd, row, static_cast<size_t>(n));
        return;
    }
    // A full buffer drops the oldest line instead of blocking the caller.
    int slot = countPending.fetch_add(1) % kLimitPending;
    if (slot < 0)
        slot = 0;
    snprintf(pending[slot], kLengthPending, "%.*s", n, row);
    openKeyPending();
}

void flush() {
    keyPending();
    int count = countPending.exchange(0);
    if (count > kLimitPending)
        count = kLimitPending;
    for (int i = 0; i < count; i++) {
        size_t len = strlen(pending[i]);
        if (len)
            write(fdLog, pending[i], len);
    }
    openKeyPending();
}

// getExternalFilesDir is the public API for the per-app external directory;
// calling it avoids guessing the package name or the storage mount point.
bool directoryFile(char* out, size_t size) {
    EnvScope scope;
    if (!scope)
        return false;
    JNIEnv* env = scope.env();

    bool success = false;
    jobject activity = nullptr;
    jobject folder = nullptr;
    jstring path = nullptr;
    const char* character = nullptr;

    if (klassUnityPlayer()) {
        jfieldID fieldActivity =
            env->GetStaticFieldID(klassUnityPlayer(), "currentActivity", "Landroid/app/Activity;");
        if (fieldActivity)
            activity = env->GetStaticObjectField(klassUnityPlayer(), fieldActivity);
    }
    if (activity) {
        jclass klassActivity = env->GetObjectClass(activity);
        jmethodID getDir = klassActivity ? env->GetMethodID(klassActivity, "getExternalFilesDir",
                                                            "(Ljava/lang/String;)Ljava/io/File;")
                                         : nullptr;
        if (getDir)
            folder = env->CallObjectMethod(activity, getDir, nullptr);
        if (klassActivity)
            env->DeleteLocalRef(klassActivity);
    }
    if (folder) {
        jclass klassFile = env->FindClass("java/io/File");
        if (klassFile) {
            jmethodID getPath =
                env->GetMethodID(klassFile, "getAbsolutePath", "()Ljava/lang/String;");
            if (getPath)
                path = static_cast<jstring>(env->CallObjectMethod(folder, getPath));
            env->DeleteLocalRef(klassFile);
        }
    }
    if (path) {
        character = env->GetStringUTFChars(path, nullptr);
        if (character) {
            snprintf(out, size, "%s", character);
            success = true;
        }
    }

    if (character)
        env->ReleaseStringUTFChars(path, character);
    if (path)
        env->DeleteLocalRef(path);
    if (folder)
        env->DeleteLocalRef(folder);
    if (activity)
        env->DeleteLocalRef(activity);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    return success;
}

// Fallback for the short window before UnityPlayer.currentActivity is set:
// build the same directory from the process package and the storage root. The
// package directory has existed since install, so a missing or unwritable
// directory means the guess is wrong (renamed package, clone, another mount) and
// creating it would hide the log outside the app sandbox.
bool directoryOld(char* out, size_t size) {
    char appPackage[192];
    if (!packageApplication(appPackage, sizeof(appPackage)))
        return false;

    char root[512];
    if (!rootStorageExternal(root, sizeof(root)))
        return false;
    char directory[512];
    snprintf(directory, sizeof(directory), "%s/Android/data/%s/files", root, appPackage);
    if (access(directory, W_OK | X_OK) != 0)
        return false;
    snprintf(out, size, "%s", directory);
    return true;
}

// A session file carries its session number, so the name stays short and the
// highest number in the folder is always the newest.
int numberSession(const char* name) {
    int value = 0;
    int used = 0;
    if (sscanf(name, "bokumods-%d.log%n", &value, &used) != 1)
        return 0;
    if (used <= 0 || name[used] != '\0' || value < 1)
        return 0;
    return value;
}

int sessionLast(const char* pathDirectory) {
    DIR* stream = opendir(pathDirectory);
    if (!stream)
        return 0;
    int last = 0;
    while (struct dirent* entry = readdir(stream)) {
        int value = numberSession(entry->d_name);
        if (value > last)
            last = value;
    }
    closedir(stream);
    return last;
}

// One file per session, numbered in the order sessions start. O_EXCL never
// touches an existing file, and the number only grows, so a deleted file cannot
// pass its number on to the next session.
int openNew(const char* pathDirectory, char* path, size_t size, int* error) {
    int first = sessionLast(pathDirectory) + 1;
    for (int i = first; i < first + 100; i++) {
        snprintf(path, size, "%s/bokumods-%d.log", pathDirectory, i);
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (fd >= 0)
            return fd;
        *error = errno;
        if (*error != EEXIST)
            return -1;
    }
    return -1;
}

bool ensureOpen() {
    if (fdLog >= 0)
        return true;
    // The directory is unreachable before the activity exists, so retry at most
    // every two seconds, not on every line.
    static std::atomic<long> lastAttempt{0};
    long now = static_cast<long>(std::time(nullptr));
    long previous = lastAttempt.load();
    if (now - previous < 2)
        return false;
    lastAttempt = now;
    openLogFile();
    return fdLog >= 0;
}

// An unhandled IL2CPP exception kills the process, so the PC of the last fault
// is the only reliable clue about where a crash came from. The previous handler
// (Unity installs one) is restored before the signal is re-raised, so the
// tombstone still happens.
struct SlotSignal {
    int sig;
    struct sigaction old;
};

// Return addresses on the crashed stack name the caller chain, but resolving
// them with dladdr() would take the linker lock in an already broken process.
// Module ranges are therefore captured before any crash and refreshed after the
// game libraries load, and the handler only compares numbers. Only executable
// segments are stored, so a data word that happens to sit in a loaded module is
// not read as code.
struct Module {
    const char* name;
    uintptr_t initial;
    uintptr_t end;
};

constexpr int kLimitModule = 256;
Module module[kLimitModule] = {};
int countModule = 0;
bool handlerInstalled = false;

// A return address on arm64 can carry pointer authentication (PAC) bits from the
// function that used it, while module addresses do not. The top bits are stripped
// before the comparison, because otherwise every frame looks outside any module
// and is never printed.
uintptr_t withoutPac(uintptr_t address) {
#if defined(__aarch64__)
    return address & 0x0000FFFFFFFFFFFFull;
#else
    return address;
#endif
}

int collectModule(struct dl_phdr_info* info, size_t, void*) {
    if (countModule >= kLimitModule)
        return 1;
    if (!info->dlpi_name || !info->dlpi_name[0])
        return 0;
    uintptr_t initial = UINTPTR_MAX;
    uintptr_t end = 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)& ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_LOAD || !(ph.p_flags & PF_X))
            continue;
        uintptr_t lo = info->dlpi_addr + ph.p_vaddr;
        uintptr_t hi = lo + ph.p_memsz;
        if (lo < initial)
            initial = lo;
        if (hi > end)
            end = hi;
    }
    if (end <= initial)
        return 0;
    const char* lineSlant = strrchr(info->dlpi_name, '/');
    module[countModule].name = lineSlant ? lineSlant + 1 : info->dlpi_name;
    module[countModule].initial = initial;
    module[countModule].end = end;
    countModule++;
    return 0;
}

// The caller of abort() and __stack_chk_fail() is recorded by the probes below,
// because the PC of the signal itself always lands inside libc and a frame scan
// has no frame pointer to follow. Non-zero values are resolved through the same
// module table, so the next SIGABRT names the code that gave up.
std::atomic<uintptr_t> pemanggilAbort{0};
std::atomic<uintptr_t> pemanggilStackCheck{0};
void* abortOriginal = nullptr;
void* stackCheckOriginal = nullptr;

// bionic stores the message set through android_set_abort_message() and shows it
// in the tombstone. The same message is printed here, because it often names
// which check decided to kill the process.
char messageAbort[256] = {};
void* messageAbortOriginal = nullptr;

bool writeFrameCode(int fd, const char* clock, const char* marker, uintptr_t address) {
    address = withoutPac(address);
    for (int m = 0; m < countModule; m++) {
        if (address < module[m].initial || address >= module[m].end)
            continue;
        char row[512];
        int n = snprintf(row, sizeof(row), "[%s] ERROR CRASH %s %s+0x%lx\n", clock, marker,
                         module[m].name, static_cast<unsigned long>(address - module[m].initial));
        if (n > 0 && fd >= 0)
            write(fd, row, static_cast<size_t>(n));
        return true;
    }
    return false;
}

using FatalFunction = void (*)(void);

// Unhandled exceptions, failed assertions and heap checks all end in abort(), so
// its caller shows which component killed the process.
void probeAbort() {
    pemanggilAbort.store(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    auto original = reinterpret_cast<FatalFunction>(abortOriginal);
    if (original)
        original();
    _exit(127);
}

// The game code itself is built with a stack protector, so a broken canary names
// the function that wrote past its frame.
void probeStackCheck() {
    pemanggilStackCheck.store(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    auto original = reinterpret_cast<FatalFunction>(stackCheckOriginal);
    if (original)
        original();
    _exit(127);
}

void probeMessageAbort(const char* message) {
    if (message)
        snprintf(messageAbort, sizeof(messageAbort), "%s", message);
    auto original = reinterpret_cast<void (*)(const char*)>(messageAbortOriginal);
    if (original)
        original(message);
}

// A probe address does not always fall inside a captured module, so the raw
// address is still printed so it can be matched against a dump or tombstone.
void writeMarker(int fd, const char* clock, const char* marker, uintptr_t address) {
    if (!address)
        return;
    address = withoutPac(address);
    if (writeFrameCode(fd, clock, marker, address))
        return;
    char row[256];
    int n = snprintf(row, sizeof(row), "[%s] ERROR CRASH %s raw=0x%lx\n", clock, marker,
                     static_cast<unsigned long>(address));
    if (n > 0 && fd >= 0)
        write(fd, row, static_cast<size_t>(n));
}

// The thread that calls abort is not always the main thread, and its name shows
// which component was running when the process was killed.
void writeThread(int fd, const char* clock) {
    long tid = syscall(SYS_gettid);
    char name[80] = "?";
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/task/%ld/comm", tid);
    int file = open(path, O_RDONLY | O_CLOEXEC);
    if (file >= 0) {
        ssize_t n = read(file, name, sizeof(name) - 1);
        close(file);
        if (n > 0) {
            name[n] = '\0';
            char* end = strchr(name, '\n');
            if (end)
                *end = '\0';
        }
    }
    char row[192];
    int n = snprintf(row, sizeof(row), "[%s] ERROR CRASH thread=%s tid=%ld\n", clock, name, tid);
    if (n > 0 && fd >= 0)
        write(fd, row, static_cast<size_t>(n));
}

SlotSignal slotList[] = {
    {SIGSEGV, {}},
    {SIGBUS, {}},
    {SIGILL, {}},
    {SIGABRT, {}},
};

void handleCrash(int sig, siginfo_t* info, void* context) {
    auto* uc = static_cast<ucontext_t*>(context);
    uintptr_t pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
    uintptr_t lr = static_cast<uintptr_t>(uc->uc_mcontext.regs[30]);
    uintptr_t fault = reinterpret_cast<uintptr_t>(info->si_addr);
    Dl_info dl = {};
    uintptr_t offset = 0;
    const char* name = "?";
    if (dladdr(reinterpret_cast<void*>(pc), &dl) && dl.dli_fbase) {
        name = dl.dli_fname;
        offset = pc - reinterpret_cast<uintptr_t>(dl.dli_fbase);
    }
    // x19/x20 are the end/start pointers of a UTF-16 scan in libunity, so the
    // pair shows whether a wrong string length pushed it past the buffer.
    char clock[16];
    formatTime(clock, sizeof(clock), "%H:%M:%S");
    long tid = syscall(SYS_gettid);
    char row[512];
    int n = snprintf(row, sizeof(row),
                     "[%s] ERROR CRASH signal=%s code=%d pc=0x%lx module=%s+0x%lx "
                     "fault=0x%lx tid=%ld\n",
                     clock, signalName(sig), info->si_code, static_cast<unsigned long>(pc), name,
                     static_cast<unsigned long>(offset), static_cast<unsigned long>(fault), tid);
    int fd = fdLog;
    if (n > 0 && fd >= 0)
        write(fd, row, static_cast<size_t>(n));

    n = snprintf(row, sizeof(row), "[%s] ERROR CRASH regs lr=0x%lx x19=0x%lx x20=0x%lx sp=0x%lx\n",
                 clock, static_cast<unsigned long>(withoutPac(lr)),
                 static_cast<unsigned long>(uc->uc_mcontext.regs[19]),
                 static_cast<unsigned long>(uc->uc_mcontext.regs[20]),
                 static_cast<unsigned long>(uc->uc_mcontext.sp));
    if (n > 0 && fd >= 0)
        write(fd, row, static_cast<size_t>(n));

    writeThread(fd, clock);
    if (messageAbort[0]) {
        n = snprintf(row, sizeof(row), "[%s] ERROR CRASH abort message=%s\n", clock, messageAbort);
        if (n > 0 && fd >= 0)
            write(fd, row, static_cast<size_t>(n));
    }

    // The probe knows where abort() was called from, and that is the only frame
    // that matters when IL2CPP or the C++ runtime decides to end the process.
    writeMarker(fd, clock, "abort caller", pemanggilAbort.load(std::memory_order_relaxed));
    writeMarker(fd, clock, "stack check caller",
                pemanggilStackCheck.load(std::memory_order_relaxed));

    // Only words that fall inside an executable module segment are printed, so
    // heap values, module data and stale stack words stay out of the log.
    uintptr_t sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);
    int show = 0;
    for (int i = 0; i < 512 && show < 16; i++) {
        uintptr_t candidate = reinterpret_cast<const uintptr_t*>(sp)[i];
        if (writeFrameCode(fd, clock, "frame", candidate))
            show++;
    }
    for (SlotSignal& slot : slotList) {
        if (slot.sig == sig) {
            sigaction(sig, &slot.old, nullptr);
            break;
        }
    }
    raise(sig);
}

} // namespace

const char* logFilePath() { return pathSession[0] ? pathSession : nullptr; }

void openLogFile() {
    if (fdLog >= 0)
        return;
    std::lock_guard<std::mutex> hold(keyOpen);
    if (fdLog >= 0)
        return;
    char directory[512];
    bool fromActivity = directoryFile(directory, sizeof(directory));
    if (!fromActivity && !directoryOld(directory, sizeof(directory)))
        return;

    mkdir(directory, 0777);

    char path[600];
    int error = 0;
    int fd = openNew(directory, path, sizeof(path), &error);
    bool diperbaiki = false;
    // The directory is the only thing this process may have to repair; if
    // creation still fails, the ensureOpen retry runs two seconds later.
    if (fd < 0 && (error == EACCES || error == EPERM)) {
        chmod(directory, 0777);
        fd = openNew(directory, path, sizeof(path), &error);
        diperbaiki = fd >= 0;
    }
    if (fd < 0)
        return;
    fdLog = fd;
    snprintf(pathSession, sizeof(pathSession), "%s", path);

    char clock[16];
    char date[16];
    formatTime(clock, sizeof(clock), "%H:%M:%S");
    formatTime(date, sizeof(date), "%Y-%m-%d");
    char row[640];
    int n = snprintf(row, sizeof(row), "[%s] INFO  Log started (%s)\n", clock, date);
    if (n > 0)
        write(fd, row, static_cast<size_t>(n));
    // The directory is always Android/data/<package>/files, so the full path
    // would only repeat what the reader already knows and make the line hard to
    // scan.
    const char* lineSlant = strrchr(path, '/');
    const char* name = lineSlant ? lineSlant + 1 : path;
    n = snprintf(row, sizeof(row), "[%s] INFO  Log file: %s (new session%s)\n", clock, name,
                 fromActivity ? "" : ", package fallback");
    if (n > 0)
        write(fd, row, static_cast<size_t>(n));
    if (diperbaiki) {
        n = snprintf(row, sizeof(row),
                     "[%s] ERROR Log directory not writable (%d %s), mode repaired\n", clock, error,
                     strerror(error));
        if (n > 0)
            write(fd, row, static_cast<size_t>(n));
    }
    flush();
}

void printLog(char level, const char* fmt, ...) {
    // The prefix and the message are written into one buffer, so there is no
    // 400-byte copy per line before the line is appended.
    char row[512];
    int initial = snprintf(row, sizeof(row), "[%s] %s ", timeNow(), nameLevel(level));
    if (initial <= 0 || initial >= static_cast<int>(sizeof(row)) - 1)
        return;

    va_list argument;
    va_start(argument, fmt);
    int content =
        vsnprintf(row + initial, sizeof(row) - static_cast<size_t>(initial) - 1, fmt, argument);
    va_end(argument);
    if (content < 0)
        return;

    // vsnprintf reports the length before truncation, so the newline position is clamped.
    size_t end = static_cast<size_t>(initial) + static_cast<size_t>(content);
    if (end > sizeof(row) - 2)
        end = sizeof(row) - 2;
    row[end++] = '\n';
    ensureOpen();
    writeRow(row, static_cast<int>(end));
}

void refreshModules() {
    countModule = 0;
    dl_iterate_phdr(collectModule, nullptr);
}

void installCrashLog() {
    refreshModules();
    // The handler is installed only once; later calls just refresh the module
    // list, because slot.old already holds our own handler.
    if (handlerInstalled)
        return;
    handlerInstalled = true;
    for (SlotSignal& slot : slotList) {
        struct sigaction action = {};
        action.sa_sigaction = handleCrash;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        sigaction(slot.sig, &action, &slot.old);
    }
}

// Both probes hook libc directly, which no target names, so any install failure
// only removes that one frame from the next crash block.
void installCrashProbe() {
    void* addressAbort = dlsym(RTLD_DEFAULT, "abort");
    installProbeHook(addressAbort, reinterpret_cast<void*>(probeAbort), &abortOriginal, "abort");
    void* addressStackCheck = dlsym(RTLD_DEFAULT, "__stack_chk_fail");
    installProbeHook(addressStackCheck, reinterpret_cast<void*>(probeStackCheck),
                     &stackCheckOriginal, "__stack_chk_fail");
    // libc exports this function as a weak symbol, so its absence only removes
    // one message line from the crash block.
    void* addressMessage = dlsym(RTLD_DEFAULT, "android_set_abort_message");
    installProbeHook(addressMessage, reinterpret_cast<void*>(probeMessageAbort),
                     &messageAbortOriginal, "android_set_abort_message");
}

#else

// log.h provides no-op stubs; this keeps the translation unit non-empty.
static_assert(true, "");

#endif
