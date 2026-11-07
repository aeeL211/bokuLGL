package com.android.support;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

/**
 * Tombol sudut bergaya game: kotak hitam, garis putih, glyph pixel putih.
 *
 * Gambarnya 34dp seperti sebelumnya, tetapi view-nya {@link Theme#TARGET_DP}
 * (48dp) supaya area sentuhnya memenuhi ukuran minimum. Glyph digambar dari
 * grid pixel 16x16 lewat {@link Piksel}, sama seperti ikon fitur.
 */
final class TombolSudut extends View {
    private static final float KOTAK_DP = 34f;
    private static final float GARIS_DP = 2f;
    private static final float FRAKSI_GLYPH = 0.70f;

    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catGlyph = Piksel.catTajam();
    private final RectF kotak = new RectF();
    private final RectF garis = new RectF();
    private final Rect tujuan = new Rect();
    private final Bitmap glyph;
    private final float kotakPx;
    private final float garisPx;
    private final float radiusPx;
    private int sel = 1;

    TombolSudut(Context context, String[] pola) {
        super(context);
        glyph = Piksel.buat(pola, Theme.ACTIVE_BORDER, Theme.ACTIVE_BORDER, Theme.ACTIVE_BORDER);
        kotakPx = Theme.dp(context, KOTAK_DP);
        garisPx = Theme.dp(context, GARIS_DP);
        radiusPx = Theme.dp(context, Theme.RADIUS_PANEL);
        setClickable(true);
        setFocusable(true);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int sisi = (int) Theme.dp(getContext(), Theme.TARGET_DP);
        setMeasuredDimension(resolveSize(sisi, widthMeasureSpec),
                resolveSize(sisi, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float sisi = Math.min(kotakPx, Math.min(w, h));
        float kiri = (w - sisi) / 2f;
        float atas = (h - sisi) / 2f;
        kotak.set(kiri, atas, kiri + sisi, atas + sisi);
        float masuk = garisPx / 2f;
        garis.set(kotak.left + masuk, kotak.top + masuk, kotak.right - masuk, kotak.bottom - masuk);
        sel = Piksel.sel((int) sisi, (int) sisi, FRAKSI_GLYPH);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        cat.setStyle(Paint.Style.FILL);
        cat.setColor(isPressed() ? Theme.BOX_SUDUT_TEKAN : Theme.BOX_SUDUT);
        canvas.drawRoundRect(kotak, radiusPx, radiusPx, cat);

        cat.setStyle(Paint.Style.STROKE);
        cat.setStrokeWidth(garisPx);
        cat.setColor(Theme.ACTIVE_BORDER);
        canvas.drawRoundRect(garis, radiusPx, radiusPx, cat);

        Piksel.gambar(canvas, glyph, catGlyph, tujuan, getWidth(), getHeight(), sel);
    }
}
