package com.android.support;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;

/**
 * Memuat ikon melayang dari URL, data URI, atau berkas, lalu mendekodenya
 * menjadi Drawable lewat {@link IkonGambar}. Tidak memakai WebView.
 *
 * URL tidak diunduh ulang tiap menu dibuka. Urutannya memori (selama proses
 * hidup), berkas di cacheDir/ikon, lalu jaringan. Berkas yang lebih tua dari
 * {@link #UMUR_SEGAR_MS} tetap langsung dipakai dan diperbarui diam-diam untuk
 * pembukaan berikutnya. Hanya satu ikon yang disimpan: berkas URL lama dihapus
 * begitu URL baru berhasil diunduh.
 */
public final class IconCache {
    private static final String TAG = BKLog.TAG;
    private static final String FOLDER = "ikon";
    private static final int LIMIT_BYTE = 3 * 1024 * 1024;
    private static final int TIMEOUT_MS = 8000;
    private static final int MAX_SWITCH = 5;
    private static final long UMUR_SEGAR_MS = 7L * 24L * 60L * 60L * 1000L;

    /** Dipanggil di thread utama. */
    public interface Pendengar {
        void ready(Drawable icon);

        void fail();
    }

    private static final Object key = new Object();
    private static String sourceMemory;
    private static byte[] fillMemory;

    private IconCache() {
    }

    public static void load(Context context, final String source, final Pendengar pendengar) {
        final Context app = context.getApplicationContext();
        final Handler main = new Handler(Looper.getMainLooper());

        new Thread(new Runnable() {
            @Override
            public void run() {
                Drawable result = null;
                try {
                    byte[] content = ambil(app, source);
                    if (content != null)
                        result = IconImage.create(app.getResources(), content);
                } catch (Throwable t) {
                    Log.d(TAG, "Ikon gagal dimuat: " + t);
                }
                final Drawable icon = result;
                main.post(new Runnable() {
                    @Override
                    public void run() {
                        if (icon != null)
                            pendengar.ready(icon);
                        else
                            pendengar.fail();
                    }
                });
            }
        }, "IkonCache").start();
    }

    private static byte[] ambil(Context app, String source) {
        byte[] fromMemory = readMemory(source);
        if (fromMemory != null)
            return fromMemory;

        byte[] content;
        if (source.startsWith("http://") || source.startsWith("https://"))
            content = fetchUrl(app, source);
        else if (source.startsWith("data:"))
            content = fromDataUri(source);
        else
            content = read(new File(source.startsWith("file://") ? source.substring(7) : source));

        if (content != null && shapeImage(content))
            saveMemory(source, content);
        else
            content = null;
        return content;
    }

    private static byte[] fetchUrl(Context app, String url) {
        File file = fileFor(app, url);

        if (file.isFile() && file.length() > 0) {
            byte[] content = read(file);
            if (content != null && shapeImage(content)) {
                if (System.currentTimeMillis() - file.lastModified() > UMUR_SEGAR_MS)
                    refreshIdle(app, url, file);
                return content;
            }
            // Berkas rusak: dibuang, lalu diunduh ulang.
            file.delete();
        }

        byte[] content = unduh(url);
        if (content == null || !shapeImage(content))
            return null;
        write(app, file, content);
        return content;
    }

    private static byte[] fromDataUri(String uri) {
        int koma = uri.indexOf(',');
        if (koma < 0)
            return null;
        try {
            return Base64.decode(uri.substring(koma + 1), Base64.DEFAULT);
        } catch (Throwable t) {
            return null;
        }
    }

