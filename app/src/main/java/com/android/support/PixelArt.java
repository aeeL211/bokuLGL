package com.android.support;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;

import java.util.IdentityHashMap;
import java.util.Map;

/**
 * Pixel-art pattern renderer. A pattern is decoded once into a small Bitmap
 * (16x16), then drawn with a single unfiltered scaled drawBitmap, so the edges
 * stay sharp and onDraw no longer calls drawRect 256 times.
 *
 * Feature default patterns use the three theme tones and are kept in a static
 * cache. The number of patterns is limited (about 35, 1 KB each), so the cache
 * never needs clearing.
 */
final class PixelArt {
    /** Number of icon grid columns. */
    static final int COLUMNS = 16;

    private static final char TONE_BRIGHT = '1';
    private static final char TONE_MEDIUM = '2';
    private static final char TONE_DARK = '3';

    private static final Map<String[], Bitmap> cache = new IdentityHashMap<String[], Bitmap>();

    private PixelArt() {
    }

    /** Pattern bitmap in the feature icon palette. Null when the pattern is empty. */
    static synchronized Bitmap defaultPattern(String[] pattern) {
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

    /** Decodes a pattern into a bitmap with three tone colors. '.' pixels are left transparent. */
    static Bitmap create(String[] pattern, int bright, int current, int dark) {
        if (pattern == null || pattern.length == 0)
            return null;

        int height = pattern.length;
        int width = 0;
        for (String row : pattern)
            width = Math.max(width, row.length());
        if (width == 0)
            return null;

        int[] pixels = new int[width * height];
        for (int y = 0; y < height; y++) {
            String row = pattern[y];
            for (int x = 0; x < row.length(); x++) {
                char content = row.charAt(x);
                if (content == TONE_BRIGHT)
                    pixels[y * width + x] = bright;
                else if (content == TONE_MEDIUM)
                    pixels[y * width + x] = current;
                else if (content == TONE_DARK)
                    pixels[y * width + x] = dark;
            }
        }

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.setPixels(pixels, 0, width, 0, 0, width, height);
        return bitmap;
    }

    /** Paint for drawing patterns: no antialias and no filter, so pixels stay square. */
    static Paint crispPaint() {
        Paint paint = new Paint();
        paint.setAntiAlias(false);
        paint.setFilterBitmap(false);
        paint.setDither(false);
        return paint;
    }

    /**
     * Size of one cell in screen pixels, always an integer so icon edges stay
     * sharp. The pattern uses {@code fraction} of the area's shortest side.
     */
    static int cell(int width, int height, float fraction) {
        int cell = (int) Math.floor(Math.min(width, height) * fraction / COLUMNS);
        return Math.max(1, cell);
    }

    /** Draws the pattern bitmap centered in the area (0,0)-(width,height) with integer cells. */
    static void image(Canvas canvas, Bitmap bitmap, Paint paint, Rect target,
                       int width, int height, int cell) {
        if (bitmap == null)
            return;
        int w = bitmap.getWidth() * cell;
        int h = bitmap.getHeight() * cell;
        int left = Math.round((width - w) / 2f);
        int top = Math.round((height - h) / 2f);
        target.set(left, top, left + w, top + h);
        canvas.drawBitmap(bitmap, null, target, paint);
    }
}
