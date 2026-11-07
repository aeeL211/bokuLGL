package com.android.support;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Typeface;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * Palette, radii, and drawable builders for the pixel-art wood panel menu.
 *
 * Every color and corner size lives here, so Menu.java no longer writes its own
 * hex values or dp numbers.
 */
public final class Theme {
    private Theme() {
    }

    // Wood panel palette.
    public static final int HEADER = 0xFF996C34;
    public static final int BODY = 0xFFFACA82;
    public static final int ROW = 0xFFDCB983;
    public static final int ICON_BOX = 0xFF81521D;
    public static final int TITLE_TEXT = 0xFFEABD7B;
    public static final int TEXT = 0xFF2B1A08;
    public static final int STROKE_CONTROL = 0xFFB28950;
    public static final int SWITCH_ON_TRACK = 0xFFF8B551;
    public static final int SWITCH_ON_KNOB = 0xFFFFF799;
    public static final int SWITCH_OFF_TRACK = 0xFFB28950;
    public static final int SWITCH_OFF_KNOB = 0xFFCEA972;
    public static final int TAN = 0xFFC9A56F;
    public static final int TRACK = 0xFF936832;
    public static final int ACTIVE_BORDER = 0xFFF6F6F6;

    // Corner button like the in-game back button: black box, white line.
    public static final int BOX_CORNER = 0xFF000000;
    public static final int BOX_CORNER_PRESS = 0xFF3A3A3A;

    // Pressed colors: rows slightly darker, filled buttons use TRACK.
    public static final int ROW_PRESS = 0xFFCDA874;
    public static final int HEADER_PRESS = 0xFF86602D;

    // Three game pixel-art icon tones (taken from the CRT filter / Vignette / Light Effect icons).
    public static final int ICON_LIGHT = 0xFFCCAA77;
    public static final int ICON_MID = 0xFFBB8855;
    public static final int ICON_DARK = 0xFF996633;

    // Wood button edge: light cream over a dark brown fill.
    public static final int BUTTON_BORDER = 0xFFDCB983;

    // Palette derivatives used repeatedly.
    public static final int FOOTER = HEADER;
    public static final int MUTED = ICON_BOX;
    public static final int ACCENT = TITLE_TEXT;

    // Corners, in dp. Every drawable takes its values from here.
    public static final float RADIUS_PANEL = 3f;
    public static final float RADIUS_ROW = 5f;
    public static final float RADIUS_ICON = 5f;
    public static final float RADIUS_THUMB = 3f;
    public static final float RADIUS_KNOB = 10f;

    // Line thickness, in dp.
    public static final float BOLD_PANEL = 3f;
    public static final float BOLD_CONTROL = 3f;
    public static final float BOLD_FOCUS = 3f;

    // Text sizes. No bold anywhere in the menu.
    public static final float TITLE_SP = 22f;
    public static final float SECTION_SP = 18f;
    public static final float ROW_SP = 16f;
    public static final float DESC_SP = 14f;

    /** Minimum touch size of every control, in dp. */
    public static final float TARGET_DP = 48f;

    // Slider as before: 28dp-tall SeekBar, 4dp track (12dp inset top and bottom), 10x24dp thumb.
    /** SeekBar height. */
    public static final float SLIDER_HEIGHT_DP = 28f;
    private static final float SLIDER_INSET_DP = 12f;
    private static final float SLIDER_THUMB_WIDTH_DP = 10f;
    private static final float SLIDER_THUMB_HEIGHT_DP = 24f;
    private static final float ZIGZAG_FINGER_DP = 3f;
    private static final float ZIGZAG_STEP_DP = 8f;

    private static final String FONT_ASSET = "fonts/itim.ttf";
    private static Typeface sFont;

    public static Typeface font(Context context) {
        if (sFont == null) {
            try {
                sFont = Typeface.createFromAsset(context.getApplicationContext().getAssets(),
                        FONT_ASSET);
            } catch (Throwable t) {
                Log.d(BKLog.TAG, "Failed to load font: " + t);
                sFont = Typeface.DEFAULT;
            }
        }
        return sFont;
    }

