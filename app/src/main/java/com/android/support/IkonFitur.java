package com.android.support;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

/**
 * Kotak ikon fitur: pola pixel-art dari {@link PolaIkon} di atas kotak cokelat.
 *
 * Kotak ikon menempel penuh di tepi kiri baris (tanpa margin dan tanpa garis),
 * persis seperti kotak ikon di menu game. Hanya sudut kiri yang melengkung,
 * dengan radius yang sama seperti baris.
 *
 * Semua objek gambar dibuat sekali di konstruktor atau onSizeChanged, jadi
 * onDraw tidak mengalokasikan apa pun.
 */
public class IkonFitur extends View {
    /** Lebar kotak ikon dalam dp. Tingginya mengikuti tinggi baris. */
    public static final int LEBAR_DP = 48;

    /** Bagian sisi terpendek kotak yang dipakai pola, seperti di game. */
    private static final float FRAKSI_POLA = 0.70f;

    private final Paint catKotak = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catPola = Piksel.catTajam();
    private final Path kotak = new Path();
    private final RectF area = new RectF();
    private final Rect tujuan = new Rect();
    private final float[] sudut = new float[8];
    private final Bitmap bitmap;
    private int sel = 1;

    public IkonFitur(Context context, String[] pola, boolean sudutKiri) {
        super(context);
        bitmap = Piksel.standar(pola);
        catKotak.setStyle(Paint.Style.FILL);

        // Urutan sudut: kiri-atas, kanan-atas, kanan-bawah, kiri-bawah.
        float radius = Theme.dp(context, Theme.RADIUS_ROW);
        float kanan = sudutKiri ? 0f : radius;
        sudut[0] = radius;
        sudut[1] = radius;
        sudut[2] = kanan;
        sudut[3] = kanan;
        sudut[4] = kanan;
        sudut[5] = kanan;
        sudut[6] = radius;
        sudut[7] = radius;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int sisi = (int) Theme.dp(getContext(), LEBAR_DP);
        setMeasuredDimension(resolveSize(sisi, widthMeasureSpec),
                resolveSize(sisi, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        area.set(0f, 0f, w, h);
        kotak.reset();
        kotak.addRoundRect(area, sudut, Path.Direction.CW);
        sel = Piksel.sel(w, h, FRAKSI_POLA);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        // Hanya view yang bisa diketuk atau berada di baris yang bisa diketuk yang berstatus ditekan.
        catKotak.setColor(isPressed() ? Theme.TRACK : Theme.ICON_BOX);
        canvas.drawPath(kotak, catKotak);
        Piksel.gambar(canvas, bitmap, catPola, tujuan, getWidth(), getHeight(), sel);
    }
}
