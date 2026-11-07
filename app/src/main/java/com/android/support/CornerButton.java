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
final class CornerButton extends View {
    private static final float BOX_DP = 34f;
    private static final float LINE_DP = 2f;
    private static final float FRACTION_GLYPH = 0.70f;

    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catGlyph = PixelArt.catTajam();
    private final RectF box = new RectF();
    private final RectF line = new RectF();
    private final Rect target = new Rect();
    private final Bitmap glyph;
    private final float boxPx;
    private final float linePx;
    private final float radiusPx;
    private int sel = 1;

    CornerButton(Context context, String[] pattern) {
        super(context);
        glyph = PixelArt.create(pattern, Theme.ACTIVE_BORDER, Theme.ACTIVE_BORDER, Theme.ACTIVE_BORDER);
        boxPx = Theme.dp(context, BOX_DP);
        linePx = Theme.dp(context, LINE_DP);
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
        float sisi = Math.min(boxPx, Math.min(w, h));
        float left = (w - sisi) / 2f;
        float top = (h - sisi) / 2f;
        box.set(left, top, left + sisi, top + sisi);
        float enter = linePx / 2f;
        line.set(box.left + enter, box.top + enter, box.right - enter, box.bottom - enter);
        sel = PixelArt.sel((int) sisi, (int) sisi, FRACTION_GLYPH);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        cat.setStyle(Paint.Style.FILL);
        cat.setColor(isPressed() ? Theme.BOX_CORNER_PRESS : Theme.BOX_CORNER);
        canvas.drawRoundRect(box, radiusPx, radiusPx, cat);

        cat.setStyle(Paint.Style.STROKE);
        cat.setStrokeWidth(linePx);
        cat.setColor(Theme.ACTIVE_BORDER);
        canvas.drawRoundRect(line, radiusPx, radiusPx, cat);

        PixelArt.image(canvas, glyph, catGlyph, target, getWidth(), getHeight(), sel);
    }
}
