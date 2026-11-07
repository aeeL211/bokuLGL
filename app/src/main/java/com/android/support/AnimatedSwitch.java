package com.android.support;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.OvershootInterpolator;
import android.widget.CompoundButton;

/**
 * Game-style pill switch: 52x26dp pill, 3dp #B28950 stroke, 20dp round knob
 * that fills the inside of the pill.
 *
 * OFF : #B28950 track, #CEA972 knob on the left.
 * ON  : #F8B551 track, #FFF799 knob on the right.
 *
 * Still a CompoundButton subclass so accessibility and click handling match
 * the stock Switch. The view is 48dp tall so the touch area meets the minimum
 * size; the pill itself is drawn in the center.
 *
 * The animation is driven from onDraw, not ValueAnimator, for three reasons
 * that made the previous animation sometimes invisible:
 *
 * 1. ValueAnimator follows the system "animator duration scale". With battery
 *    saver or a developer option set to 0, the animation finishes instantly.
 * 2. The change listener is called before the animation starts. If the work in
 *    the listener (JNI to native) blocks the main thread, the first frame
 *    arrives late and ValueAnimator jumps to the end. Now the animation starts
 *    first and the listener is called a few frames later.
 * 3. Unplugging the window mid-animation left the knob halfway. Now the
 *    position is always returned to the real state.
 *
 * Progress per frame is capped at {@link #FRAME_LIMIT_MS}, so even when one
 * frame stalls, the knob still moves in steps instead of jumping.
 */
public class AnimatedSwitch extends CompoundButton {
    private static final float DURATION = 190f;
    private static final float DURATION_PRESS = 90f;
    private static final float LIMIT_FRAME_MS = 20f;
    private static final long DELAY_NOTIFY_MS = 50L;

    private static final float PILL_WIDTH_DP = 52f;
    private static final float PILL_HEIGHT_DP = 26f;
    private static final float LINE_DP = 3f;
    private static final float PRESS_STRETCH_DP = 4f;
    private static final float TARGET_HEIGHT_DP = Theme.TARGET_DP;

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF rect = new RectF();
    private final OvershootInterpolator bounce = new OvershootInterpolator(0.9f);
    private final DecelerateInterpolator brake = new DecelerateInterpolator();

    // position 0 = off, 1 = on (may slightly overshoot during the bounce).
    private float position;
    // press 0..1 = the knob stretches while a finger touches.
    private float press;
    private float pressTarget;

    private boolean drag;
    private float dragInitial;
    private float dragTarget;
    private float dragProgress;
    private float dragDuration;
    private boolean dragBounce;

    private boolean running;
    private long lastMs;

    // Pixel sizes are computed once in the constructor, not every frame.
    private final float pillWidthPx;
    private final float pillHeightPx;
    private final float linePx;
    private final float stretchPx;
    private final int targetHeightPx;

    private OnCheckedChangeListener listener;
    private boolean lastNotified;

    private final Runnable notifyChange = new Runnable() {
        @Override
        public void run() {
            boolean now = isChecked();
            if (now == lastNotified)
                return;
            lastNotified = now;
            if (listener != null)
                listener.onCheckedChanged(AnimatedSwitch.this, now);
        }
    };

    public AnimatedSwitch(Context context) {
        super(context);
        pillWidthPx = Theme.dp(context, PILL_WIDTH_DP);
        pillHeightPx = Theme.dp(context, PILL_HEIGHT_DP);
        linePx = Theme.dp(context, LINE_DP);
        stretchPx = Theme.dp(context, PRESS_STRETCH_DP);
        targetHeightPx = (int) Theme.dp(context, TARGET_HEIGHT_DP);
        setBackground(null);
        setButtonDrawable(null);
        setPadding(0, 0, 0, 0);
        setClickable(true);
        setFocusable(true);
        position = isChecked() ? 1f : 0f;
    }

    /**
     * The listener is not passed on to CompoundButton so it is not called in
     * the middle of setChecked, before the animation starts.
     */
    @Override
    public void setOnCheckedChangeListener(OnCheckedChangeListener listener) {
        this.listener = listener;
        lastNotified = isChecked();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(resolveSize((int) pillWidthPx, widthMeasureSpec),
                resolveSize(targetHeightPx, heightMeasureSpec));
    }