    /** Memperbarui berkas lama di thread sendiri; hasilnya dipakai pada pembukaan berikutnya. */
    private static void refreshIdle(final Context app, final String url, final File file) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                byte[] content = unduh(url);
                if (content == null || !shapeImage(content)) {
                    // Gagal: umur berkas diperpanjang supaya tidak mencoba terus tiap masuk.
                    file.setLastModified(System.currentTimeMillis());
                    return;
                }
                write(app, file, content);
            }
        }, "IkonCache-segar").start();
    }

    private static File folder(Context app) {
        File dir = new File(app.getCacheDir(), FOLDER);
        if (!dir.isDirectory())
            dir.mkdirs();
        return dir;
    }

    private static File fileFor(Context app, String url) {
        return new File(folder(app), sha1(url) + ".img");
    }

    private static void write(Context app, File file, byte[] content) {
        File temporary = new File(file.getPath() + ".tmp");
        FileOutputStream exit = null;
        try {
            exit = new FileOutputStream(temporary);
            exit.write(content);
            exit.flush();
            exit.close();
            exit = null;
            if (file.exists())
                file.delete();
            if (!temporary.renameTo(file)) {
                temporary.delete();
                return;
            }
            // Hanya satu ikon yang disimpan: berkas milik URL lain dibuang.
            File[] all = folder(app).listFiles();
            if (all != null) {
                for (File f : all) {
                    if (!f.equals(file))
                        f.delete();
                }
            }
        } catch (Throwable t) {
            Log.d(TAG, "Ikon gagal disimpan: " + t);
            temporary.delete();
        } finally {
            if (exit != null) {
                try {
                    exit.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }

    private static byte[] read(File file) {
        if (!file.isFile() || file.length() > LIMIT_BYTE)
            return null;
        FileInputStream enter = null;
        try {
            enter = new FileInputStream(file);
            ByteArrayOutputStream exit = new ByteArrayOutputStream((int) file.length());
            byte[] buf = new byte[8192];
            int n;
            while ((n = enter.read(buf)) != -1)
                exit.write(buf, 0, n);
            return exit.toByteArray();
        } catch (Throwable t) {
            return null;
        } finally {
            if (enter != null) {
                try {
                    enter.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }

    private static byte[] unduh(String address) {
        HttpURLConnection koneksi = null;
        try {
            String now = address;
            for (int i = 0; i < MAX_SWITCH; i++) {
                koneksi = (HttpURLConnection) new URL(now).openConnection();
                koneksi.setConnectTimeout(TIMEOUT_MS);
                koneksi.setReadTimeout(TIMEOUT_MS);
                koneksi.setInstanceFollowRedirects(false);
                koneksi.setRequestProperty("User-Agent", "Mozilla/5.0");
                int code = koneksi.getResponseCode();

                if (code >= 300 && code < 400) {
                    String lokasi = koneksi.getHeaderField("Location");
                    koneksi.disconnect();
                    koneksi = null;
                    if (lokasi == null)
                        return null;
                    now = new URL(new URL(now), lokasi).toString();
                    continue;
                }
                if (code != 200)
                    return null;

                int length = koneksi.getContentLength();
                if (length > LIMIT_BYTE)
                    return null;

                InputStream enter = koneksi.getInputStream();
                ByteArrayOutputStream exit =
                        new ByteArrayOutputStream(length > 0 ? length : 16 * 1024);
                byte[] buf = new byte[8192];
                int n;
                int total = 0;
                while ((n = enter.read(buf)) != -1) {
                    total += n;
                    if (total > LIMIT_BYTE)
                        return null;
                    exit.write(buf, 0, n);
                }
                enter.close();
                return exit.toByteArray();
            }
        } catch (Throwable t) {
            Log.d(TAG, "Ikon gagal diunduh: " + t);
        } finally {
            if (koneksi != null)
                koneksi.disconnect();
        }
        return null;
    }

    /** Memeriksa tanda awal berkas supaya halaman galat HTML tidak ikut disimpan. */
    private static boolean shapeImage(byte[] b) {
        if (b == null || b.length < 12)
            return false;
        if (b[0] == 'G' && b[1] == 'I' && b[2] == 'F' && b[3] == '8')
            return true;
        if ((b[0] & 0xFF) == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G')
            return true;
        if ((b[0] & 0xFF) == 0xFF && (b[1] & 0xFF) == 0xD8)
            return true;
        if (b[0] == 'R' && b[1] == 'I' && b[2] == 'F' && b[3] == 'F'
                && b[8] == 'W' && b[9] == 'E' && b[10] == 'B' && b[11] == 'P')
            return true;
        return b[0] == 'B' && b[1] == 'M';
    }

    private static String sha1(String text) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            byte[] hash = md.digest(text.getBytes("UTF-8"));
            StringBuilder sb = new StringBuilder();
            for (byte x : hash)
                sb.append(String.format("%02x", x & 0xFF));
            return sb.toString();
        } catch (Throwable t) {
            return String.valueOf(text.hashCode());
        }
    }

    private static byte[] readMemory(String source) {
        synchronized (key) {
            return source.equals(sourceMemory) ? fillMemory : null;
        }
    }

    private static void saveMemory(String source, byte[] content) {
        synchronized (key) {
            sourceMemory = source;
            fillMemory = content;
        }
    }
}
