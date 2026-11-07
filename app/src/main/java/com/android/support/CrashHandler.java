package com.android.support;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.Thread.UncaughtExceptionHandler;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;
import java.util.Map;

public final class CrashHandler {

    private static final String TAG = BKLog.TAG;
    private static final String LOG_DIR = "crash_logs";
    private static final String FILE_PREFIX = "crash_";
    private static final String FILE_EXTENSION = ".txt";
    private static final int LOG_DISIMPAN = 5;
    private static final long TOAST_TAMPIL_MS = 2000L;
    private static final long TUNGGU_MAKS_MS = 2500L;

    private static UncaughtExceptionHandler defaultHandler;
    private static Context appContext;
    private static volatile boolean sedangMenangani;
    final static boolean showToasts = true;

    private CrashHandler() {
    }

    /// Due to changes in access to application folders and to maintain convenience, the path has been changed to: android/media/PACKAGE/files/LOG_DIR.
    /// This path should be accessible on all devices
    public static synchronized void init(final Context context) {
        if (context == null)
            return;
        appContext = context.getApplicationContext();

        // Dipanggil dua kali (MainActivity lalu Main) tidak boleh membungkus
        // handler milik kita sendiri, supaya tidak ada rantai handler yang bertambah.
        UncaughtExceptionHandler sekarang = Thread.getDefaultUncaughtExceptionHandler();
        if (!(sekarang instanceof PenangkapCrash))
            defaultHandler = sekarang;

        cleanOldLogs();

        Thread.setDefaultUncaughtExceptionHandler(new PenangkapCrash());
    }

    private static final class PenangkapCrash implements UncaughtExceptionHandler {
        @Override
        public void uncaughtException(Thread thread, Throwable throwable) {
            Log.e(TAG, "Uncaught exception detected", throwable);

            // Crash di dalam penanganan crash langsung diteruskan ke handler asli.
            if (sedangMenangani) {
                teruskan(thread, throwable);
                return;
            }
            sedangMenangani = true;

            try {
                handleException(thread, throwable);
            } catch (Throwable e) {
                Log.e(TAG, "Error in crash handler", e);
                teruskan(thread, throwable);
            }
        }
    }

    private static void teruskan(Thread thread, Throwable throwable) {
        if (defaultHandler != null) {
            defaultHandler.uncaughtException(thread, throwable);
        } else {
            android.os.Process.killProcess(android.os.Process.myPid());
            System.exit(10);
        }
    }

