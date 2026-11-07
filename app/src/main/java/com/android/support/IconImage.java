package com.android.support;

import android.annotation.TargetApi;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Movie;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.ImageDecoder;
import android.os.Build;
import android.os.SystemClock;

import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Pengganti WebView untuk ikon melayang. Hanya memakai decoder bawaan Android,
 * jadi tidak ada proses renderer Chromium, tidak ada HTML, dan memori jauh lebih kecil.
 *
 * Android 9 ke atas: ImageDecoder. GIF dan WebP bergerak menjadi
 * AnimatedImageDrawable yang didekode di thread sendiri milik sistem.
 * Android 8 ke bawah: GIF memakai android.graphics.Movie, gambar lain memakai
 * BitmapFactory.
 *
 * Gambar selalu didekode dengan sisi terpanjang paling besar {@link #MAKS_PX},
 * jauh di atas ukuran ikon di layar, supaya GIF besar tidak memakan memori.
 */
public final class IconImage {
    private static final int MAX_PX = 256;

    private IconImage() {
    }

    /** Null bila bukan gambar yang bisa didekode. Aman dipanggil dari thread latar. */
    public static Drawable create(Resources res, byte[] content) {
        if (content == null || content.length == 0)
            return null;
        try {
            if (Build.VERSION.SDK_INT >= 28)
                return dekode28(content);
            return decodeOld(res, content);
        } catch (Throwable t) {
            return null;
        }
    }

    /** Movie hanya benar bila digambar lewat software layer. */
    public static boolean needSoftware(Drawable d) {
        return d instanceof GifOld;
    }

    public static void jalankan(Drawable d, boolean run) {
        if (!(d instanceof Animatable))
            return;
        Animatable anim = (Animatable) d;
        if (run && !anim.isRunning())
            anim.start();
        else if (!run && anim.isRunning())
            anim.stop();
    }

    @TargetApi(28)
    private static Drawable dekode28(byte[] content) throws IOException {
        ImageDecoder.Source source = ImageDecoder.createSource(ByteBuffer.wrap(content));
        Drawable d = ImageDecoder.decodeDrawable(source, new ImageDecoder.OnHeaderDecodedListener() {
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source source) {
                int w = info.getSize().getWidth();
                int h = info.getSize().getHeight();
                int terpanjang = Math.max(w, h);
                if (terpanjang > MAX_PX) {
                    float scale = (float) MAX_PX / terpanjang;
                    decoder.setTargetSize(Math.max(1, Math.round(w * scale)),
                            Math.max(1, Math.round(h * scale)));
                }
            }
        });
        if (d instanceof AnimatedImageDrawable)
            ((AnimatedImageDrawable) d).setRepeatCount(AnimatedImageDrawable.REPEAT_INFINITE);
        return d;
    }

    private static Drawable decodeOld(Resources res, byte[] content) {
        boolean gif = content.length >= 4 && content[0] == 'G' && content[1] == 'I' && content[2] == 'F';
        if (gif) {
            Movie movie = Movie.decodeByteArray(content, 0, content.length);
            if (movie != null && movie.duration() > 0 && movie.width() > 0 && movie.height() > 0)
                return new GifOld(movie);
        }

        BitmapFactory.Options limit = new BitmapFactory.Options();
        limit.inJustDecodeBounds = true;
        BitmapFactory.decodeByteArray(content, 0, content.length, limit);
        int terpanjang = Math.max(limit.outWidth, limit.outHeight);
        if (terpanjang <= 0)
            return null;

        int sample = 1;
        while (terpanjang / (sample * 2) >= MAX_PX)
            sample *= 2;
        BitmapFactory.Options opsi = new BitmapFactory.Options();
        opsi.inSampleSize = sample;
        Bitmap bitmap = BitmapFactory.decodeByteArray(content, 0, content.length, opsi);
        return bitmap == null ? null : new BitmapDrawable(res, bitmap);
    }

    /** GIF bergerak untuk Android 8 ke bawah. Hanya berjalan selama start() aktif. */
    private static final class GifOld extends Drawable implements Animatable {
        private final Movie movie;
        private final int duration;
        private boolean run;
        private long start;

        GifOld(Movie movie) {
            this.movie = movie;
            this.duration = Math.max(1, movie.duration());
        }

        @Override
        public void draw(Canvas canvas) {
            Rect b = getBounds();
            if (b.isEmpty())
                return;
            int t = run ? (int) ((SystemClock.uptimeMillis() - start) % duration) : 0;
            movie.setTime(t);

            float scale = Math.min((float) b.width() / movie.width(),
                    (float) b.height() / movie.height());
            float dx = b.left + (b.width() - movie.width() * scale) / 2f;
            float dy = b.top + (b.height() - movie.height() * scale) / 2f;
            canvas.save();
            canvas.translate(dx, dy);
            canvas.scale(scale, scale);
            movie.draw(canvas, 0, 0);
            canvas.restore();

            if (run)
                invalidateSelf();
        }

        @Override
        public int getIntrinsicWidth() {
            return movie.width();
        }

        @Override
        public int getIntrinsicHeight() {
            return movie.height();
        }

        @Override
        public void setAlpha(int alpha) {
        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {
        }

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }

        @Override
        public void start() {
            start = SystemClock.uptimeMillis();
            run = true;
            invalidateSelf();
        }

        @Override
        public void stop() {
            run = false;
        }

        @Override
        public boolean isRunning() {
            return run;
        }
    }
}
