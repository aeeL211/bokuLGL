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
public class FeatureIcon extends View {
    /** Lebar kotak ikon dalam dp. Tingginya mengikuti tinggi baris. */
    public static final int WIDTH_DP = 48;

    /** Bagian sisi terpendek kotak yang dipakai pola, seperti di game. */
    private static final float FRACTION_PATTERN = 0.70f;

    private final Paint catBox = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catPattern = PixelArt.catTajam();
    private final Path box = new Path();
    private final RectF area = new RectF();
    private final Rect target = new Rect();
    private final float[] corner = new float[8];
    private final Bitmap bitmap;
    private int sel = 1;

    public FeatureIcon(Context context, String[] pattern, boolean cornerLeft) {
        super(context);
        bitmap = PixelArt.standar(pattern);
        catBox.setStyle(Paint.Style.FILL);

        // Urutan sudut: kiri-atas, kanan-atas, kanan-bawah, kiri-bawah.
        float radius = Theme.dp(context, Theme.RADIUS_ROW);
        float right = cornerLeft ? 0f : radius;
        corner[0] = radius;
        corner[1] = radius;
        corner[2] = right;
        corner[3] = right;
        corner[4] = right;
        corner[5] = right;
        corner[6] = radius;
        corner[7] = radius;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int sisi = (int) Theme.dp(getContext(), WIDTH_DP);
        setMeasuredDimension(resolveSize(sisi, widthMeasureSpec),
                resolveSize(sisi, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        area.set(0f, 0f, w, h);
        box.reset();
        box.addRoundRect(area, corner, Path.Direction.CW);
        sel = PixelArt.sel(w, h, FRACTION_PATTERN);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        // Hanya view yang bisa diketuk atau berada di baris yang bisa diketuk yang berstatus ditekan.
        catBox.setColor(isPressed() ? Theme.TRACK : Theme.ICON_BOX);
        canvas.drawPath(box, catBox);
        PixelArt.image(canvas, bitmap, catPattern, target, getWidth(), getHeight(), sel);
    }
}
