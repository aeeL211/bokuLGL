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
final class PixelArt {
    /** Jumlah kolom grid ikon. */
    static final int COLUMNS = 16;

    private static final char TONE_BRIGHT = '1';
    private static final char TONE_MEDIUM = '2';
    private static final char TONE_DARK = '3';

    private static final Map<String[], Bitmap> cache = new IdentityHashMap<String[], Bitmap>();

    private PixelArt() {
    }

    /** Bitmap pola dengan palet ikon fitur. Null bila pola kosong. */
    static synchronized Bitmap standar(String[] pattern) {
        if (pattern == null)
            return null;
        Bitmap bitmap = cache.get(pattern);
        if (bitmap == null) {
            bitmap = create(pattern, Theme.ICON_LIGHT, Theme.ICON_MID, Theme.ICON_DARK);
            if (bitmap != null)
                cache.put(pattern, bitmap);
        }
        return bitmap;
    }

    /** Mendekode pola menjadi bitmap dengan tiga warna nada. Piksel '.' dibiarkan transparan. */
    static Bitmap create(String[] pattern, int bright, int current, int dark) {
        if (pattern == null || pattern.length == 0)
            return null;

        int height = pattern.length;
        int width = 0;
        for (String row : pattern)
            width = Math.max(width, row.length());
        if (width == 0)
            return null;

        int[] piksel = new int[width * height];
        for (int y = 0; y < height; y++) {
            String row = pattern[y];
            for (int x = 0; x < row.length(); x++) {
                char content = row.charAt(x);
                if (content == TONE_BRIGHT)
                    piksel[y * width + x] = bright;
                else if (content == TONE_MEDIUM)
                    piksel[y * width + x] = current;
                else if (content == TONE_DARK)
                    piksel[y * width + x] = dark;
            }
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.setPixels(piksel, 0, width, 0, 0, width, height);
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
    static int sel(int width, int height, float fraction) {
        int sel = (int) Math.floor(Math.min(width, height) * fraction / COLUMNS);
        return Math.max(1, sel);
    }

    /** Menggambar bitmap pola di tengah area (0,0)-(lebar,tinggi) dengan sel bilangan bulat. */
    static void image(Canvas canvas, Bitmap bitmap, Paint cat, Rect target,
                       int width, int height, int sel) {
        if (bitmap == null)
            return;
        int w = bitmap.getWidth() * sel;
        int h = bitmap.getHeight() * sel;
        int left = Math.round((width - w) / 2f);
        int top = Math.round((height - h) / 2f);
        target.set(left, top, left + w, top + h);
        canvas.drawBitmap(bitmap, null, target, cat);
    }
}
