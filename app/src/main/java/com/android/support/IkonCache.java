package com.android.support;

import android.content.Context;
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
import java.util.Locale;

/**
 * Cache ikon dari URL, supaya gambar atau GIF tidak diunduh ulang tiap menu dibuka.
 *
 * Urutan: memori (selama proses hidup), lalu berkas di cacheDir/ikon, dan
 * baru terakhir jaringan. Berkas yang lebih tua dari {@link #UMUR_SEGAR_MS}
 * tetap langsung dipakai, lalu diperbarui diam-diam di latar belakang untuk
 * dipakai pada pembukaan berikutnya. Hanya satu ikon yang disimpan: berkas
 * milik URL lama dihapus begitu URL baru berhasil diunduh.
 *
 * Hasilnya berupa data URI sehingga WebView tidak perlu menyentuh jaringan
 * dan GIF tetap bergerak.
 */
public final class IkonCache {
    private static final String TAG = "Mod_Menu";
    private static final String FOLDER = "ikon";
    private static final int BATAS_BYTE = 3 * 1024 * 1024;
    private static final int TIMEOUT_MS = 8000;
    private static final int MAKS_ALIH = 5;
    private static final long UMUR_SEGAR_MS = 7L * 24L * 60L * 60L * 1000L;

    /** Dipanggil di thread utama. */
    public interface Pendengar {
        void siap(String dataUri);

        void gagal();
    }

    private static final Object kunci = new Object();
    private static String urlMemori;
    private static String dataMemori;

    private IkonCache() {
    }

    public static boolean cocok(String sumber) {
        return sumber != null && (sumber.startsWith("http://") || sumber.startsWith("https://"));
    }

    /** Memuat ikon dari memori, berkas, atau jaringan, lalu memanggil pendengar di thread utama. */
    public static void muat(Context context, final String url, final Pendengar pendengar) {
        final Context app = context.getApplicationContext();
        final Handler utama = new Handler(Looper.getMainLooper());

        String dariMemori = ambilMemori(url);
        if (dariMemori != null) {
            pendengar.siap(dariMemori);
            return;
        }

        new Thread(new Runnable() {
            @Override
            public void run() {
                String data = null;
                try {
                    data = ambil(app, url);
                } catch (Throwable t) {
                    Log.d(TAG, "Ikon gagal dimuat: " + t);
                }
                final String hasil = data;
                utama.post(new Runnable() {
                    @Override
                    public void run() {
                        if (hasil != null)
                            pendengar.siap(hasil);
                        else
                            pendengar.gagal();
                    }
                });
            }
        }, "IkonCache").start();
    }

    private static String ambil(Context app, String url) {
        File berkas = berkasUntuk(app, url);

        if (berkas.isFile() && berkas.length() > 0) {
            byte[] isi = baca(berkas);
            String data = isi == null ? null : keDataUri(isi);
            if (data != null) {
                simpanMemori(url, data);
                if (System.currentTimeMillis() - berkas.lastModified() > UMUR_SEGAR_MS)
                    segarkanDiam(app, url, berkas);
                return data;
            }
            // Berkas rusak: dibuang, lalu diunduh ulang.
            berkas.delete();
        }

        byte[] isi = unduh(url);
        if (isi == null)
            return null;
        String data = keDataUri(isi);
        if (data == null)
            return null;
        tulis(app, berkas, isi);
        simpanMemori(url, data);
        return data;
    }