    /**
     * Values from restore are set without animation and without notifying,
     * because the view has no size yet when Preferences.loadPrefBool is called.
     * Taps after layout always animate from the current position, so a quick tap
     * can be reversed without jumping.
     */
    @Override
    public void setChecked(boolean checked) {
        super.setChecked(checked);

        float target = checked ? 1f : 0f;
        boolean visible = isLaidOut() && isShown();
        if (!visible) {
            stopAnimation();
            position = target;
            invalidate();
            return;
        }

        if (drag ? dragTarget != target : position != target)
            startDrag(target);

        // Notification is delayed a few frames so the knob is already moving
        // before the heavy work in the listener. Rapid taps are coalesced: only
        // the final state is reported, and only if it differs from the last
        // report.
        removeCallbacks(notifyChange);
        postDelayed(notifyChange, DELAY_NOTIFY_MS);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        pressTarget = isPressed() ? 1f : 0f;
        if (isLaidOut() && press != pressTarget)
            startFrame();
        invalidate();
    }

    private void startDrag(float target) {
        float distance = Math.abs(target - position);
        dragInitial = position;
        dragTarget = target;
        dragProgress = 0f;
        dragDuration = DURATION * Math.max(0.45f, distance);
        // A long move bounces a little at the end like a physical switch; a
        // short move (reversing midway) only decelerates.
        dragBounce = distance > 0.6f;
        drag = true;
        startFrame();
    }

    private void startFrame() {
        if (!running) {
            running = true;
            lastMs = SystemClock.uptimeMillis();
        }
        postInvalidateOnAnimation();
    }

    /** Advances the animation one frame. True when another frame is still needed. */
    private boolean step() {
        long now = SystemClock.uptimeMillis();
        float dt = Math.min((float) (now - lastMs), LIMIT_FRAME_MS);
        lastMs = now;
        boolean needsFrame = false;

        if (drag) {
            dragProgress += dt / dragDuration;
            if (dragProgress >= 1f) {
                position = dragTarget;
                drag = false;
            } else {
                float t = dragBounce
                        ? bounce.getInterpolation(dragProgress)
                        : brake.getInterpolation(dragProgress);
                position = dragInitial + (dragTarget - dragInitial) * t;
                needsFrame = true;
            }
        }

        if (press != pressTarget) {
            float step = dt / DURATION_PRESS;
            press = press < pressTarget
                    ? Math.min(pressTarget, press + step)
                    : Math.max(pressTarget, press - step);
            if (press != pressTarget)
                needsFrame = true;
        }
        return needsFrame;
    }

    /** Stops the animation and resets the drawing to the real state. */
    private void stopAnimation() {
        drag = false;
        running = false;
        press = 0f;
        pressTarget = 0f;
        position = isChecked() ? 1f : 0f;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        stopAnimation();
    }

    @Override
    protected void onDetachedFromWindow() {
        // A pending notification is delivered now so it is not lost, and the
        // knob is not left halfway.
        removeCallbacks(notifyChange);
        notifyChange.run();
        stopAnimation();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        boolean needsFrame = running && step();
        if (needsFrame)
            postInvalidateOnAnimation();
        else
            running = false;

        float width = getWidth();
        float height = getHeight();
        float pillWidth = Math.min(width, pillWidthPx);
        float pillHeight = pillHeightPx;
        float line = linePx;

        float left = (width - pillWidth) / 2f;
        float top = (height - pillHeight) / 2f;
        float radius = pillHeight / 2f;

        // Colors follow the position but are clamped so the overshoot does not
        // push them out of range. Theme.mix allocates no new object.
        float f = Math.max(0f, Math.min(1f, position));
        int trackColor = Theme.mix(Theme.SWITCH_OFF_TRACK, Theme.SWITCH_ON_TRACK, f);
        int knobColor = Theme.mix(Theme.SWITCH_OFF_KNOB, Theme.SWITCH_ON_KNOB, f);

        // Outer #B28950 line as a full pill, then the track inside. When off the
        // track is the same color as the line, exactly like the game.
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Theme.STROKE_CONTROL);
        rect.set(left, top, left + pillWidth, top + pillHeight);
        canvas.drawRoundRect(rect, radius, radius, paint);

        paint.setColor(trackColor);
        rect.set(left + line, top + line, left + pillWidth - line, top + pillHeight - line);
        float radiusInside = radius - line;
        canvas.drawRoundRect(rect, radiusInside, radiusInside, paint);

        // The round knob fills the full inner height. When pressed the knob
        // stretches toward the center so its outer edge does not jump.
        float diameter = pillHeight - 2f * line;
        float stretch = stretchPx * press;
        float travel = pillWidth - 2f * line - diameter;
        float knobLeft = left + line + travel * position - stretch * position;
        rect.set(knobLeft, top + line, knobLeft + diameter + stretch, top + line + diameter);

        paint.setColor(knobColor);
        canvas.drawRoundRect(rect, diameter / 2f, diameter / 2f, paint);
    }
}
