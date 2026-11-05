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
public final class IkonGambar {
    private static final int MAKS_PX = 256;

    private IkonGambar() {
    }

    /** Null bila bukan gambar yang bisa didekode. Aman dipanggil dari thread latar. */
    public static Drawable buat(Resources res, byte[] isi) {
        if (isi == null || isi.length == 0)
            return null;
        try {
            if (Build.VERSION.SDK_INT >= 28)
                return dekode28(isi);
            return dekodeLama(res, isi);
        } catch (Throwable t) {
            return null;
        }
    }

    /** Movie hanya benar bila digambar lewat software layer. */
    public static boolean butuhSoftware(Drawable d) {
        return d instanceof GifLama;
    }

    public static void jalankan(Drawable d, boolean jalan) {
        if (!(d instanceof Animatable))
            return;
        Animatable anim = (Animatable) d;
        if (jalan && !anim.isRunning())
            anim.start();
        else if (!jalan && anim.isRunning())
            anim.stop();
    }

    @TargetApi(28)
    private static Drawable dekode28(byte[] isi) throws IOException {
        ImageDecoder.Source sumber = ImageDecoder.createSource(ByteBuffer.wrap(isi));
        Drawable d = ImageDecoder.decodeDrawable(sumber, new ImageDecoder.OnHeaderDecodedListener() {
            @Override
            public void onHeaderDecoded(ImageDecoder decoder, ImageDecoder.ImageInfo info,
                                        ImageDecoder.Source source) {
                int w = info.getSize().getWidth();
                int h = info.getSize().getHeight();
                int terpanjang = Math.max(w, h);
                if (terpanjang > MAKS_PX) {
                    float skala = (float) MAKS_PX / terpanjang;
                    decoder.setTargetSize(Math.max(1, Math.round(w * skala)),
                            Math.max(1, Math.round(h * skala)));
                }
            }
        });
        if (d instanceof AnimatedImageDrawable)
            ((AnimatedImageDrawable) d).setRepeatCount(AnimatedImageDrawable.REPEAT_INFINITE);
        return d;
    }

    private static Drawable dekodeLama(Resources res, byte[] isi) {
        boolean gif = isi.length >= 4 && isi[0] == 'G' && isi[1] == 'I' && isi[2] == 'F';
        if (gif) {
            Movie movie = Movie.decodeByteArray(isi, 0, isi.length);
            if (movie != null && movie.duration() > 0 && movie.width() > 0 && movie.height() > 0)
                return new GifLama(movie);
        }

        BitmapFactory.Options batas = new BitmapFactory.Options();
        batas.inJustDecodeBounds = true;
        BitmapFactory.decodeByteArray(isi, 0, isi.length, batas);
        int terpanjang = Math.max(batas.outWidth, batas.outHeight);
        if (terpanjang <= 0)
            return null;

        int sampel = 1;
        while (terpanjang / (sampel * 2) >= MAKS_PX)
            sampel *= 2;
        BitmapFactory.Options opsi = new BitmapFactory.Options();
        opsi.inSampleSize = sampel;
        Bitmap bitmap = BitmapFactory.decodeByteArray(isi, 0, isi.length, opsi);
        return bitmap == null ? null : new BitmapDrawable(res, bitmap);
    }

    /** GIF bergerak untuk Android 8 ke bawah. Hanya berjalan selama start() aktif. */
    private static final class GifLama extends Drawable implements Animatable {
        private final Movie movie;
        private final int durasi;
        private boolean jalan;
        private long mulai;

        GifLama(Movie movie) {
            this.movie = movie;
            this.durasi = Math.max(1, movie.duration());
        }

        @Override
        public void draw(Canvas canvas) {
            Rect b = getBounds();
            if (b.isEmpty())
                return;
            int t = jalan ? (int) ((SystemClock.uptimeMillis() - mulai) % durasi) : 0;
            movie.setTime(t);

            float skala = Math.min((float) b.width() / movie.width(),
                    (float) b.height() / movie.height());
            float dx = b.left + (b.width() - movie.width() * skala) / 2f;
            float dy = b.top + (b.height() - movie.height() * skala) / 2f;
            canvas.save();
            canvas.translate(dx, dy);
            canvas.scale(skala, skala);
            movie.draw(canvas, 0, 0);
            canvas.restore();

            if (jalan)
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
            mulai = SystemClock.uptimeMillis();
            jalan = true;
            invalidateSelf();
        }

        @Override
        public void stop() {
            jalan = false;
        }

        @Override
        public boolean isRunning() {
            return jalan;
        }
    }
}
