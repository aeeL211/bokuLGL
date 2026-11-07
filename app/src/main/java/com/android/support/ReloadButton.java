package com.android.support;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;

/**
 * Icon-only reload button for the player list. The state is shown by the glyph
 * and the box alone:
 * <ul>
 * <li>idle: the glyph sits on a dark wood box;</li>
 * <li>loading: the glyph rotates in 90 degree steps. Whole multiples of 90
 * degrees keep the pixels sharp, and the glyph is symmetric under a 180 degree
 * rotation so there is no jump;</li>
 * <li>done: the box turns bright like an active switch and the glyph darkens,
 * then it returns to idle.</li>
 * </ul>
 * The box is {@link #HEIGHT_DP} tall, while the view is {@link Theme#TARGET_DP}
 * (48dp) so the touch area is big enough. The width is fixed for every state so
 * the header does not shift while the glyph rotates.
 */
final class ReloadButton extends View {
    private static final int IDLE = 0;
    private static final int LOADING = 1;
    private static final int DONE = 2;

    private static final float HEIGHT_DP = 34f;
    private static final float LINE_DP = 2f;
    private static final float PAD_DP = 10f;
    private static final float GLYPH_DP = 20f;

    /** One rotation step (90 degrees) every STEP_MS; 6 steps = one and a half turns. */
    private static final long STEP_MS = 130L;
    private static final int COUNT_STEP = 6;
    private static final long SHOW_DONE_MS = 1000L;

    private final Runnable action;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint glyphPaint = PixelArt.crispPaint();
    private final RectF box = new RectF();
    private final RectF line = new RectF();
    private final Rect target = new Rect();
    private final Bitmap glyphBright;
    private final Bitmap glyphDark;
    private final float heightPx;
    private final float linePx;
    private final float radiusPx;
    private final float padPx;
    private final int cell;
    private final int glyphPx;

    private int state = IDLE;
    private int rotate;

    private final Runnable step = new Runnable() {
        @Override
        public void run() {
            rotate++;
            if (rotate >= COUNT_STEP) {
                change(DONE);
                postDelayed(back, SHOW_DONE_MS);
            } else {
                invalidate();
                postDelayed(this, STEP_MS);
            }
        }
    };

    private final Runnable back = new Runnable() {
        @Override
        public void run() {
            change(IDLE);
        }
    };

    /**
     * @param pattern glyph 16x16 that is symmetric under a 180 degree rotation
     * @param action run on the main thread every time the button is tapped
     */
    ReloadButton(Context context, String[] pattern, Runnable action) {
        super(context);
        this.action = action;
        glyphBright = PixelArt.create(pattern, Theme.TITLE_TEXT, Theme.TITLE_TEXT, Theme.TITLE_TEXT);
        glyphDark = PixelArt.create(pattern, Theme.TEXT, Theme.TEXT, Theme.TEXT);
        heightPx = Theme.dp(context, HEIGHT_DP);
        linePx = Theme.dp(context, LINE_DP);
        radiusPx = Theme.dp(context, Theme.RADIUS_PANEL);
        padPx = Theme.dp(context, PAD_DP);
        cell = Math.max(1, (int) Math.floor(Theme.dp(context, GLYPH_DP) / PixelArt.COLUMNS));
        glyphPx = cell * PixelArt.COLUMNS;

        setClickable(true);
        setFocusable(true);
        setContentDescription("Muat ulang daftar pemain");
        setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                start();
            }
        });
    }

    private void start() {
        removeCallbacks(step);
        removeCallbacks(back);
        if (action != null)
            action.run();
        rotate = 0;
        if (!Transition.active(getContext())) {
            // System animations are off: show the result immediately with no rotation.
            change(DONE);
            postDelayed(back, SHOW_DONE_MS);
            return;
        }
        change(LOADING);
        postDelayed(step, STEP_MS);
    }

    private void change(int fresh) {
        state = fresh;
        if (fresh == IDLE)
            rotate = 0;
        setContentDescription(fresh == LOADING ? "Memuat daftar pemain"
                : fresh == DONE ? "Daftar pemain diperbarui" : "Muat ulang daftar pemain");
        invalidate();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = (int) Math.ceil(padPx * 2 + glyphPx);
        int height = (int) Theme.dp(getContext(), Theme.TARGET_DP);
        setMeasuredDimension(resolveSize(width, widthMeasureSpec),
                resolveSize(height, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float top = (h - heightPx) / 2f;
        box.set(0f, top, w, top + heightPx);
        float enter = linePx / 2f;
        line.set(box.left + enter, box.top + enter, box.right - enter, box.bottom - enter);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        boolean done = state == DONE;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(done ? Theme.SWITCH_ON_TRACK : isPressed() ? Theme.TRACK : Theme.ICON_BOX);
        canvas.drawRoundRect(box, radiusPx, radiusPx, paint);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(linePx);
        paint.setColor(Theme.BUTTON_BORDER);
        canvas.drawRoundRect(line, radiusPx, radiusPx, paint);

        int height = getHeight();

        // Glyph: a glyphPx-sided box centered in the view, rotated around its
        // own center.
        canvas.save();
        canvas.translate(Math.round((getWidth() - glyphPx) / 2f),
                Math.round((height - glyphPx) / 2f));
        if (state == LOADING)
            canvas.rotate(90f * (rotate % 4), glyphPx / 2f, glyphPx / 2f);
        PixelArt.image(canvas, done ? glyphDark : glyphBright, glyphPaint, target,
                glyphPx, glyphPx, cell);
        canvas.restore();
    }

    @Override
    protected void onDetachedFromWindow() {
        removeCallbacks(step);
        removeCallbacks(back);
        // When reattached, do not get stuck mid-animation.
        state = IDLE;
        rotate = 0;
        super.onDetachedFromWindow();
    }
}