    private static void handleException(Thread thread, Throwable throwable) {
        // Save crash log
        File crashFile = saveCrashLog(throwable, thread);

        // Show notification to user. Toast butuh Looper, dan thread yang crash
        // (apalagi thread utama) tidak bisa dipakai, jadi toast dijalankan di
        // thread pembantu dengan Looper sendiri.
        Thread penampil = null;
        if (showToasts && appContext != null)
            penampil = tampilkanToast(crashFile);

        // Wait a bit for toast to show and log to be saved
        if (penampil != null) {
            try {
                penampil.join(TUNGGU_MAKS_MS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        teruskan(thread, throwable);
    }

    private static File saveCrashLog(Throwable throwable, Thread sumber) {
        File dir = getLogDirectory();
        if (dir == null)
            return null;

        String timeStamp = new SimpleDateFormat("yyyy_MM_dd-HH_mm_ss", Locale.US)
                .format(new Date());

        String fileName = FILE_PREFIX + timeStamp + FILE_EXTENSION;
        File logFile = new File(dir, fileName);

        try {
            writeCrashLog(logFile, throwable, sumber);
            Log.i(TAG, "Crash log saved to: " + logFile.getAbsolutePath());
            return logFile;
        } catch (IOException e) {
            Log.e(TAG, "Failed to save crash log", e);
            return null;
        }
    }

    private static void writeCrashLog(File file, Throwable throwable, Thread sumber)
            throws IOException {
        File parentDir = file.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            if (!parentDir.mkdirs()) {
                Log.w(TAG, "Failed to create directory: " + parentDir.getAbsolutePath());
            }
        }

        String logContent = buildCrashReport(throwable, sumber);

        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(logContent.getBytes(StandardCharsets.UTF_8));
            fos.flush();
        }
    }

    private static String buildCrashReport(Throwable throwable, Thread sumber) {
        StringBuilder report = new StringBuilder(8192);

        // App and device info
        report.append("=============== CRASH REPORT ===============\n");
        report.append("Time: ").append(getCurrentTime()).append("\n");
        report.append("App Version: ").append(getAppVersion()).append("\n");
        report.append("Android API: ").append(Build.VERSION.SDK_INT).append("\n");
        report.append("Android Version: ").append(Build.VERSION.RELEASE).append("\n");
        report.append("Device: ").append(Build.MANUFACTURER).append(" ").append(Build.MODEL).append("\n");
        report.append("Product: ").append(Build.PRODUCT).append("\n");
        report.append("Board: ").append(Build.BOARD).append("\n");
        report.append("Fingerprint: ").append(Build.FINGERPRINT).append("\n");
        // Thread yang crash, bukan thread yang sedang menulis laporan.
        Thread asal = sumber != null ? sumber : Thread.currentThread();
        report.append("Thread: ").append(asal.getName()).append("\n");
        report.append("=============== STACK TRACE ===============\n");

        // printStackTrace sudah mencetak seluruh rantai "Caused by", jadi tidak
        // perlu menelusuri getCause() sendiri (versi lama menulisnya ke writer
        // yang isinya sudah diambil, sehingga rantai penyebab tidak pernah masuk).
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        throwable.printStackTrace(pw);
        pw.flush();
        report.append(sw.toString());

        // Thread dump
        report.append("\n\n=============== ALL THREADS ===============\n");
        Map<Thread, StackTraceElement[]> allThreads = Thread.getAllStackTraces();
        for (Map.Entry<Thread, StackTraceElement[]> entry : allThreads.entrySet()) {
            Thread thread = entry.getKey();
            report.append("\nThread: ").append(thread.getName())
                    .append(" [ID: ").append(thread.getId())
                    .append(", State: ").append(thread.getState())
                    .append(", Priority: ").append(thread.getPriority())
                    .append("]\n");

            for (StackTraceElement element : entry.getValue()) {
                report.append("    at ").append(element.toString()).append("\n");
            }
        }

        return report.toString();
    }

    private static File getLogDirectory() {
        if (appContext == null) {
            Log.e(TAG, "CrashHandler belum diinisialisasi");
            return null;
        }

        File standardAppDir = appContext.getExternalFilesDir(null);
        if (standardAppDir == null) {
            Log.e(TAG, "Cannot get app directory, using internal storage");
            return new File(appContext.getFilesDir(), LOG_DIR);
        }

        String standardPath = standardAppDir.getAbsolutePath();
        String targetPath = standardPath.replace(
                "Android/data/" + appContext.getPackageName(),
                "Android/media/" + appContext.getPackageName()
        );
        File logDir = new File(targetPath, LOG_DIR);

        if (!logDir.exists()) {
            if (logDir.mkdirs()) {
                Log.i(TAG, "Created log directory: " + logDir.getAbsolutePath());
            } else {
                Log.e(TAG, "Failed to create Android/media directory, falling back to standard location");
                return new File(standardAppDir, LOG_DIR);
            }
        }
        return logDir;
    }

    private static String getDisplayPath(File file) {
        if (file == null) return null;
        String absolutePath = file.getAbsolutePath();
        int androidIndex = absolutePath.indexOf("/Android/");
        if (androidIndex != -1) {
            return absolutePath.substring(androidIndex + 1);
        }
        return file.getName();
    }

    /** Menampilkan toast di thread pembantu ber-Looper selama {@link #TOAST_TAMPIL_MS}. */
    private static Thread tampilkanToast(final File crashFile) {
        final String message;
        if (crashFile != null && crashFile.exists()) {
            message = "CRASH! check:\n" + getDisplayPath(crashFile);
        } else {
            message = "CRASH! Could not save log.";
        }

        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    Looper.prepare();
                    Toast.makeText(appContext, message, Toast.LENGTH_LONG).show();
                    final Looper looper = Looper.myLooper();
                    new Handler(looper).postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            looper.quit();
                        }
                    }, TOAST_TAMPIL_MS);
                    Looper.loop();
                } catch (Throwable t) {
                    Log.e(TAG, "Toast crash gagal ditampilkan", t);
                }
            }
        }, "CrashToast");
        thread.start();
        return thread;
    }

    private static String getAppVersion() {
        if (appContext == null) return "unknown";

        try {
            PackageInfo pInfo = appContext.getPackageManager()
                    .getPackageInfo(appContext.getPackageName(), 0);
            long versionCode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ?
                    pInfo.getLongVersionCode() : pInfo.versionCode;
            return pInfo.versionName + " (" + versionCode + ")";
        } catch (PackageManager.NameNotFoundException e) {
            return "unknown";
        }
    }

    private static String getCurrentTime() {
        return new SimpleDateFormat("yyyy-MM-dd HH:mm:ss Z", Locale.US)
                .format(new Date());
    }

    /// utility method to manually log an exception
    public static void logException(Throwable throwable) {
        Log.e(TAG, "Manual exception logging", throwable);
        saveCrashLog(throwable, Thread.currentThread());
    }

    /// clear old crash logs (keep last 5)
    public static void cleanOldLogs() {
        File logDir = getLogDirectory();
        if (logDir == null || !logDir.exists() || !logDir.isDirectory()) return;

        File[] logs = logDir.listFiles((dir, name) ->
                name.startsWith(FILE_PREFIX) && name.endsWith(FILE_EXTENSION));

        if (logs != null && logs.length > LOG_DISIMPAN) {
            Arrays.sort(logs, (f1, f2) ->
                    Long.compare(f2.lastModified(), f1.lastModified()));

            // Versi lama mulai dari indeks 10, jadi yang tersisa 10 berkas
            // padahal komentarnya "keep last 5".
            for (int i = LOG_DISIMPAN; i < logs.length; i++) {
                if (!logs[i].delete()) {
                    Log.w(TAG, "Failed to delete old log: " + logs[i].getName());
                }
            }
        }
    }
}
