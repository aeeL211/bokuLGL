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
 * Feature icon box: pixel-art pattern from {@link IconPattern} on a brown box.
 *
 * The icon box is flush with the left edge of the row (no margin and no line),
 * exactly like the icon box in the game menu. Only the left corner is rounded,
 * with the same radius as the row.
 *
 * All drawing objects are created once in the constructor or onSizeChanged, so
 * onDraw allocates nothing.
 */
public class FeatureIcon extends View {
    /** Icon box width in dp. The height follows the row height. */
    public static final int WIDTH_DP = 48;

    /** Fraction of the box's shortest side used by the pattern, like in the game. */
    private static final float FRACTION_PATTERN = 0.70f;

    private final Paint catBox = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint patternPaint = PixelArt.crispPaint();
    private final Path box = new Path();
    private final RectF area = new RectF();
    private final Rect target = new Rect();
    private final float[] corner = new float[8];
    private final Bitmap bitmap;
    private int cell = 1;

    public FeatureIcon(Context context, String[] pattern, boolean cornerLeft) {
        super(context);
        bitmap = PixelArt.defaultPattern(pattern);
        catBox.setStyle(Paint.Style.FILL);

        // Corner order: top-left, top-right, bottom-right, bottom-left.
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
        int side = (int) Theme.dp(getContext(), WIDTH_DP);
        setMeasuredDimension(resolveSize(side, widthMeasureSpec),
                resolveSize(side, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        area.set(0f, 0f, w, h);
        box.reset();
        box.addRoundRect(area, corner, Path.Direction.CW);
        cell = PixelArt.cell(w, h, FRACTION_PATTERN);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        // Only a view that is clickable or inside a clickable row gets the pressed state.
        catBox.setColor(isPressed() ? Theme.TRACK : Theme.ICON_BOX);
        canvas.drawPath(box, catBox);
        PixelArt.image(canvas, bitmap, patternPaint, target, getWidth(), getHeight(), cell);
    }
}
