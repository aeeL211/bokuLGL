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

// Setiap sesi menulis berkasnya sendiri di direktori files eksternal aplikasi,
// yaitu Android/data/<package>/files/bokumods-1.log, bokumods-2.log dan
// seterusnya. Path-nya berasal dari Context.getExternalFilesDir() lewat
// UnityPlayer.currentActivity, jadi nama paket tidak pernah ditulis mati dan
// paket yang diganti nama tetap bekerja.
std::atomic<int> fdLog{-1};

// Dua pemanggil pertama (konstruktor pustaka dan JNI_OnLoad) bisa mencoba
// membuka berkas bersamaan. Kunci ini memastikan hanya satu berkas sesi yang
// dibuat; pemeriksaan fdLog di dalamnya menangani pemanggil yang datang
// belakangan.
std::mutex keyOpen;

// Path berkas yang dibuka sesi ini, disimpan supaya perintah bisa mencetaknya.
char pathSession[600] = {};

// Activity, dan bersamanya direktori log, baru ada beberapa detik setelah proses
// mulai. Baris dari jendela itu disimpan di sini dan disiram saat berkasnya
// akhirnya terbuka, supaya awal sesi tidak hilang.
constexpr int kLimitPending = 32;
constexpr int kLengthPending = 220;
char pending[kLimitPending][kLengthPending] = {};
std::atomic<int> countPending{0};

// Thread pekerja, JNI_OnLoad dan konstruktor semuanya menulis log sebelum
// berkasnya ada, jadi ring tertunda butuh kuncinya sendiri. Penangan sinyal tidak
// pernah mengambilnya: ia menulis langsung ke fdLog, yang sudah terbuka saat
// crash mulai penting.
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

// localtime_r dan strftime menghitung zona waktu pada setiap baris, padahal
// beberapa baris biasanya berbagi detik yang sama. Cache thread-local membuat
// hitungan itu hanya sekali per detik, tanpa kunci antar-thread.
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

// INFO diberi spasi tambahan sampai lima karakter supaya kolom pesan sejajar
// dengan ERROR.
const char* nameLevel(char tingkat) { return tingkat == 'E' ? "ERROR" : "INFO "; }

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
    // Periksa ulang di bawah kunci: pembuka berkas bisa memasang fdLog di antara
    // pemeriksaan sebelumnya dan penyimpanan ini, dan baris yang tertinggal
    // setelah siraman terakhir tidak akan pernah sampai ke berkas.
    keyPending();
    int fd = fdLog;
    if (fd >= 0) {
        openKeyPending();
        write(fd, row, static_cast<size_t>(n));
        return;
    }
    // Buffer penuh membuang baris tertua, bukan memblokir pemanggil.
    int slot = countPending.fetch_add(1) % kLimitPending;
    if (slot < 0)
        slot = 0;
    snprintf(pending[slot], kLengthPending, "%.*s", n, row);
    openKeyPending();
}

