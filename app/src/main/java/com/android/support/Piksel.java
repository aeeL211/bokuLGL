package com.android.support;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;

import java.util.IdentityHashMap;
import java.util.Map;

/**
 * Penggambar pola pixel-art. Satu pola didekode sekali menjadi Bitmap kecil
 * (16x16), lalu digambar dengan satu drawBitmap yang diperbesar tanpa filter,
 * jadi tepinya tetap tajam dan onDraw tidak lagi memanggil drawRect 256 kali.
 *
 * Pola bawaan fitur memakai tiga nada tema dan disimpan di cache statis. Jumlah
 * pola terbatas (sekitar 35, masing-masing 1 KB), jadi cache tidak perlu dibersihkan.
 */
final class Piksel {
    /** Jumlah kolom grid ikon. */
    static final int KOLOM = 16;

    private static final char NADA_TERANG = '1';
    private static final char NADA_SEDANG = '2';
    private static final char NADA_GELAP = '3';

    private static final Map<String[], Bitmap> cache = new IdentityHashMap<String[], Bitmap>();

    private Piksel() {
    }

    /** Bitmap pola dengan palet ikon fitur. Null bila pola kosong. */
    static synchronized Bitmap standar(String[] pola) {
        if (pola == null)
            return null;
        Bitmap bitmap = cache.get(pola);
        if (bitmap == null) {
            bitmap = buat(pola, Theme.ICON_LIGHT, Theme.ICON_MID, Theme.ICON_DARK);
            if (bitmap != null)
                cache.put(pola, bitmap);
        }
        return bitmap;
    }

    /** Mendekode pola menjadi bitmap dengan tiga warna nada. Piksel '.' dibiarkan transparan. */
    static Bitmap buat(String[] pola, int terang, int sedang, int gelap) {
        if (pola == null || pola.length == 0)
            return null;

        int tinggi = pola.length;
        int lebar = 0;
        for (String baris : pola)
            lebar = Math.max(lebar, baris.length());
        if (lebar == 0)
            return null;

        int[] piksel = new int[lebar * tinggi];
        for (int y = 0; y < tinggi; y++) {
            String baris = pola[y];
            for (int x = 0; x < baris.length(); x++) {
                char isi = baris.charAt(x);
                if (isi == NADA_TERANG)
                    piksel[y * lebar + x] = terang;
                else if (isi == NADA_SEDANG)
                    piksel[y * lebar + x] = sedang;
                else if (isi == NADA_GELAP)
                    piksel[y * lebar + x] = gelap;
            }
        }

        Bitmap bitmap = Bitmap.createBitmap(lebar, tinggi, Bitmap.Config.ARGB_8888);
        bitmap.setPixels(piksel, 0, lebar, 0, 0, lebar, tinggi);
        return bitmap;
    }

    /** Paint untuk menggambar pola: tanpa antialias dan tanpa filter, supaya piksel tetap kotak. */
    static Paint catTajam() {
        Paint cat = new Paint();
        cat.setAntiAlias(false);
        cat.setFilterBitmap(false);
        cat.setDither(false);
        return cat;
    }

    /**
     * Ukuran satu sel dalam piksel layar, selalu bilangan bulat supaya tepi ikon
     * tajam. Pola memakai {@code fraksi} dari sisi terpendek area.
     */
    static int sel(int lebar, int tinggi, float fraksi) {
        int sel = (int) Math.floor(Math.min(lebar, tinggi) * fraksi / KOLOM);
        return Math.max(1, sel);
    }

    /** Menggambar bitmap pola di tengah area (0,0)-(lebar,tinggi) dengan sel bilangan bulat. */
    static void gambar(Canvas canvas, Bitmap bitmap, Paint cat, Rect tujuan,
                       int lebar, int tinggi, int sel) {
        if (bitmap == null)
            return;
        int w = bitmap.getWidth() * sel;
        int h = bitmap.getHeight() * sel;
        int kiri = Math.round((lebar - w) / 2f);
        int atas = Math.round((tinggi - h) / 2f);
        tujuan.set(kiri, atas, kiri + w, atas + h);
        canvas.drawBitmap(bitmap, null, tujuan, cat);
    }
}
