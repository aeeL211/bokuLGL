package com.android.support;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.text.Html;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextUtils;
import android.text.method.DigitsKeyListener;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

/**
 * Every menu dialog, built by hand (not the stock AlertDialog) so they look the
 * same on every Android version and brand.
 *
 * The layout mirrors the menu panel: 3dp brown frame, header band with the
 * title, wavy edge, content on BODY, wavy edge, then a footer with the wood
 * buttons. The main button (right) has a white frame like an active menu
 * control, the other buttons have a cream frame.
 *
 * Open dialogs are tracked and closed in {@link #close()}, so no dialog is left
 * holding a Context after the menu is destroyed.
 */
final class MenuDialog {
    private static final String TAG = Menu.TAG;
    private static final int TYPE_OVERLAY = Build.VERSION.SDK_INT >= 26 ? 2038 : 2002;

    /** Maximum width matches the menu width, leaving a screen margin. */
    private static final float WIDTH_MAX_DP = 340f;
    private static final float MARGIN_SCREEN_DP = 16f;
    /** Content taller than this scrolls, so the buttons are not pushed off screen. */
    private static final float HEIGHT_CONTENT_MAX = 0.5f;
    private static final float DIM = 0.5f;
    private static final float TEXT_MESSAGE_SP = Theme.ROW_SP;

    /** Receiver for the text the user enters. */
    interface Receiver {
        void onText(String text);
    }

    /** One button: label and the action run before the dialog closes. */
    static final class Action {
        final String label;
        final Runnable action;

        Action(String label, Runnable action) {
            this.label = label == null ? "" : label;
            this.action = action;
        }
    }

    /** The displayed dialog with its buttons (null when that button does not exist). */
    static final class Result {
        final Dialog dialog;
        Button positive;
        Button neutral;
        Button negative;

        Result(Dialog dialog) {
            this.dialog = dialog;
        }
    }

    private final Context context;
    private boolean overlay;
    private Dialog opened;

    MenuDialog(Context context) {
        this.context = context;
    }

    /** The dialog must use an overlay window when the menu runs as a Service. */
    void setOverlay(boolean overlay) {
        this.overlay = overlay;
    }

    /** Closes the dialog that is still open. Safe to call repeatedly. */
    void close() {
        Dialog dialog = opened;
        opened = null;
        if (dialog != null && dialog.isShowing()) {
            try {
                dialog.dismiss();
            } catch (Throwable t) {
                Log.d(TAG, "Failed to close dialog: " + t);
            }
        }
    }

    /** Explanation dialog opened by long-pressing a feature. */
    void explanation(int featNum, String featName) {
        display(Html.fromHtml(featName), Explanation.forTarget(featNum, featName), null, null,
                new Action("Mengerti", null), null, null, true);
    }