void siram() {
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

// getExternalFilesDir adalah API publik untuk direktori eksternal per aplikasi;
// memanggilnya menghindari menebak nama paket atau titik mount penyimpanan.
bool directoryFile(char* out, size_t size) {
    EnvScope lingkup;
    if (!lingkup)
        return false;
    JNIEnv* env = lingkup.env();

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

// Cadangan untuk jendela singkat sebelum UnityPlayer.currentActivity diisi:
// bangun direktori yang sama dari paket proses dan akar penyimpanan. Direktori
// paket sudah ada sejak pemasangan, jadi direktori yang hilang atau tidak bisa
// ditulis berarti tebakannya salah (paket diganti nama, klon, mount lain) dan
// membuatnya akan menyembunyikan log di luar sandbox aplikasi.
bool directoryOld(char* out, size_t size) {
    char packet[192];
    if (!packetApplication(packet, sizeof(packet)))
        return false;

    char root[512];
    if (!rootStorageExternal(root, sizeof(root)))
        return false;
    char directory[512];
    snprintf(directory, sizeof(directory), "%s/Android/data/%s/files", root, packet);
    if (access(directory, W_OK | X_OK) != 0)
        return false;
    snprintf(out, size, "%s", directory);
    return true;
}

// Berkas sesi membawa nomor sesinya, jadi namanya tetap pendek dan nomor
// tertinggi di folder selalu yang terbaru.
int numberSession(const char* name) {
    int value = 0;
    int terpakai = 0;
    if (sscanf(name, "bokumods-%d.log%n", &value, &terpakai) != 1)
        return 0;
    if (terpakai <= 0 || name[terpakai] != '\0' || value < 1)
        return 0;
    return value;
}

int sessionLast(const char* pathDirectory) {
    DIR* aliran = opendir(pathDirectory);
    if (!aliran)
        return 0;
    int last = 0;
    while (struct dirent* entry = readdir(aliran)) {
        int value = numberSession(entry->d_name);
        if (value > last)
            last = value;
    }
    closedir(aliran);
    return last;
}

// Satu berkas per sesi, bernomor sesuai urutan sesi dimulai. O_EXCL tidak pernah
// menyentuh berkas sebelumnya, dan nomornya hanya bertambah, jadi berkas yang
// dihapus tidak bisa mewariskan nomornya ke sesi berikutnya.
int openNew(const char* pathDirectory, char* path, size_t size, int* galat) {
    int first = sessionLast(pathDirectory) + 1;
    for (int i = first; i < first + 100; i++) {
        snprintf(path, size, "%s/bokumods-%d.log", pathDirectory, i);
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (fd >= 0)
            return fd;
        *galat = errno;
        if (*galat != EEXIST)
            return -1;
    }
    return -1;
}

bool ensureOpen() {
    if (fdLog >= 0)
        return true;
    // Direktori tidak terjangkau sebelum activity ada, jadi coba ulang paling
    // sering setiap dua detik, bukan pada setiap baris.
    static std::atomic<long> percobaanLast{0};
    long now = static_cast<long>(std::time(nullptr));
    long sebelumnya = percobaanLast.load();
    if (now - sebelumnya < 2)
        return false;
    percobaanLast = now;
    openLogFile();
    return fdLog >= 0;
}

// Eksepsi IL2CPP yang tidak tertangani mematikan proses, jadi PC dari fault
// terakhir adalah satu-satunya petunjuk andal tentang asal sebuah crash. Penangan
// sebelumnya (Unity memasang satu) dipulihkan sebelum sinyal di-raise ulang,
// supaya tombstone-nya tetap terjadi.
struct SlotSignal {
    int sig;
    struct sigaction old;
};

// Alamat kembali di stack yang crash menyebut rantai pemanggil, tetapi
// menyelesaikannya dengan dladdr() akan mengambil kunci linker di proses yang
// sudah rusak. Karena itu rentang modul dipotret sebelum ada crash dan
// disegarkan setelah pustaka game termuat, lalu penangannya hanya membandingkan
// angka. Hanya segmen executable yang disimpan, jadi kata data yang kebetulan
// berada di modul yang dimuat tidak dibaca sebagai kode.
struct Module {
    const char* name;
    uintptr_t initial;
    uintptr_t end;
};

constexpr int kLimitModule = 256;
Module module[kLimitModule] = {};
int countModule = 0;
bool handlerInstalled = false;

// Alamat kembali pada arm64 bisa membawa bit penandatangan pointer (PAC) dari
// fungsi yang memakainya, sedangkan alamat modul tidak. Bit atas dibuang sebelum
// perbandingan, karena tanpa itu setiap frame dianggap di luar modul dan tidak
// pernah tercetak.
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

// Pemanggil abort() dan __stack_chk_fail() dicatat oleh probe di bawah, karena PC
// dari sinyalnya sendiri selalu mendarat di dalam libc dan pemindaian frame tidak
// punya frame pointer untuk diikuti. Nilai bukan nol diselesaikan lewat tabel
// modul yang sama, jadi SIGABRT berikutnya menyebut kode yang menyerah.
std::atomic<uintptr_t> pemanggilAbort{0};
std::atomic<uintptr_t> pemanggilStackCheck{0};
void* abortOriginal = nullptr;
void* stackCheckOriginal = nullptr;

// bionic menyimpan pesan yang dipasang lewat android_set_abort_message() dan
// menampilkannya di tombstone. Pesan yang sama dicetak di sini, karena sering
// menyebut pemeriksaan mana yang memutuskan mematikan proses.
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

using FungsiFatal = void (*)(void);

// Eksepsi yang tidak tertangani, assertion yang gagal dan pemeriksaan heap
// semuanya berakhir di abort(), jadi pemanggilnya menunjukkan komponen yang
// mematikan proses.
void probeAbort() {
    pemanggilAbort.store(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    auto original = reinterpret_cast<FungsiFatal>(abortOriginal);
    if (original)
        original();
    _exit(127);
}

// Kode game sendiri dibangun dengan stack protector, jadi canary yang rusak
// menyebut fungsi yang menulis melewati frame-nya.
void probeStackCheck() {
    pemanggilStackCheck.store(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
    auto original = reinterpret_cast<FungsiFatal>(stackCheckOriginal);
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

// Alamat probe tidak selalu jatuh di modul yang terpotret, jadi alamat mentahnya
// tetap dicetak supaya bisa dicocokkan dengan dump atau tombstone.
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

// Thread yang memanggil abort bukan selalu thread utama, dan namanya
// menunjukkan komponen mana yang sedang berjalan saat proses dimatikan.
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
    int n =
        snprintf(row, sizeof(row), "[%s] ERROR CRASH thread=%s tid=%ld\n", clock, name, tid);
    if (n > 0 && fd >= 0)
        write(fd, row, static_cast<size_t>(n));
}

SlotSignal slotList[] = {
    {SIGSEGV, {}},
    {SIGBUS, {}},
    {SIGILL, {}},
    {SIGABRT, {}},
};

void tanganiCrash(int sig, siginfo_t* info, void* context) {
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
    // x19/x20 adalah pointer akhir/awal dari pemindaian UTF-16 di libunity, jadi
    // pasangan itu menunjukkan apakah panjang string yang salah mendorongnya
    // melewati buffer.
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

    n = snprintf(row, sizeof(row),
                 "[%s] ERROR CRASH regs lr=0x%lx x19=0x%lx x20=0x%lx sp=0x%lx\n", clock,
                 static_cast<unsigned long>(withoutPac(lr)),
                 static_cast<unsigned long>(uc->uc_mcontext.regs[19]),
                 static_cast<unsigned long>(uc->uc_mcontext.regs[20]),
                 static_cast<unsigned long>(uc->uc_mcontext.sp));
    if (n > 0 && fd >= 0)
        write(fd, row, static_cast<size_t>(n));

    writeThread(fd, clock);
    if (messageAbort[0]) {
        n = snprintf(row, sizeof(row), "[%s] ERROR CRASH abort message=%s\n", clock,
                     messageAbort);
        if (n > 0 && fd >= 0)
            write(fd, row, static_cast<size_t>(n));
    }

    // Probe tahu dari mana abort() dipanggil, dan itulah satu-satunya frame yang
    // berarti saat IL2CPP atau runtime C++ memutuskan mengakhiri proses.
    writeMarker(fd, clock, "abort caller", pemanggilAbort.load(std::memory_order_relaxed));
    writeMarker(fd, clock, "stack check caller",
                 pemanggilStackCheck.load(std::memory_order_relaxed));

    // Hanya kata yang jatuh di dalam segmen modul executable yang dicetak, jadi
    // nilai heap, data modul dan kata stack basi tetap di luar log.
    uintptr_t sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);
    int show = 0;
    for (int i = 0; i < 512 && show < 16; i++) {
        uintptr_t kandidat = reinterpret_cast<const uintptr_t*>(sp)[i];
        if (writeFrameCode(fd, clock, "frame", kandidat))
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
    int galat = 0;
    int fd = openNew(directory, path, sizeof(path), &galat);
    bool diperbaiki = false;
    // Direktori adalah satu-satunya hal yang mungkin harus diperbaiki proses ini;
    // bila pembuatan tetap gagal, percobaan ulang pastikanTerbuka berjalan dua
    // detik kemudian.
    if (fd < 0 && (galat == EACCES || galat == EPERM)) {
        chmod(directory, 0777);
        fd = openNew(directory, path, sizeof(path), &galat);
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
    int n = snprintf(row, sizeof(row), "[%s] INFO  Log dimulai (%s)\n", clock, date);
    if (n > 0)
        write(fd, row, static_cast<size_t>(n));
    // Direktorinya selalu Android/data/<package>/files, jadi path lengkap hanya
    // mengulang apa yang sudah diketahui pembaca dan membuat barisnya sulit
    // dipindai.
    const char* lineSlant = strrchr(path, '/');
    const char* name = lineSlant ? lineSlant + 1 : path;
    n = snprintf(row, sizeof(row), "[%s] INFO  Berkas log: %s (sesi baru%s)\n", clock, name,
                 fromActivity ? "" : ", cadangan paket");
    if (n > 0)
        write(fd, row, static_cast<size_t>(n));
    if (diperbaiki) {
        n = snprintf(row, sizeof(row),
                     "[%s] ERROR Direktori log tidak bisa ditulis (%d %s), mode diperbaiki\n",
                     clock, galat, strerror(galat));
        if (n > 0)
            write(fd, row, static_cast<size_t>(n));
    }
    siram();
}

void printLog(char tingkat, const char* fmt, ...) {
    // Prefiks dan pesan ditulis ke satu buffer, jadi tidak ada salinan 400 byte
    // per baris sebelum barisnya ditempel.
    char row[512];
    int initial = snprintf(row, sizeof(row), "[%s] %s ", timeNow(), nameLevel(tingkat));
    if (initial <= 0 || initial >= static_cast<int>(sizeof(row)) - 1)
        return;

    va_list argumen;
    va_start(argumen, fmt);
    int content = vsnprintf(row + initial, sizeof(row) - static_cast<size_t>(initial) - 1, fmt, argumen);
    va_end(argumen);
    if (content < 0)
        return;

    // vsnprintf melaporkan panjang sebelum pemotongan, jadi posisi newline dibatasi.
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
    // Penangan hanya dipasang sekali; pemanggilan berikutnya cukup menyegarkan
    // daftar modul, karena slot.lama sudah berisi penangan kita sendiri.
    if (handlerInstalled)
        return;
    handlerInstalled = true;
    for (SlotSignal& slot : slotList) {
        struct sigaction action = {};
        action.sa_sigaction = tanganiCrash;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        sigaction(slot.sig, &action, &slot.old);
    }
}

// Kedua probe memasang hook langsung ke libc, yang tidak disebut target mana pun,
// jadi setiap kegagalan pemasangan hanya menghilangkan satu frame itu dari blok
// crash berikutnya.
void installCrashProbe() {
    void* addressAbort = dlsym(RTLD_DEFAULT, "abort");
    installProbeHook(addressAbort, reinterpret_cast<void*>(probeAbort), &abortOriginal, "abort");
    void* addressStackCheck = dlsym(RTLD_DEFAULT, "__stack_chk_fail");
    installProbeHook(addressStackCheck, reinterpret_cast<void*>(probeStackCheck), &stackCheckOriginal,
                    "__stack_chk_fail");
    // libc mengekspor fungsi ini sebagai simbol lemah, jadi ketiadaannya hanya
    // menghilangkan satu baris pesan dari blok crash.
    void* addressMessage = dlsym(RTLD_DEFAULT, "android_set_abort_message");
    installProbeHook(addressMessage, reinterpret_cast<void*>(probeMessageAbort), &messageAbortOriginal,
                    "android_set_abort_message");
}

#else

// log.h menyediakan stub tanpa operasi; ini menjaga unit terjemahan tetap tidak
// kosong.
static_assert(true, "");

#endif
