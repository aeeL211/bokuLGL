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
public final class IkonCache {
    private static final String TAG = BKLog.TAG;
    private static final String FOLDER = "ikon";
    private static final int BATAS_BYTE = 3 * 1024 * 1024;
    private static final int TIMEOUT_MS = 8000;
    private static final int MAKS_ALIH = 5;
    private static final long UMUR_SEGAR_MS = 7L * 24L * 60L * 60L * 1000L;

    /** Dipanggil di thread utama. */
    public interface Pendengar {
        void siap(Drawable ikon);

        void gagal();
    }

    private static final Object kunci = new Object();
    private static String sumberMemori;
    private static byte[] isiMemori;

    private IkonCache() {
    }

    public static void muat(Context context, final String sumber, final Pendengar pendengar) {
        final Context app = context.getApplicationContext();
        final Handler utama = new Handler(Looper.getMainLooper());

        new Thread(new Runnable() {
            @Override
            public void run() {
                Drawable hasil = null;
                try {
                    byte[] isi = ambil(app, sumber);
                    if (isi != null)
                        hasil = IkonGambar.buat(app.getResources(), isi);
                } catch (Throwable t) {
                    Log.d(TAG, "Ikon gagal dimuat: " + t);
                }
                final Drawable ikon = hasil;
                utama.post(new Runnable() {
                    @Override
                    public void run() {
                        if (ikon != null)
                            pendengar.siap(ikon);
                        else
                            pendengar.gagal();
                    }
                });
            }
        }, "IkonCache").start();
    }

    private static byte[] ambil(Context app, String sumber) {
        byte[] dariMemori = ambilMemori(sumber);
        if (dariMemori != null)
            return dariMemori;

        byte[] isi;
        if (sumber.startsWith("http://") || sumber.startsWith("https://"))
            isi = ambilUrl(app, sumber);
        else if (sumber.startsWith("data:"))
            isi = dariDataUri(sumber);
        else
            isi = baca(new File(sumber.startsWith("file://") ? sumber.substring(7) : sumber));

        if (isi != null && bentukGambar(isi))
            simpanMemori(sumber, isi);
        else
            isi = null;
        return isi;
    }

    private static byte[] ambilUrl(Context app, String url) {
        File berkas = berkasUntuk(app, url);

        if (berkas.isFile() && berkas.length() > 0) {
            byte[] isi = baca(berkas);
            if (isi != null && bentukGambar(isi)) {
                if (System.currentTimeMillis() - berkas.lastModified() > UMUR_SEGAR_MS)
                    segarkanDiam(app, url, berkas);
                return isi;
            }
            // Berkas rusak: dibuang, lalu diunduh ulang.
            berkas.delete();
        }

        byte[] isi = unduh(url);
        if (isi == null || !bentukGambar(isi))
            return null;
        tulis(app, berkas, isi);
        return isi;
    }

    private static byte[] dariDataUri(String uri) {
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
    private static void segarkanDiam(final Context app, final String url, final File berkas) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                byte[] isi = unduh(url);
                if (isi == null || !bentukGambar(isi)) {
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
        if (!berkas.isFile() || berkas.length() > BATAS_BYTE)
            return null;
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

    /** Memeriksa tanda awal berkas supaya halaman galat HTML tidak ikut disimpan. */
    private static boolean bentukGambar(byte[] b) {
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

    private static byte[] ambilMemori(String sumber) {
        synchronized (kunci) {
            return sumber.equals(sumberMemori) ? isiMemori : null;
        }
    }

    private static void simpanMemori(String sumber, byte[] isi) {
        synchronized (kunci) {
            sumberMemori = sumber;
            isiMemori = isi;
        }
    }
}