    /**
     * Dialog with a single input field. Enter on the keyboard acts like pressing
     * OK. This dialog does not close on an outside tap so text being typed is not
     * lost by accident.
     *
     * @param hint         caption above the field (e.g. the value range), or null
     * @param valueText    true when only digits and a minus sign are allowed
     * @param maxCharacter text length limit, 0 for no limit
     * @param neutral      third button label, or null when there is none
     */
    void input(String title, String hint, String initial, boolean valueText, int maxCharacter,
               final Receiver ok, String neutral, Runnable actionNeutral) {
        final EditText inputField = new EditText(context);
        inputField.setTextColor(Theme.TEXT);
        inputField.setTextSize(Theme.ROW_SP);
        inputField.setHintTextColor(Theme.MUTED);
        inputField.setBackground(Theme.dialogInput(context));
        inputField.setMinHeight(dp(Theme.TARGET_DP));
        inputField.setPadding(dp(12), dp(8), dp(12), dp(8));
        inputField.setSingleLine(true);
        inputField.setImeOptions(EditorInfo.IME_ACTION_DONE);
        if (valueText) {
            inputField.setInputType(InputType.TYPE_CLASS_NUMBER);
            inputField.setKeyListener(DigitsKeyListener.getInstance("0123456789-"));
        }
        if (maxCharacter > 0)
            inputField.setFilters(new InputFilter[]{new InputFilter.LengthFilter(maxCharacter)});
        if (initial != null && initial.length() > 0) {
            inputField.setText(initial);
            inputField.setSelection(inputField.length());
        }

        LinearLayout content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        if (hint != null && hint.length() > 0) {
            TextView caption = Theme.text(context, hint, Theme.DESC_SP, Theme.MUTED);
            caption.setPadding(dp(2), 0, dp(2), dp(6));
            content.addView(caption, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        }
        content.addView(inputField, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        final Result[] result = new Result[1];
        inputField.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE && result[0] != null
                        && result[0].positive != null) {
                    result[0].positive.performClick();
                    return true;
                }
                return false;
            }
        });

        Action okButton = new Action("OK", new Runnable() {
            @Override
            public void run() {
                ok.onText(inputField.getText().toString());
            }
        });
        Action neutralButton = neutral != null && actionNeutral != null
                ? new Action(neutral, actionNeutral) : null;
        result[0] = display(title, null, content, inputField, okButton, neutralButton,
                new Action("Batal", null), false);
    }

    /**
     * Builds and shows the dialog. Returns null when it fails to show
     * (no overlay permission or a dead Context) -- the menu must not crash
     * because of a dialog.
     *
     * @param message plain text in the dialog body, may be null
     * @param content extra view below the message, may be null
     * @param focus   input field that gets focus and the keyboard, may be null
     */
    Result display(CharSequence title, CharSequence message, View content, EditText focus,
                   Action positive, Action neutral, Action negative, boolean closeOutside) {
        close();

        final Dialog dialog = new Dialog(context, android.R.style.Theme_DeviceDefault_Dialog);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        Result result = new Result(dialog);

        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        int width = Math.min(dp(WIDTH_MAX_DP), dm.widthPixels - 2 * dp(MARGIN_SCREEN_DP));

        // Outer frame: 3dp dark fill, the view padding forms the border line.
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackground(Theme.panel(context));
        root.setPadding(dp(3), dp(3), dp(3), dp(3));

        TextView header = Theme.text(context, title == null ? "" : title, Theme.SECTION_SP,
                Theme.TITLE_TEXT);
        header.setGravity(Gravity.CENTER);
        header.setMaxLines(2);
        header.setEllipsize(TextUtils.TruncateAt.END);
        header.setMinHeight(dp(Theme.TARGET_DP));
        header.setPadding(dp(14), dp(8), dp(14), dp(8));
        header.setBackground(Theme.header(context));
        root.addView(header, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.Zigzag zigTop = new Theme.Zigzag(context, Theme.HEADER, true);
        zigTop.setBackgroundColor(Theme.BODY);
        root.addView(zigTop, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));

        LinearLayout body = new LinearLayout(context);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(14), dp(8), dp(14), dp(12));
        if (message != null && message.length() > 0) {
            TextView text = Theme.text(context, message, TEXT_MESSAGE_SP, Theme.TEXT);
            text.setLineSpacing(dp(2), 1f);
            body.addView(text, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        }
        if (content != null)
            body.addView(content, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        ScrollMax scroll = new ScrollMax(context, (int) (dm.heightPixels * HEIGHT_CONTENT_MAX));
        scroll.setBackgroundColor(Theme.BODY);
        Theme.scrollbarStyle(context, scroll);
        scroll.addView(body, new ViewGroup.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        root.addView(scroll, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.Zigzag zigBottom = new Theme.Zigzag(context, Theme.FOOTER, false);
        zigBottom.setBackgroundColor(Theme.BODY);
        root.addView(zigBottom, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));

        // Left-to-right order: negative, neutral, positive (main button on the right).
        LinearLayout footer = new LinearLayout(context);
        footer.setOrientation(LinearLayout.HORIZONTAL);
        footer.setBackground(Theme.footer(context));
        footer.setPadding(dp(8), dp(6), dp(8), dp(6));
        result.negative = addButton(footer, dialog, negative, false);
        result.neutral = addButton(footer, dialog, neutral, false);
        result.positive = addButton(footer, dialog, positive, true);
        root.addView(footer, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.applyFont(root);
        dialog.setContentView(root, new ViewGroup.LayoutParams(width, WRAP_CONTENT));
        dialog.setCanceledOnTouchOutside(closeOutside);

        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            window.setLayout(width, WRAP_CONTENT);
            window.setGravity(Gravity.CENTER);
            window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.setDimAmount(DIM);
            if (overlay)
                window.setType(TYPE_OVERLAY);
            if (focus != null)
                window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE
                        | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }

        try {
            dialog.show();
        } catch (Throwable t) {
            Log.d(TAG, "Failed to show dialog: " + t);
            return null;
        }
        opened = dialog;
        Transition.enter(root);
        if (focus != null)
            showKeyboard(focus);
        return result;
    }

    private Button addButton(LinearLayout footer, final Dialog dialog, final Action action,
                             boolean primary) {
        if (action == null)
            return null;

        Button view = new Button(context);
        view.setText(action.label);
        view.setTextColor(Theme.TITLE_TEXT);
        view.setTextSize(Theme.ROW_SP);
        view.setTypeface(Theme.font(context));
        view.setAllCaps(false);
        view.setSingleLine(true);
        view.setEllipsize(TextUtils.TruncateAt.END);
        view.setGravity(Gravity.CENTER);
        view.setBackground(primary ? Theme.mainButton(context) : Theme.pressButton(context));
        view.setMinWidth(0);
        view.setMinimumWidth(0);
        view.setMinHeight(dp(Theme.TARGET_DP));
        view.setMinimumHeight(dp(Theme.TARGET_DP));
        view.setPadding(dp(8), 0, dp(8), 0);
        view.setStateListAnimator(null);
        view.setElevation(0f);
        view.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // The action runs first (it reads the field), the dialog closes
                // afterwards even if the action throws.
                try {
                    if (action.action != null)
                        action.action.run();
                } finally {
                    dialog.dismiss();
                }
            }
        });

        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(0, dp(Theme.TARGET_DP), 1f);
        if (footer.getChildCount() > 0)
            params.leftMargin = dp(6);
        footer.addView(view, params);
        return view;
    }

    /** The keyboard is shown through showSoftInput, not toggleSoftInput which can hide it instead. */
    private void showKeyboard(final EditText field) {
        field.requestFocus();
        field.post(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager)
                        context.getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null)
                    imm.showSoftInput(field, InputMethodManager.SHOW_IMPLICIT);
            }
        });
    }

    /** Height-capped ScrollView, so long content does not push the buttons off screen. */
    private static final class ScrollMax extends ScrollView {
        private final int max;

        ScrollMax(Context context, int max) {
            super(context);
            this.max = max;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int remaining = MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.UNSPECIFIED
                    ? max : Math.min(max, MeasureSpec.getSize(heightMeasureSpec));
            super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(remaining, MeasureSpec.AT_MOST));
        }
    }

    private int dp(float value) {
        return (int) Theme.dp(context, value);
    }
}
