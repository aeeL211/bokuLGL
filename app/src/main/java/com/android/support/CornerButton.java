package com.android.support;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

/**
 * Game-style corner button: black box, white line, white pixel glyph.
 *
 * The drawing is 34dp as before, but the view is {@link Theme#TARGET_DP}
 * (48dp) so the touch area meets the minimum size. The glyph is drawn from
 * a 16x16 pixel grid through {@link PixelArt}, just like the feature icons.
 */
final class CornerButton extends View {
    private static final float BOX_DP = 34f;
    private static final float LINE_DP = 2f;
    private static final float FRACTION_GLYPH = 0.70f;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint glyphPaint = PixelArt.crispPaint();
    private final RectF box = new RectF();
    private final RectF line = new RectF();
    private final Rect target = new Rect();
    private final Bitmap glyph;
    private final float boxPx;
    private final float linePx;
    private final float radiusPx;
    private int cell = 1;

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
        int side = (int) Theme.dp(getContext(), Theme.TARGET_DP);
        setMeasuredDimension(resolveSize(side, widthMeasureSpec),
                resolveSize(side, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float side = Math.min(boxPx, Math.min(w, h));
        float left = (w - side) / 2f;
        float top = (h - side) / 2f;
        box.set(left, top, left + side, top + side);
        float enter = linePx / 2f;
        line.set(box.left + enter, box.top + enter, box.right - enter, box.bottom - enter);
        cell = PixelArt.cell((int) side, (int) side, FRACTION_GLYPH);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(isPressed() ? Theme.BOX_CORNER_PRESS : Theme.BOX_CORNER);
        canvas.drawRoundRect(box, radiusPx, radiusPx, paint);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(linePx);
        paint.setColor(Theme.ACTIVE_BORDER);
        canvas.drawRoundRect(line, radiusPx, radiusPx, paint);

        PixelArt.image(canvas, glyph, glyphPaint, target, getWidth(), getHeight(), cell);
    }
}