    /**
     * Applies the game font to every TextView, Button, Switch, CheckBox, EditText,
     * and their subclasses. Bold spans from Html.fromHtml are stripped too so the
     * text stays regular.
     */
    public static void applyFont(View view) {
        if (view == null)
            return;

        if (view instanceof TextView) {
            TextView textView = (TextView) view;
            textView.setTypeface(font(view.getContext()));
            textView.setPaintFlags(textView.getPaintFlags() & ~Paint.FAKE_BOLD_TEXT_FLAG);
            if (!(textView instanceof EditText))
                deleteBold(textView);
        }

        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++)
                applyFont(group.getChildAt(i));
        }
    }

    private static void deleteBold(TextView textView) {
        CharSequence content = textView.getText();
        if (!(content instanceof Spanned))
            return;

        // Text is not rebuilt when there is no bold span at all.
        StyleSpan[] old = ((Spanned) content).getSpans(0, content.length(), StyleSpan.class);
        boolean hasBold = false;
        for (StyleSpan span : old) {
            if ((span.getStyle() & Typeface.BOLD) != 0) {
                hasBold = true;
                break;
            }
        }
        if (!hasBold)
            return;

        SpannableStringBuilder fresh = new SpannableStringBuilder(content);
        for (StyleSpan span : fresh.getSpans(0, fresh.length(), StyleSpan.class)) {
            if ((span.getStyle() & Typeface.BOLD) != 0)
                fresh.removeSpan(span);
        }
        textView.setText(fresh);
    }

    public static TextView text(Context context, CharSequence content, float sizeSp, int color) {
        TextView textView = new TextView(context);
        textView.setText(content);
        textView.setTextSize(sizeSp);
        textView.setTextColor(color);
        textView.setTypeface(font(context));
        return textView;
    }

    public static float dp(Context context, float value) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                context.getResources().getDisplayMetrics());
    }

    public static String colorHex(int color) {
        return String.format("#%06X", 0xFFFFFF & color);
    }

    private static final double GAMMA = 2.2;

    /**
     * Blends two ARGB colors without boxing, safe to call every frame. The
     * result matches Android's stock ArgbEvaluator: RGB is mixed in linear space
     * (gamma 2.2), alpha is mixed plainly. Mixing directly in sRGB would make
     * mid-animation colors look slightly darker.
     */
    public static int mix(int from, int to, float f) {
        if (f <= 0f)
            return from;
        if (f >= 1f)
            return to;
        int a0 = from >>> 24;
        int a = Math.round(a0 + ((to >>> 24) - a0) * f);
        int r = channel((from >> 16) & 0xFF, (to >> 16) & 0xFF, f);
        int g = channel((from >> 8) & 0xFF, (to >> 8) & 0xFF, f);
        int b = channel(from & 0xFF, to & 0xFF, f);
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    private static int channel(int from, int to, float f) {
        double d = Math.pow(from / 255.0, GAMMA);
        double k = Math.pow(to / 255.0, GAMMA);
        double v = d + (k - d) * f;
        return (int) Math.round(Math.pow(v, 1.0 / GAMMA) * 255.0);
    }

    private static GradientDrawable box(int color, float radiusDp, float boldDp,
                                           int lineColor, Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(context, radiusDp));
        if (boldDp > 0)
            drawable.setStroke(Math.max(1, (int) dp(context, boldDp)), lineColor);
        return drawable;
    }

    /** Drawable with one normal state and one pressed state. */
    private static Drawable press(Drawable normal, Drawable pressed) {
        StateListDrawable drawable = new StateListDrawable();
        drawable.addState(new int[]{android.R.attr.state_pressed}, pressed);
        drawable.addState(new int[]{}, normal);
        return drawable;
    }

    /** Panel outer frame: 3dp dark fill, so the view padding forms the outline. */
    public static Drawable panel(Context context) {
        return box(ICON_BOX, RADIUS_PANEL, 0f, ICON_BOX, context);
    }

    public static Drawable header(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(HEADER);
        float r = dp(context, RADIUS_PANEL);
        drawable.setCornerRadii(new float[]{r, r, r, r, 0f, 0f, 0f, 0f});
        return drawable;
    }

    public static Drawable footer(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(FOOTER);
        float r = dp(context, RADIUS_PANEL);
        drawable.setCornerRadii(new float[]{0f, 0f, 0f, 0f, r, r, r, r});
        return drawable;
    }

    /** Section header: plain brown band, no thick stroke. */
    public static Drawable sectionHeader(Context context) {
        return box(HEADER, RADIUS_ROW, 0f, HEADER, context);
    }

    /** Tappable section header: slightly darker when pressed. */
    public static Drawable sectionHeaderPress(Context context) {
        return press(sectionHeader(context),
                box(HEADER_PRESS, RADIUS_ROW, 0f, HEADER_PRESS, context));
    }

    /** Transparent section body: the rows inside sit directly on BODY. */
    public static Drawable sectionBody(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(0x00000000);
        return drawable;
    }

    /** Game control row: flat #DCB983 with no stroke, ~5dp rounded corners. */
    public static Drawable row(Context context) {
        return box(ROW, RADIUS_ROW, 0f, ROW, context);
    }

    /** Dialog main button: like an active button (white stroke), with press feedback. */
    public static Drawable mainButton(Context context) {
        return press(activeButton(context),
                box(TRACK, RADIUS_PANEL, BOLD_FOCUS, ACTIVE_BORDER, context));
    }

    /** Dialog input field: a ROW box with a thin stroke instead of the default underline. */
    public static Drawable dialogInput(Context context) {
        return box(ROW, RADIUS_ROW, 1f, STROKE_CONTROL, context);
    }

    /** Tappable card: slightly darker when pressed. */
    public static Drawable card(Context context) {
        return press(box(ROW, RADIUS_ROW, 0f, ROW, context),
                box(ROW_PRESS, RADIUS_ROW, 0f, ROW_PRESS, context));
    }

    /** The selected player card uses a white focus stroke. */
    public static Drawable cardSelected(Context context) {
        return press(box(ROW, RADIUS_ROW, BOLD_FOCUS, ACTIVE_BORDER, context),
                box(ROW_PRESS, RADIUS_ROW, BOLD_FOCUS, ACTIVE_BORDER, context));
    }

    /** Wood button: dark brown fill with a 3dp cream stroke, small corners. */
    public static Drawable button(Context context) {
        return box(ICON_BOX, RADIUS_PANEL, BOLD_CONTROL, BUTTON_BORDER, context);
    }

    public static Drawable activeButton(Context context) {
        return box(ICON_BOX, RADIUS_PANEL, BOLD_FOCUS, ACTIVE_BORDER, context);
    }

    public static Drawable offButton(Context context) {
        return box(TRACK, RADIUS_PANEL, BOLD_CONTROL, STROKE_CONTROL, context);
    }

    /** Small button: the fill turns TRACK when pressed so it feels responsive. */
    public static Drawable smallButton(Context context) {
        return smallButton(context, 0f);
    }

    /**
     * Small button with more touch area than its drawing. The view is 48dp
     * tall, but the box is drawn shrunk by {@code insetVertDp} at the top and
     * bottom, so the look stays the same and fingers still land.
     */
    public static Drawable smallButton(Context context, float insetVertDp) {
        Drawable drawable = press(
                box(ICON_BOX, RADIUS_PANEL, 2f, BUTTON_BORDER, context),
                box(TRACK, RADIUS_PANEL, 2f, BUTTON_BORDER, context));
        if (insetVertDp <= 0f)
            return drawable;
        int v = (int) dp(context, insetVertDp);
        return new InsetDrawable(drawable, 0, v, 0, v);
    }

    /** Full wood button (footer) with press feedback. */
    public static Drawable pressButton(Context context) {
        return press(button(context),
                box(TRACK, RADIUS_PANEL, BOLD_CONTROL, BUTTON_BORDER, context));
    }

    /**
     * Thin layer for a Button inside a button box that already has a border:
     * usually transparent, slightly brighter when pressed.
     */
    public static Drawable pressOverlay(Context context) {
        return press(box(0x00000000, RADIUS_PANEL, 0f, 0x00000000, context),
                box(0x26FFFFFF, RADIUS_PANEL, 0f, 0x26FFFFFF, context));
    }

    /** Tappable row: slightly darker when pressed. */
    public static Drawable rowPress(Context context) {
        return press(row(context),
                box(ROW_PRESS, RADIUS_ROW, 0f, ROW_PRESS, context));
    }

    /** Thin TRACK-colored scrollbar, like the scroll marker in the game. */
    public static void scrollbarStyle(Context context, ScrollView scrollView) {
        scrollView.setVerticalScrollBarEnabled(true);
        scrollView.setScrollBarSize((int) dp(context, 3f));
        if (Build.VERSION.SDK_INT >= 29)
            scrollView.setVerticalScrollbarThumbDrawable(box(TRACK, 2f, 0f, TRACK, context));
    }

    public static Drawable badge(Context context, int color) {
        return box(color, RADIUS_PANEL, 0f, color, context);
    }

    /**
     * Thin 4dp track like the game slider: #936832 line, the filled part uses
     * dark brown #81521D so the position stays readable over the #DCB983 row.
     * The SeekBar is 28dp tall with a 12dp inset at the top and bottom.
     */
    public static Drawable seekbarProgress(Context context) {
        GradientDrawable track = box(TRACK, 2f, 0f, TRACK, context);
        GradientDrawable content = box(ICON_BOX, 2f, 0f, ICON_BOX, context);
        ClipDrawable clip = new ClipDrawable(content, Gravity.LEFT, ClipDrawable.HORIZONTAL);

        LayerDrawable layers = new LayerDrawable(new Drawable[]{track, clip});
        int inset = (int) dp(context, SLIDER_INSET_DP);
        layers.setLayerInset(0, 0, inset, 0, inset);
        layers.setLayerInset(1, 0, inset, 0, inset);
        return layers;
    }

    /** Upright rectangular #936832 thumb like the game, with no black border. */
    public static Drawable seekbarThumb(Context context) {
        GradientDrawable drawable = box(TRACK, RADIUS_THUMB, 0f, TRACK, context);
        drawable.setSize((int) dp(context, SLIDER_THUMB_WIDTH_DP),
                (int) dp(context, SLIDER_THUMB_HEIGHT_DP));
        return drawable;
    }

    /**
     * Wavy edge that separates the header and body, like the jagged edge in
     * the game. Half circles colored {@code bumpColor} are drawn in a row over
     * the view background. When {@code hanging} is true the waves hang from
     * the top edge (below the header); when false they rise from the bottom
     * edge (above the footer).
     */
    public static class Zigzag extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final float fingerDp;
        private final float step;
        private final boolean hanging;

        public Zigzag(Context context, int bumpColor, boolean hanging) {
            super(context);
            this.hanging = hanging;
            this.fingerDp = dp(context, ZIGZAG_FINGER_DP);
            this.step = dp(context, ZIGZAG_STEP_DP);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(bumpColor);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float width = getWidth();
            float height = getHeight();
            float finger = Math.min(height, fingerDp);
            float cy = hanging ? 0f : height;
            for (float x = step / 2f; x < width + step; x += step)
                canvas.drawCircle(x, cy, finger, paint);
        }
    }

    /**
     * Small fold arrow (right or down). Feature icons use IconPattern because
     * they are a 16x16 grid. The path is built once whenever the size changes.
     */
    public static class Icon extends View {
        public static final int ARROW_RIGHT = 2;
        public static final int ARROW_DOWN = 3;

        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();
        private final int sizePx;
        private int kind;

        public static Icon plain(Context context, int kind, int glyphColor, int sizeDp) {
            return new Icon(context, kind, glyphColor, sizeDp);
        }

        private Icon(Context context, int kind, int glyphColor, int sizeDp) {
            super(context);
            this.kind = kind;
            this.sizePx = (int) dp(context, sizeDp);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(glyphColor);
        }

        public void setKind(int kind) {
            if (this.kind == kind)
                return;
            this.kind = kind;
            shape(getWidth(), getHeight());
            invalidate();
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            setMeasuredDimension(resolveSize(sizePx, widthMeasureSpec),
                    resolveSize(sizePx, heightMeasureSpec));
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            shape(w, h);
        }

        private void shape(int width, int height) {
            float cx = width / 2f;
            float cy = height / 2f;
            float s = Math.min(width, height);
            path.reset();
            if (kind == ARROW_DOWN) {
                path.moveTo(cx - s * 0.24f, cy - s * 0.14f);
                path.lineTo(cx + s * 0.24f, cy - s * 0.14f);
                path.lineTo(cx, cy + s * 0.22f);
            } else {
                path.moveTo(cx - s * 0.16f, cy - s * 0.24f);
                path.lineTo(cx + s * 0.22f, cy);
                path.lineTo(cx - s * 0.16f, cy + s * 0.24f);
            }
            path.close();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            canvas.drawPath(path, paint);
        }
    }
}