    /** Memperbarui berkas lama di thread sendiri; hasilnya dipakai pada pembukaan berikutnya. */
    private static void segarkanDiam(final Context app, final String url, final File berkas) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                byte[] isi = unduh(url);
                if (isi == null || keDataUri(isi) == null) {
                    // Gagal: umur berkas diperpanjang supaya tidak mencoba terus tiap masuk.
                    berkas.setLastModified(System.currentTimeMillis());
                    return;
                }
                tulis(app, berkas, isi);
            }
        }, "IkonCache-segar").start();
    }

    private static File folder(Context app) {
        File dir = new File(app.getCacheDir(), FOLDER);
        if (!dir.isDirectory())
            dir.mkdirs();
        return dir;
    }

    private static File berkasUntuk(Context app, String url) {
        return new File(folder(app), sha1(url) + ".img");
    }

    private static void tulis(Context app, File berkas, byte[] isi) {
        File sementara = new File(berkas.getPath() + ".tmp");
        FileOutputStream keluar = null;
        try {
            keluar = new FileOutputStream(sementara);
            keluar.write(isi);
            keluar.flush();
            keluar.close();
            keluar = null;
            if (berkas.exists())
                berkas.delete();
            if (!sementara.renameTo(berkas)) {
                sementara.delete();
                return;
            }
            // Hanya satu ikon yang disimpan: berkas milik URL lain dibuang.
            File[] semua = folder(app).listFiles();
            if (semua != null) {
                for (File f : semua) {
                    if (!f.equals(berkas))
                        f.delete();
                }
            }
        } catch (Throwable t) {
            Log.d(TAG, "Ikon gagal disimpan: " + t);
            sementara.delete();
        } finally {
            if (keluar != null) {
                try {
                    keluar.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }

    private static byte[] baca(File berkas) {
        FileInputStream masuk = null;
        try {
            masuk = new FileInputStream(berkas);
            ByteArrayOutputStream keluar = new ByteArrayOutputStream((int) berkas.length());
            byte[] buf = new byte[8192];
            int n;
            while ((n = masuk.read(buf)) != -1)
                keluar.write(buf, 0, n);
            return keluar.toByteArray();
        } catch (Throwable t) {
            return null;
        } finally {
            if (masuk != null) {
                try {
                    masuk.close();
                } catch (Throwable ignored) {
                }
            }
        }
    }

    private static byte[] unduh(String alamat) {
        HttpURLConnection koneksi = null;
        try {
            String sekarang = alamat;
            for (int i = 0; i < MAKS_ALIH; i++) {
                koneksi = (HttpURLConnection) new URL(sekarang).openConnection();
                koneksi.setConnectTimeout(TIMEOUT_MS);
                koneksi.setReadTimeout(TIMEOUT_MS);
                koneksi.setInstanceFollowRedirects(false);
                koneksi.setRequestProperty("User-Agent", "Mozilla/5.0");
                int kode = koneksi.getResponseCode();

                if (kode >= 300 && kode < 400) {
                    String lokasi = koneksi.getHeaderField("Location");
                    koneksi.disconnect();
                    koneksi = null;
                    if (lokasi == null)
                        return null;
                    sekarang = new URL(new URL(sekarang), lokasi).toString();
                    continue;
                }
                if (kode != 200)
                    return null;

                int panjang = koneksi.getContentLength();
                if (panjang > BATAS_BYTE)
                    return null;

                InputStream masuk = koneksi.getInputStream();
                ByteArrayOutputStream keluar =
                        new ByteArrayOutputStream(panjang > 0 ? panjang : 16 * 1024);
                byte[] buf = new byte[8192];
                int n;
                int total = 0;
                while ((n = masuk.read(buf)) != -1) {
                    total += n;
                    if (total > BATAS_BYTE)
                        return null;
                    keluar.write(buf, 0, n);
                }
                masuk.close();
                return keluar.toByteArray();
            }
        } catch (Throwable t) {
            Log.d(TAG, "Ikon gagal diunduh: " + t);
        } finally {
            if (koneksi != null)
                koneksi.disconnect();
        }
        return null;
    }

    /** Null bila isinya bukan gambar (misalnya halaman galat HTML), supaya tidak ikut disimpan. */
    private static String keDataUri(byte[] b) {
        String mime = mime(b);
        if (mime == null)
            return null;
        return "data:" + mime + ";base64," + Base64.encodeToString(b, Base64.NO_WRAP);
    }

    private static String mime(byte[] b) {
        if (b == null || b.length < 12)
            return null;
        if (b[0] == 'G' && b[1] == 'I' && b[2] == 'F' && b[3] == '8')
            return "image/gif";
        if ((b[0] & 0xFF) == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G')
            return "image/png";
        if ((b[0] & 0xFF) == 0xFF && (b[1] & 0xFF) == 0xD8)
            return "image/jpeg";
        if (b[0] == 'R' && b[1] == 'I' && b[2] == 'F' && b[3] == 'F'
                && b[8] == 'W' && b[9] == 'E' && b[10] == 'B' && b[11] == 'P')
            return "image/webp";
        if (b[0] == 'B' && b[1] == 'M')
            return "image/bmp";
        String awal = new String(b, 0, Math.min(b.length, 256)).trim().toLowerCase(Locale.ROOT);
        if (awal.startsWith("<svg") || (awal.startsWith("<?xml") && awal.contains("<svg")))
            return "image/svg+xml";
        return null;
    }

    private static String sha1(String teks) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            byte[] hash = md.digest(teks.getBytes("UTF-8"));
            StringBuilder sb = new StringBuilder();
            for (byte x : hash)
                sb.append(String.format("%02x", x & 0xFF));
            return sb.toString();
        } catch (Throwable t) {
            return String.valueOf(teks.hashCode());
        }
    }

    private static String ambilMemori(String url) {
        synchronized (kunci) {
            return url.equals(urlMemori) ? dataMemori : null;
        }
    }

    private static void simpanMemori(String url, String data) {
        synchronized (kunci) {
            urlMemori = url;
            dataMemori = data;
        }
    }
}
