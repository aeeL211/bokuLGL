//Please don't replace listeners with lambda!

package com.android.support;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.text.Html;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.Base64;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;
import static android.view.WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
import static android.widget.RelativeLayout.ALIGN_PARENT_RIGHT;

public class Menu {
    public static final String TAG = BKLog.TAG; //Tag for logcat

    // Wooden panel styling. All colors live in Theme.
    int MENU_WIDTH = 340;
    int MENU_HEIGHT = 210;
    int POS_X = 0;
    int POS_Y = 100;
    int ICON_SIZE = 45; //Change both width and height of image
    float ICON_ALPHA = 0.7f; //Transparent

    // Wait intervals for the game library to load (ms), and display setting keys
    // stored in the same preferences as the features.
    private static final long WAIT_INITIAL_MS = 500L;
    private static final long WAIT_LIB_MS = 600L;
    private static final String KEY_X = "ui_posisi_x";
    private static final String KEY_Y = "ui_posisi_y";
    private static final String KEY_FOLD = "ui_lipat_";
    // Slider value chip: 26dp tall image inside a 48dp touch area.
    private static final float BUTTON_SMALL_INSET_DP = 9f;
    // Auto dump writes files for every guest that joins, so it needs a warning
    // before it is switched on.
    private static final int FEATURE_AUTO_DUMP = 30;

    RelativeLayout mCollapsed, mRootContainer;
    LinearLayout mExpanded, mods, mSettings, mCollapse;
    WindowManager mWindowManager;
    WindowManager.LayoutParams vmParams;
    ImageView startimage;
    ImageView iconImage;
    Drawable iconDrawable;
    String iconSource = "";
    String sourceInstalled;
    boolean imageReady;
    FrameLayout rootFrame;
    ScrollView scrollView;
    boolean stopChecking, overlayRequired;
    Context getContext;
    private MenuDialog menuDialog;
    private boolean destroyed;

    //initialize methods from the native library
    native void Init(Context context, TextView title, TextView subTitle);

    native String Icon();

    native String IconWebViewData();

    native String[] GetFeatureList();

    native String[] SettingsList();

    native boolean IsGameLibLoaded();

    // Room section: guest list formatted as "name<TAB>key<TAB>role" per line.
    native String PlayerList();

    // Local player position: "tileX<TAB>tileY<TAB>worldX<TAB>worldY". The tile part
    // is empty when the game does not provide tile fields.
    native String PlayerPosition();

    native boolean KickPlayer(String key, String name, boolean permanent);

    native boolean KickHost(String name);

    native boolean KickAll();

    native boolean CopyPlayerSkin(String key, String name);

    // Key of the player whose card is currently selected. The choice is stored as
    // key text, not an index, so it keeps pointing at the same player when the list
    // changes order and is released automatically when that player leaves.
    String keySelected;

    // Default-value restorers for display settings (size, opacity, icon image).
    private final List<Runnable> setterReload = new ArrayList<Runnable>();

    // One tick owned by Menu for everything that must stay fresh (player list,
    // position). The tick runs while the menu is open, independent of each
    // view's attach/detach, and runs once right when the menu opens so the
    // content is not stale.
    private static final long TICK_MS = 1000L;
    private final Handler handlerTick = new Handler(android.os.Looper.getMainLooper());
    private final List<Runnable> pemantau = new ArrayList<Runnable>();
    private boolean tickRun;
    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            if (!tickRun)
                return;
            if (mExpanded != null && mExpanded.getVisibility() == View.VISIBLE)
                startMonitor();
            handlerTick.postDelayed(this, TICK_MS);
        }
    };

    private void startMonitor() {
        // No list copy on each tick. Observers only grow, and those that
        // register during a running pass only join the next pass.
        int count = pemantau.size();
        for (int i = 0; i < count; i++) {
            try {
                pemantau.get(i).run();
            } catch (Throwable t) {
                Log.d(TAG, "Observer failed: " + t);
            }
        }
    }

    private void registerMonitor(Runnable r) {
        pemantau.add(r);
    }

    private void startTicker() {
        if (tickRun)
            return;
        tickRun = true;
        handlerTick.postDelayed(tick, TICK_MS);
    }

    private void openMenu() {
        setAnimationIcon(false);
        mCollapsed.setVisibility(View.GONE);
        mExpanded.setVisibility(View.VISIBLE);
        Transition.appear(mExpanded, 1f);
        startMonitor();
    }

    /**
     * Matches the collapsed icon size, opacity, and source to the settings.
     * The built-in icon stays in startimage. A user source (URL, GIF) is decoded
     * by IconImage into a Drawable and shown in a plain ImageView, no WebView.
     */
    private void applyIcon() {
        int side = dp(ICON_SIZE);
        for (View icon : new View[]{startimage, iconImage}) {
            ViewGroup.LayoutParams params = icon.getLayoutParams();
            params.width = side;
            params.height = side;
            icon.setLayoutParams(params);
        }
        mCollapsed.setAlpha(ICON_ALPHA);

        String source = iconSource == null ? "" : iconSource.trim();
        if (source.length() == 0)
            source = IconWebViewData();
        if (source == null || source.length() == 0) {
            sourceInstalled = null;
            releaseImage();
            showIcon();
            return;
        }

        // The same source is not loaded again, so dragging the size and opacity
        // sliders does not trigger a re-download or re-decode.
        if (!source.equals(sourceInstalled)) {
            sourceInstalled = source;
            releaseImage();
            // The built-in icon stays visible until the image is ready. On
            // failure it is kept and retried the next time the menu is created.
            final String target = source;
            IconCache.load(getContext, target, new IconCache.Listener() {
                @Override
                public void ready(Drawable icon) {
                    if (!target.equals(sourceInstalled))
                        return;
                    iconDrawable = icon;
                    iconImage.setLayerType(IconImage.needSoftware(icon)
                            ? View.LAYER_TYPE_SOFTWARE : View.LAYER_TYPE_NONE, null);
                    iconImage.setImageDrawable(icon);
                    imageReady = true;
                    showIcon();
                    setAnimationIcon(isViewCollapsed());
                }

                @Override
                public void fail() {
                    Log.d(TAG, "Icon failed to load, using the built-in icon");
                }
            });
        }
        showIcon();
    }

    private void releaseImage() {
        setAnimationIcon(false);
        iconImage.setImageDrawable(null);
        iconDrawable = null;
        imageReady = false;
    }

    private void showIcon() {
        iconImage.setVisibility(imageReady ? View.VISIBLE : View.GONE);
        startimage.setVisibility(imageReady ? View.GONE : View.VISIBLE);
    }

    /** The GIF only runs while the small icon is visible; it stops while the menu is open to save CPU. */
    private void setAnimationIcon(boolean run) {
        if (iconDrawable != null)
            IconImage.setRunning(iconDrawable, run);
    }

    //Here we write the code for our Menu
    // Reference: https://www.androidhive.info/2016/11/android-floating-widget-like-facebook-chat-head/
    public Menu(Context context) {

        getContext = context;
        Preferences.context = context;
        menuDialog = new MenuDialog(context);
        rootFrame = new FrameLayout(context); // Global markup
        rootFrame.setOnTouchListener(onTouchListener());
        mRootContainer = new RelativeLayout(context); // Markup on which two markups of the icon and the menu itself will be placed
        mCollapsed = new RelativeLayout(context); // Markup of the icon (when the menu is minimized)
        mCollapsed.setVisibility(View.VISIBLE);
        mCollapsed.setAlpha(ICON_ALPHA);

        //********** The box of the mod menu **********
        mExpanded = new LinearLayout(context); // Menu markup (when the menu is expanded)
        mExpanded.setVisibility(View.GONE);
        mExpanded.setOrientation(LinearLayout.VERTICAL);
        mExpanded.setBackground(Theme.panel(context));
        mExpanded.setPadding(dp(3), dp(3), dp(3), dp(3));
        mExpanded.setLayoutParams(new LinearLayout.LayoutParams(dp(MENU_WIDTH), WRAP_CONTENT));

        //********** The icon to open mod menu **********
        startimage = new ImageView(context);
        startimage.setLayoutParams(new RelativeLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        startimage.getLayoutParams().height = dp(ICON_SIZE);
        startimage.getLayoutParams().width = dp(ICON_SIZE);
        startimage.setScaleType(ImageView.ScaleType.FIT_XY);
        String iconData = Icon();
        if (iconData != null && iconData.length() > 0) {
            byte[] decode = Base64.decode(iconData, 0);
            startimage.setImageBitmap(BitmapFactory.decodeByteArray(decode, 0, decode.length));
        }
        startimage.setContentDescription("Buka menu");
        ((ViewGroup.MarginLayoutParams) startimage.getLayoutParams()).topMargin = dp(10);
        //Initialize event handlers for buttons, etc.
        startimage.setOnTouchListener(onTouchListener());
        startimage.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                openMenu();
            }
        });

        //********** Icon from the user's chosen source (URL or GIF) **********
        iconImage = new ImageView(context);
        iconImage.setLayoutParams(new RelativeLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        iconImage.getLayoutParams().height = dp(ICON_SIZE);
        iconImage.getLayoutParams().width = dp(ICON_SIZE);
        ((ViewGroup.MarginLayoutParams) iconImage.getLayoutParams()).topMargin = dp(10);
        iconImage.setScaleType(ImageView.ScaleType.FIT_CENTER);
        iconImage.setVisibility(View.GONE);
        iconImage.setOnTouchListener(onTouchListener());
        iconImage.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                openMenu();
            }
        });

        //********** Settings **********
        mSettings = new LinearLayout(context);
        mSettings.setOrientation(LinearLayout.VERTICAL);
        mSettings.setPadding(dp(10), dp(6), dp(10), dp(8));
        fillSettings(SettingsList());

        //********** Header panel: centered title, only the minimize button on the right **********
        RelativeLayout header = new RelativeLayout(context);
        header.setBackground(Theme.header(context));
        header.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(56)));

        RelativeLayout.LayoutParams rlTitle = new RelativeLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT);
        final TextView title = Theme.text(context, "", Theme.TITLE_SP, Theme.TITLE_TEXT);
        title.setGravity(Gravity.CENTER);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        title.setPadding(dp(48), 0, dp(48), 0);
        title.setLayoutParams(rlTitle);

        // Game-style corner button: black box, white outline, pixel triangle
        // facing right. The image is 34dp, the touch area is 48dp.
        CornerButton minimize = new CornerButton(context, IconPattern.TRIANGLE_LEFT);
        RelativeLayout.LayoutParams minimizeParams = new RelativeLayout.LayoutParams(
                dp(Theme.TARGET_DP), dp(Theme.TARGET_DP));
        minimizeParams.addRule(ALIGN_PARENT_RIGHT);
        minimizeParams.addRule(RelativeLayout.CENTER_VERTICAL);
        minimizeParams.rightMargin = dp(1);
        minimize.setLayoutParams(minimizeParams);
        minimize.setContentDescription("Kecilkan menu");
        minimize.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(ICON_ALPHA);
                mExpanded.setVisibility(View.GONE);
                setAnimationIcon(true);
            }
        });

        header.addView(title);
        header.addView(minimize);

        //********** Sub title **********
        final TextView subTitle = Theme.text(context, "", Theme.DESC_SP, Theme.MUTED);
        subTitle.setEllipsize(TextUtils.TruncateAt.MARQUEE);
        subTitle.setMarqueeRepeatLimit(-1);
        subTitle.setSingleLine(true);
        subTitle.setSelected(true);
        subTitle.setGravity(Gravity.CENTER);
        subTitle.setPadding(dp(10), dp(4), dp(10), dp(4));
        subTitle.setBackgroundColor(Theme.BODY);

        //********** Mod menu feature list **********
        scrollView = new ScrollView(context);
        scrollView.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(MENU_HEIGHT)));
        scrollView.setBackgroundColor(Theme.BODY);
        Theme.scrollbarStyle(context, scrollView);
        mods = new LinearLayout(context);
        mods.setOrientation(LinearLayout.VERTICAL);
        mods.setPadding(dp(10), dp(6), dp(10), dp(8));

        //********** Footer with wooden buttons **********
        LinearLayout footer = new LinearLayout(context);
        footer.setOrientation(LinearLayout.HORIZONTAL);
        footer.setBackground(Theme.footer(context));
        footer.setPadding(dp(8), dp(6), dp(8), dp(6));

        // Settings on the left, Hide on the right, with the same button style.
        // The Settings button turns into Back while the settings page is open.
        final Button settingBtn = buttonText("Setelan");
        settingBtn.setBackground(Theme.pressButton(context));
        LinearLayout.LayoutParams paramsSetting = new LinearLayout.LayoutParams(0, dp(Theme.TARGET_DP), 1f);
        paramsSetting.setMargins(0, 0, dp(6), 0);
        settingBtn.setLayoutParams(paramsSetting);
        settingBtn.setOnClickListener(new View.OnClickListener() {
            CharSequence titleOrigin = "";
            CharSequence subTitleOrigin = "";
            int scrollOrigin;

            @Override
            public void onClick(View v) {
                try {
                    if (mSettings.getParent() == scrollView) {
                        scrollView.removeView(mSettings);
                        scrollView.addView(mods);
                        title.setText(titleOrigin);
                        subTitle.setText(subTitleOrigin);
                        settingBtn.setText("Setelan");
                        Transition.appear(scrollView, 1f);
                        // The feature list scroll position is restored after layout finishes.
                        scrollView.post(new Runnable() {
                            @Override
                            public void run() {
                                scrollView.scrollTo(0, scrollOrigin);
                            }
                        });
                    } else {
                        scrollOrigin = scrollView.getScrollY();
                        titleOrigin = title.getText();
                        subTitleOrigin = subTitle.getText();
                        scrollView.removeView(mods);
                        scrollView.addView(mSettings);
                        scrollView.scrollTo(0, 0);
                        // The main title stays "BOKU Menu"; only the subtitle
                        // marks the settings page.
                        subTitle.setText("Menu \\ Setelan");
                        settingBtn.setText("Kembali");
                        Transition.appear(scrollView, 1f);
                    }
                } catch (IllegalStateException ignored) {
                }
            }
        });
        footer.addView(settingBtn);

        final Button hideBtn = buttonText("Sembunyikan");
        hideBtn.setBackground(Theme.pressButton(context));
        hideBtn.setLayoutParams(new LinearLayout.LayoutParams(0, dp(Theme.TARGET_DP), 1f));
        hideBtn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(0);
                mExpanded.setVisibility(View.GONE);
                setAnimationIcon(false);
                Toast.makeText(view.getContext(), "Ikon disembunyikan. Ingat posisi ikon tersembunyi", Toast.LENGTH_LONG).show();
            }
        });
        footer.addView(hideBtn);

        //********** Adding view components **********
        mRootContainer.addView(mCollapsed);
        mRootContainer.addView(mExpanded);
        mCollapsed.addView(startimage);
        mCollapsed.addView(iconImage);
        mExpanded.addView(header);
        Theme.Zigzag zigTop = new Theme.Zigzag(context, Theme.HEADER, true);
        zigTop.setBackgroundColor(Theme.BODY);
        mExpanded.addView(zigTop, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(subTitle, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        scrollView.addView(mods);
        mExpanded.addView(scrollView);
        Theme.Zigzag zigBottom = new Theme.Zigzag(context, Theme.FOOTER, false);
        zigBottom.setBackgroundColor(Theme.BODY);
        mExpanded.addView(zigBottom, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(footer);

        applyIcon();
        Init(context, title, subTitle);
        Theme.applyFont(mExpanded);
        Theme.applyFont(mSettings);
        startTicker();
    }

    public void ShowMenu() {
        if (mRootContainer.getParent() == null)
            rootFrame.addView(mRootContainer);

        // The Menu's own handler (main looper) is reused, so onDestroy only
        // has to cancel everything at once and no checker keeps repeating
        // after the menu is destroyed.
        final Runnable loader = new Runnable() {
            boolean viewLoaded = false;

            @Override
            public void run() {
                if (destroyed)
                    return;
                //If the save preferences is enabled, it will check if game lib is loaded before starting menu
                //Comment the if-else code out except startService if you want to run the app and test preferences
                if (Preferences.loadPref && !IsGameLibLoaded() && !stopChecking) {
                    if (!viewLoaded) {
                        Category(mods, "Simpan preferensi aktif. Menunggu library game dimuat...\n\n"
                                + "Paksa muat menu mungkin tidak langsung menerapkan mod. Kamu perlu mengaktifkannya lagi");
                        Button(mods, -100, "Paksa muat menu");
                        Theme.applyFont(mods);
                        viewLoaded = true;
                    }
                    handlerTick.postDelayed(this, WAIT_LIB_MS);
                } else {
                    mods.removeAllViews();
                    String[] list = GetFeatureList();
                    if (list != null)
                        featureList(list, mods);
                    Theme.applyFont(mods);
                }
            }
        };
        handlerTick.postDelayed(loader, WAIT_INITIAL_MS);
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerWindowService() {
        restorePosition();
        //Variable to check later if the phone supports Draw over other apps permission
        int iparams = Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O ? 2038 : 2002;
        vmParams = new WindowManager.LayoutParams(
                WRAP_CONTENT,
                WRAP_CONTENT,
                iparams,
                8 | FLAG_TRANSLUCENT_STATUS,
                -3);
        //params = new WindowManager.LayoutParams(WindowManager.LayoutParams.LAST_APPLICATION_WINDOW, 8, -3);
        vmParams.gravity = 51;
        vmParams.x = POS_X;
        vmParams.y = POS_Y;

        mWindowManager = (WindowManager) getContext.getSystemService(Context.WINDOW_SERVICE);
        mWindowManager.addView(rootFrame, vmParams);

        overlayRequired = true;
        menuDialog.setOverlay(true);
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerActivity() {
        restorePosition();
        vmParams = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                POS_X,//initialX
                POS_Y,//initialy
                WindowManager.LayoutParams.TYPE_APPLICATION,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                        WindowManager.LayoutParams.FLAG_LAYOUT_IN_OVERSCAN |
                        WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN |
                        WindowManager.LayoutParams.FLAG_SPLIT_TOUCH,
                PixelFormat.TRANSPARENT
        );
        vmParams.gravity = 51;
        vmParams.x = POS_X;
        vmParams.y = POS_Y;

        mWindowManager = ((Activity) getContext).getWindowManager();
        mWindowManager.addView(rootFrame, vmParams);
    }

    /**
     * The last icon position is restored and clamped to the screen, so the icon
     * does not end up off-screen when the orientation or screen size changes.
     */
    private void restorePosition() {
        try {
            Preferences prefs = Preferences.with(getContext);
            DisplayMetrics dm = getContext.getResources().getDisplayMetrics();
            int maxX = Math.max(0, dm.widthPixels - dp(ICON_SIZE));
            int maxY = Math.max(0, dm.heightPixels - dp(ICON_SIZE));
            POS_X = Math.max(0, Math.min(maxX, prefs.readInt(KEY_X, POS_X)));
            POS_Y = Math.max(0, Math.min(maxY, prefs.readInt(KEY_Y, POS_Y)));
        } catch (Throwable t) {
            Log.d(TAG, "Saved position could not be read: " + t);
        }
    }

    private void storePosition() {
        try {
            Preferences prefs = Preferences.with(getContext);
            prefs.writeInt(KEY_X, vmParams.x);
            prefs.writeInt(KEY_Y, vmParams.y);
        } catch (Throwable t) {
            Log.d(TAG, "Position could not be saved: " + t);
        }
    }

    private View.OnTouchListener onTouchListener() {
        // Drag threshold follows screen density instead of a fixed 10 pixels.
        final int thresholdDrag = ViewConfiguration.get(getContext).getScaledTouchSlop();
        return new View.OnTouchListener() {
            private float initialTouchX, initialTouchY;
            private int initialX, initialY;
            private boolean moved;

            public boolean onTouch(View view, MotionEvent motionEvent) {
                int action = motionEvent.getActionMasked();
                switch (action) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = vmParams.x;
                        initialY = vmParams.y;
                        initialTouchX = motionEvent.getRawX();
                        initialTouchY = motionEvent.getRawY();
                        moved = false;
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        mExpanded.setAlpha(1f);
                        mCollapsed.setAlpha(ICON_ALPHA);
                        if (moved) {
                            storePosition();
                        } else if (action == MotionEvent.ACTION_UP && isViewCollapsed()) {
                            //The finger barely moved, so this is a tap: the menu opens.
                            //Older versions compared the signed delta (< 10), so
                            //left or upward drags counted as taps too.
                            openMenu();
                        }
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        float dx = motionEvent.getRawX() - initialTouchX;
                        float dy = motionEvent.getRawY() - initialTouchY;
                        if (!moved && Math.abs(dx) < thresholdDrag && Math.abs(dy) < thresholdDrag)
                            return true;
                        moved = true;
                        mExpanded.setAlpha(0.5f);
                        mCollapsed.setAlpha(0.5f);
                        //Calculate the X and Y coordinates of the view.
                        vmParams.x = initialX + (int) dx;
                        vmParams.y = initialY + (int) dy;
                        //Update the layout with new X & Y coordinate
                        if (mWindowManager != null && !destroyed) {
                            try {
                                mWindowManager.updateViewLayout(rootFrame, vmParams);
                            } catch (IllegalArgumentException e) {
                                Log.d(TAG, "Menu window already detached: " + e);
                            }
                        }
                        return true;
                    default:
                        return false;
                }
            }
        };
    }

    private static final String GROUP_PUBLIC = "Umum";
    private static final String GROUP_ICON = "Ikon melayang";

    /**
     * Settings page: features are grouped under small headings (General,
     * Floating icon) and end with a reset display button. Grouping is only used
     * when every entry has a feature number and native does not create its own
     * categories; otherwise the native list is used as is.
     */
    private void fillSettings(String[] list) {
        if (list == null)
            return;
        featureList(groupSetting(list), mSettings);

        if (!setterReload.isEmpty()) {
            final Button repeat = buttonText("Setel ulang tampilan");
            repeat.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    for (Runnable r : setterReload)
                        r.run();
                    Toast.makeText(getContext, "Tampilan disetel ulang", Toast.LENGTH_SHORT).show();
                }
            });
            LinearLayout box = boxButton(IconPattern.RELOAD, repeat);
            ((LinearLayout.LayoutParams) box.getLayoutParams()).topMargin = dp(12);
            mSettings.addView(box);
        }
    }

    private String[] groupSetting(String[] list) {
        List<String> general = new ArrayList<String>();
        List<String> icon = new ArrayList<String>();
        for (String entry : list) {
            Integer number = numberEntry(entry);
            if (number == null)
                return list;
            if (number == -4 || number == -5 || number == -6)
                icon.add(entry);
            else
                general.add(entry);
        }
        if (general.isEmpty() || icon.isEmpty())
            return list;

        List<String> result = new ArrayList<String>();
        result.add("Category_" + GROUP_PUBLIC);
        result.addAll(general);
        result.add("Category_" + GROUP_ICON);
        result.addAll(icon);
        return result.toArray(new String[0]);
    }

    /** Feature number at the start of the entry, or null when the entry is not numbered (category, text, link). */
    private Integer numberEntry(String entry) {
        String f = entry.replaceFirst("_True", "").replaceFirst("CollapseAdd_", "");
        String initial = f.split("_")[0];
        if (initial.length() > 0 && (TextUtils.isDigitsOnly(initial) || initial.matches("-[0-9]+"))) {
            try {
                return Integer.parseInt(initial);
            } catch (NumberFormatException e) {
                return null;
            }
        }
        return null;
    }

    private void featureList(String[] listFT, LinearLayout linearLayout) {
        //Currently looks messy right now. Let me know if you have improvements
        int featNum, subFeat = 0;
        LinearLayout baseLayout = linearLayout;

        for (int i = 0; i < listFT.length; i++) {
            // One broken entry from native must not bring down the whole menu.
            try {
                boolean switchedOn = false;
                //Log.i("featureList", listFT[i]);
                String feature = listFT[i];
                if (feature.contains("_True")) {
                    switchedOn = true;
                    feature = feature.replaceFirst("_True", "");
                }

                linearLayout = baseLayout;
                if (feature.contains("CollapseAdd_")) {
                    //if (collapse != null)
                    linearLayout = mCollapse;
                    feature = feature.replaceFirst("CollapseAdd_", "");
                }
                String[] str = feature.split("_");

                //Assign feature number
                if (TextUtils.isDigitsOnly(str[0]) || str[0].matches("-[0-9]*")) {
                    featNum = Integer.parseInt(str[0]);
                    feature = feature.replaceFirst(str[0] + "_", "");
                    subFeat++;
                } else {
                    //Subtract feature number. We don't want to count ButtonLink, Category, RichTextView and RichWebView
                    featNum = i - subFeat;
                }
                String[] strSplit = feature.split("_");
                switch (strSplit[0]) {
                    case "Toggle":
                        Switch(linearLayout, featNum, strSplit[1], switchedOn);
                        break;
                    case "SeekBar":
                        SeekBar(linearLayout, featNum, strSplit[1], Integer.parseInt(strSplit[2]),
                                Integer.parseInt(strSplit[3]),
                                strSplit.length > 4 ? Integer.parseInt(strSplit[4]) : Integer.parseInt(strSplit[2]),
                                strSplit.length > 5 ? Integer.parseInt(strSplit[5]) : 0);
                        break;
                    case "Button":
                        Button(linearLayout, featNum, strSplit[1]);
                        break;
                    case "ButtonOnOff":
                        ButtonOnOff(linearLayout, featNum, strSplit[1], switchedOn);
                        break;
                    case "Spinner":
                        TextView(linearLayout, strSplit[1]);
                        Spinner(linearLayout, featNum, strSplit[1], strSplit[2]);
                        break;
                    case "InputText":
                        InputText(linearLayout, featNum, strSplit[1]);
                        break;
                    case "InputValue":
                        if (strSplit.length == 3)
                            InputNum(linearLayout, featNum, strSplit[2], Integer.parseInt(strSplit[1]));
                        if (strSplit.length == 2)
                            InputNum(linearLayout, featNum, strSplit[1], 0);
                        break;
                    case "InputLValue":
                        if (strSplit.length == 3)
                            InputLNum(linearLayout, featNum, strSplit[2], Long.parseLong(strSplit[1]));
                        if (strSplit.length == 2)
                            InputLNum(linearLayout, featNum, strSplit[1], 0);
                        break;
                    case "CheckBox":
                        CheckBox(linearLayout, featNum, strSplit[1], switchedOn);
                        break;
                    case "RadioButton":
                        RadioButton(linearLayout, featNum, strSplit[1], strSplit[2]);
                        break;
                    case "Collapse":
                        Collapse(linearLayout, strSplit[1], switchedOn);
                        subFeat++;
                        break;
                    case "ButtonLink":
                        subFeat++;
                        ButtonLink(linearLayout, strSplit[1], strSplit[2]);
                        break;
                    case "Category":
                        subFeat++;
                        Category(linearLayout, strSplit[1]);
                        break;
                    case "PlayerList":
                        subFeat++;
                        PlayerList(linearLayout, strSplit[1]);
                        break;
                    case "LivePos":
                        subFeat++;
                        RowPosition(linearLayout, strSplit[1]);
                        break;
                    case "RichTextView":
                        subFeat++;
                        TextView(linearLayout, strSplit[1]);
                        break;
                    case "RichWebView":
                        subFeat++;
                        WebTextView(linearLayout, strSplit[1]);
                        break;
                }
            } catch (RuntimeException e) {
                Log.e(TAG, "Feature entry skipped: " + listFT[i], e);
            }
        }
    }

    private void Switch(LinearLayout linLayout, final int featNum, final String featName, boolean swiOn) {
        final AnimatedSwitch switchR = new AnimatedSwitch(getContext);
        switchR.setContentDescription(featName);
        switchR.setChecked(Preferences.loadPrefBool(featName, featNum, swiOn));
        // The switch reports a change a few frames after the tap, so the guard
        // is cleared by the next report instead of right after the dialog.
        final boolean[] dumpConfirmed = {false};
        switchR.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton compoundButton, boolean bool) {
                if (featNum == FEATURE_AUTO_DUMP && !dumpConfirmed[0]) {
                    if (bool) {
                        switchR.setChecked(false);
                        confirmDump(switchR, dumpConfirmed);
                    } else {
                        Preferences.changeFeatureBool(featName, featNum, false);
                    }
                    return;
                }
                dumpConfirmed[0] = false;
                Preferences.changeFeatureBool(featName, featNum, bool);
                if (featNum == -1) {
                    Preferences.with(switchR.getContext()).writeBoolean(-1, bool);
                    if (!bool) Preferences.with(switchR.getContext()).clear(); //Clear perferences if switched off
                }
            }
        });

        TextView label = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), label, switchR);
        row.setBackground(Theme.rowPress(getContext));
        row.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                switchR.toggle();
            }
        });
        installExplanation(row, featNum, featName);
        installExplanation(switchR, featNum, featName);
        linLayout.addView(row);
    }

    private void SeekBar(LinearLayout linLayout, final int featNum, final String featName,
                         final int min, final int max, final int def, final int decimal) {
        // 0 is a valid value for gravity, so the default is only used when
        // this feature was never saved.
        boolean saved = Preferences.alreadySaved(featName, featNum);
        int initialValue = saved ? Preferences.loadPrefInt(featName, featNum) : def;
        if (initialValue < min)
            initialValue = min;
        if (initialValue > max)
            initialValue = max;
        if (!saved)
            Preferences.changeFeatureInt(featName, featNum, initialValue);

        // Opacity and icon size are applied directly when the slider is read.
        if (featNum == -4)
            ICON_ALPHA = initialValue / 100f;
        else if (featNum == -5)
            ICON_SIZE = initialValue;

        final String unit = featureUnit(featNum);

        LinearLayout column = new LinearLayout(getContext);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setGravity(Gravity.CENTER_VERTICAL);

        // Title row: feature name on the left, value on the right. Tap the value
        // to return the slider to its default.
        LinearLayout titleRow = new LinearLayout(getContext);
        titleRow.setOrientation(LinearLayout.HORIZONTAL);
        titleRow.setGravity(Gravity.CENTER_VERTICAL);

        final TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        titleRow.addView(title, new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));

        final TextView valueText = Theme.text(getContext, textValue(initialValue, decimal) + unit,
                Theme.DESC_SP, Theme.TITLE_TEXT);
        valueText.setGravity(Gravity.CENTER);
        valueText.setSingleLine(true);
        valueText.setMinWidth(dp(44));
        valueText.setPadding(dp(8), dp(2), dp(8), dp(2));
        valueText.setBackground(Theme.smallButton(getContext));
        valueText.setContentDescription(featName + ", ketuk untuk reset");
        titleRow.addView(valueText, new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));

        final SeekBar seekBar = new SeekBar(getContext);
        seekBar.setProgressDrawable(Theme.seekbarProgress(getContext));
        seekBar.setThumb(Theme.seekbarThumb(getContext));
        seekBar.setThumbOffset(0);
        // Half-thumb horizontal padding so the thumb is not clipped at the ends.
        seekBar.setPadding(dp(5), 0, dp(5), 0);
        // The range is shifted to zero so setMin, which only exists on Oreo and
        // up, is not required.
        seekBar.setMax(max - min);
        seekBar.setProgress(initialValue - min);
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            public void onStopTrackingTouch(SeekBar seekBar) {
            }

            public void onProgressChanged(SeekBar seekBar, int i, boolean z) {
                int value = i + min;
                if (value < min)
                    value = min;
                if (value > max)
                    value = max;
                Preferences.changeFeatureInt(featName, featNum, value);
                valueText.setText(textValue(value, decimal) + unit);
                if (featNum == -4 || featNum == -5) {
                    if (featNum == -4)
                        ICON_ALPHA = value / 100f;
                    else
                        ICON_SIZE = value;
                    applyIcon();
                }
            }
        });

        valueText.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Goes through the same drag path, so native also receives the default value.
                seekBar.setProgress(Math.max(0, Math.min(max, def) - min));
            }
        });

        if (featNum == -4 || featNum == -5) {
            setterReload.add(new Runnable() {
                @Override
                public void run() {
                    seekBar.setProgress(Math.max(0, Math.min(max, def) - min));
                }
            });
        }

        column.addView(titleRow, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        column.addView(seekBar, new LinearLayout.LayoutParams(MATCH_PARENT, dp(Theme.SLIDER_HEIGHT_DP)));
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), column, null);
        installExplanation(row, featNum, featName);
        linLayout.addView(row);
    }

    /** Slider value unit: speed multiplier, opacity percent, or icon dp. */
    private String featureUnit(int featNum) {
        if (featNum == 23 || featNum == 24)
            return "x";
        if (featNum == -4)
            return "%";
        if (featNum == -5)
            return "dp";
        return "";
    }

    /** The slider value is shown with the number of decimals written in the feature list. */
    private String textValue(int value, int decimal) {
        if (decimal <= 0)
            return String.valueOf(value);
        return String.format(Locale.US, "%." + decimal + "f", value / 10f);
    }

    private void Button(LinearLayout linLayout, final int featNum, final String featName) {
        final Button button = buttonText(Html.fromHtml(featName));
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                switch (featNum) {
                    case -100:
                        stopChecking = true;
                        break;
                }
                Preferences.changeFeatureInt(featName, featNum, 0);
            }
        });

        LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        installExplanation(box, featNum, featName);
        installExplanation(button, featNum, featName);
        linLayout.addView(box);
    }

    private void ButtonLink(LinearLayout linLayout, final String featName, final String url) {
        final Button button = buttonText(Html.fromHtml(featName));
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                try {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setData(Uri.parse(url));
                    getContext.startActivity(intent);
                } catch (RuntimeException e) {
                    // No app can open the link.
                    Toast.makeText(getContext, "Tautan tidak bisa dibuka", Toast.LENGTH_SHORT).show();
                }
            }
        });
        final LinearLayout box = boxButton(IconPattern.TELEGRAM, button);
        installExplanation(box, -100, featName);
        installExplanation(button, -100, featName);
        linLayout.addView(box);
    }

    private void ButtonOnOff(LinearLayout linLayout, final int featNum, String featName, boolean switchedOn) {
        final Button button = buttonText("");
        final String finalfeatName = featName.replace("OnOff_", "");
        boolean isOn = Preferences.loadPrefBool(featName, featNum, switchedOn);
        final LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        if (isOn) {
            button.setText(Html.fromHtml(finalfeatName + ": ON"));
            box.setBackground(Theme.activeButton(getContext));
            isOn = false;
        } else {
            button.setText(Html.fromHtml(finalfeatName + ": OFF"));
            box.setBackground(Theme.offButton(getContext));
            isOn = true;
        }
        final boolean finalIsOn = isOn;
        button.setOnClickListener(new View.OnClickListener() {
            boolean isOn = finalIsOn;

            public void onClick(View v) {
                Preferences.changeFeatureBool(finalfeatName, featNum, isOn);
                //Log.d(TAG, finalfeatName + " " + featNum + " " + isActive2);
                if (isOn) {
                    button.setText(Html.fromHtml(finalfeatName + ": ON"));
                    box.setBackground(Theme.activeButton(getContext));
                    isOn = false;
                } else {
                    button.setText(Html.fromHtml(finalfeatName + ": OFF"));
                    box.setBackground(Theme.offButton(getContext));
                    isOn = true;
                }
            }
        });
        installExplanation(box, featNum, finalfeatName);
        installExplanation(button, featNum, finalfeatName);
        linLayout.addView(box);
    }

    private void Spinner(LinearLayout linLayout, final int featNum, final String featName, final String list) {
        final List<String> lists = new ArrayList<>(Arrays.asList(list.split(",")));

        final Spinner spinner = new Spinner(getContext, Spinner.MODE_DROPDOWN);
        ArrayAdapter<String> aa = new ArrayAdapter<String>(getContext,
                android.R.layout.simple_spinner_item, lists) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                TextView view = (TextView) super.getView(position, convertView, parent);
                Theme.applyFont(view);
                view.setTextColor(Theme.TEXT);
                view.setTextSize(Theme.ROW_SP);
                return view;
            }

            @Override
            public View getDropDownView(int position, View convertView, ViewGroup parent) {
                TextView view = (TextView) super.getDropDownView(position, convertView, parent);
                Theme.applyFont(view);
                view.setBackgroundColor(Theme.BODY);
                view.setTextColor(Theme.TEXT);
                view.setTextSize(Theme.ROW_SP);
                view.setPadding(dp(10), dp(8), dp(10), dp(8));
                return view;
            }
        };
        aa.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(aa);
        int selected = Preferences.loadPrefInt(featName, featNum);
        spinner.setSelection(selected >= 0 && selected < lists.size() ? selected : 0);
        if (spinner.getBackground() != null)
            spinner.getBackground().setColorFilter(Theme.TEXT, PorterDuff.Mode.SRC_ATOP);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parentView, View selectedItemView, int position, long id) {
                Preferences.changeFeatureInt(spinner.getSelectedItem().toString(), featNum, position);
                View child = parentView.getChildAt(0);
                if (child instanceof TextView)
                    ((TextView) child).setTextColor(Theme.TEXT);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), spinner, null);
        installExplanation(row, featNum, featName);
        installExplanation(spinner, featNum, featName);
        linLayout.addView(row);
    }

    private void InputNum(LinearLayout linLayout, int featNum, String featName, int maxValue) {
        InputNumber(linLayout, featNum, featName, maxValue, false);
    }

    private void InputLNum(LinearLayout linLayout, int featNum, String featName, long maxValue) {
        InputNumber(linLayout, featNum, featName, maxValue, true);
    }

    /** Number button: Int and Long share one code, only the digit limit and save path differ. */
    private void InputNumber(LinearLayout linLayout, final int featNum, final String featName,
                            final long maxValue, final boolean length) {
        final Button button = buttonText("");
        long num = length ? Preferences.loadPrefLong(featName, featNum)
                : Preferences.loadPrefInt(featName, featNum);
        button.setText(Html.fromHtml(featName + ": " + num));
        final String hint = maxValue != 0 ? "Nilai maksimum: " + maxValue : null;
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                menuDialog.input("Masukkan angka", hint, null, true, length ? 20 : 10,
                        new MenuDialog.Receiver() {
                            @Override
                            public void onText(String text) {
                                long value;
                                try {
                                    String input = text.isEmpty() ? "0" : text;
                                    value = length ? Long.parseLong(input) : Integer.parseInt(input);
                                    if (maxValue != 0 && value >= maxValue)
                                        value = maxValue;
                                } catch (NumberFormatException ex) {
                                    if (maxValue != 0)
                                        value = maxValue;
                                    else
                                        value = length ? Long.MAX_VALUE : Integer.MAX_VALUE;
                                }

                                button.setText(Html.fromHtml(featName + ": " + value));
                                if (length)
                                    Preferences.changeFeatureLong(featName, featNum, value);
                                else
                                    Preferences.changeFeatureInt(featName, featNum, (int) value);
                            }
                        }, null, null);
            }
        });

        final LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        installExplanation(box, featNum, featName);
        installExplanation(button, featNum, featName);
        linLayout.addView(box);
    }

    private void InputText(LinearLayout linLayout, final int featNum, final String featName) {
        String string = Preferences.loadPrefString(featName, featNum);
        if (featNum == -6) {
            InputImage(linLayout, featNum, featName, string);
            return;
        }

        final Button button = buttonText("");
        button.setText(Html.fromHtml(featName + ": " + string));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                menuDialog.input("Masukkan teks", null, null, false, 0, new MenuDialog.Receiver() {
                    @Override
                    public void onText(String str) {
                        button.setText(Html.fromHtml(featName + ": " + str));
                        Preferences.changeFeatureString(featName, featNum, str);
                    }
                }, null, null);
            }
        });

        final LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        installExplanation(box, featNum, featName);
        installExplanation(button, featNum, featName);
        linLayout.addView(box);
    }

    /**
     * Icon image setting: two-level row (name on top, current source below in
     * a dim color). Tap the row to change it, or pick Default in the dialog to
     * return to the built-in icon.
     */
    private void InputImage(LinearLayout linLayout, final int featNum, final String featName,
                             String initial) {
        iconSource = initial == null ? "" : initial;

        LinearLayout column = new LinearLayout(getContext);
        column.setOrientation(LinearLayout.VERTICAL);

        TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        column.addView(title);

        final TextView value = Theme.text(getContext, summarySource(iconSource), Theme.DESC_SP,
                Theme.MUTED);
        value.setSingleLine(true);
        value.setEllipsize(TextUtils.TruncateAt.MIDDLE);
        column.addView(value);

        final LinearLayout row = row(IconPattern.pattern(featNum, featName), column, null);
        row.setBackground(Theme.rowPress(getContext));
        installExplanation(row, featNum, featName);
        row.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                menuDialog.input("Sumber gambar ikon", "URL gambar atau GIF, kosongkan untuk bawaan",
                        iconSource, false, 0, new MenuDialog.Receiver() {
                            @Override
                            public void onText(String text) {
                                setIconSource(text, featNum, featName, value);
                            }
                        }, "Bawaan", new Runnable() {
                            @Override
                            public void run() {
                                setIconSource("", featNum, featName, value);
                            }
                        });
            }
        });

        setterReload.add(new Runnable() {
            @Override
            public void run() {
                if (iconSource.length() > 0)
                    setIconSource("", featNum, featName, value);
            }
        });

        linLayout.addView(row);
    }

    private void setIconSource(String source, int featNum, String featName, TextView value) {
        source = source == null ? "" : source.trim();
        iconSource = source;
        value.setText(summarySource(source));
        applyIcon();
        Preferences.changeFeatureString(featName, featNum, source);
    }

    private String summarySource(String source) {
        return source.length() == 0 ? "Bawaan" : source;
    }

    private void CheckBox(LinearLayout linLayout, final int featNum, final String featName, boolean switchedOn) {
        final CheckBox checkBox = new CheckBox(getContext);
        checkBox.setText(featName);
        checkBox.setTextColor(Theme.TEXT);
        checkBox.setTextSize(Theme.ROW_SP);
        checkBox.setTypeface(Theme.font(getContext));
        checkBox.setButtonTintList(ColorStateList.valueOf(Theme.ICON_BOX));
        checkBox.setChecked(Preferences.loadPrefBool(featName, featNum, switchedOn));
        checkBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                Preferences.changeFeatureBool(featName, featNum, isChecked);
            }
        });
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), checkBox, null);
        installExplanation(row, featNum, featName);
        installExplanation(checkBox, featNum, featName);
        linLayout.addView(row);
    }

    @SuppressLint("SetTextI18n")
    private void RadioButton(LinearLayout linLayout, final int featNum, String featName, final String list) {
        //Credit: LoraZalora
        final List<String> lists = new ArrayList<>(Arrays.asList(list.split(",")));

        final TextView textView = Theme.text(getContext, featName + ":", Theme.ROW_SP, Theme.TEXT);

        final RadioGroup radioGroup = new RadioGroup(getContext);
        radioGroup.setPadding(dp(10), dp(6), dp(10), dp(6));
        radioGroup.setOrientation(LinearLayout.VERTICAL);
        radioGroup.setBackground(Theme.row(getContext));
        LinearLayout.LayoutParams groupParams = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        groupParams.setMargins(0, dp(3), 0, dp(3));
        radioGroup.setLayoutParams(groupParams);
        radioGroup.addView(textView);

        for (int i = 0; i < lists.size(); i++) {
            final RadioButton Radioo = new RadioButton(getContext);
            final String finalfeatName = featName, radioName = lists.get(i);
            View.OnClickListener first_radio_listener = new View.OnClickListener() {
                public void onClick(View v) {
                    textView.setText(Html.fromHtml(finalfeatName + ": <font color='"
                            + Theme.colorHex(Theme.ICON_BOX) + "'>" + radioName));
                    Preferences.changeFeatureInt(finalfeatName, featNum, radioGroup.indexOfChild(Radioo));
                }
            };
            Radioo.setText(lists.get(i));
            Radioo.setTextColor(Theme.TEXT);
            Radioo.setTextSize(Theme.ROW_SP);
            Radioo.setTypeface(Theme.font(getContext));
            Radioo.setButtonTintList(ColorStateList.valueOf(Theme.ICON_BOX));
            Radioo.setOnClickListener(first_radio_listener);
            installExplanation(Radioo, featNum, featName);
            radioGroup.addView(Radioo);
        }

        int index = Preferences.loadPrefInt(featName, featNum);
        if (index > 0 && index <= lists.size()) { //Preventing it to get an index less than 1. below 1 = null = crash
            textView.setText(Html.fromHtml(featName + ": <font color='" + Theme.colorHex(Theme.ICON_BOX)
                    + "'>" + lists.get(index - 1)));
            ((RadioButton) radioGroup.getChildAt(index)).setChecked(true);
        }
        installExplanation(radioGroup, featNum, featName);
        linLayout.addView(radioGroup);
    }

    private boolean readOpen(String key, boolean fallback) {
        try {
            return Preferences.with(getContext).readBoolean(key, fallback);
        } catch (Throwable t) {
            return fallback;
        }
    }

    private void saveOpen(String key, boolean opened) {
        try {
            Preferences.with(getContext).writeBoolean(key, opened);
        } catch (Throwable t) {
            Log.d(TAG, "Section state could not be saved: " + t);
        }
    }

    /**
     * Fold-out section. Open/close is animated and the state is stored per
     * section title, so the last open order is restored when the menu is
     * opened again.
     */
    @SuppressLint("SetTextI18n")
    private void Collapse(LinearLayout linLayout, final String text, final boolean expanded) {
        final String key = KEY_FOLD + text;
        final boolean initialOpen = readOpen(key, expanded);

        LinearLayout section = new LinearLayout(getContext);
        section.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams sectionParams = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        sectionParams.setMargins(0, dp(10), 0, 0);
        section.setLayoutParams(sectionParams);

        LinearLayout header = new LinearLayout(getContext);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setBackground(Theme.sectionHeaderPress(getContext));
        header.setPadding(dp(14), 0, dp(10), 0);
        header.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(Theme.TARGET_DP)));

        final TextView title = Theme.text(getContext, text, Theme.SECTION_SP, Theme.TITLE_TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        header.addView(title, titleParams);

        final Theme.Icon arrow = Theme.Icon.plain(getContext,
                initialOpen ? Theme.Icon.ARROW_DOWN : Theme.Icon.ARROW_RIGHT,
                Theme.TITLE_TEXT, 20);
        header.addView(arrow, new LinearLayout.LayoutParams(dp(20), dp(20)));

        final LinearLayout content = new LinearLayout(getContext);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(0, dp(4), 0, dp(2));
        content.setBackground(Theme.sectionBody(getContext));
        mCollapse = content;

        final LinearLayout wrapper = new LinearLayout(getContext);
        wrapper.setOrientation(LinearLayout.VERTICAL);
        wrapper.addView(content);
        wrapper.setVisibility(initialOpen ? View.VISIBLE : View.GONE);

        header.setContentDescription(text);
        header.setOnClickListener(new View.OnClickListener() {
            boolean opened = initialOpen;

            @Override
            public void onClick(View v) {
                opened = !opened;
                Transition.fold(wrapper, opened);
                arrow.setKind(opened ? Theme.Icon.ARROW_DOWN : Theme.Icon.ARROW_RIGHT);
                saveOpen(key, opened);
                // Newly visible content is refreshed immediately, without waiting for the tick.
                if (opened)
                    startMonitor();
            }
        });

        section.addView(header);
        section.addView(wrapper);
        linLayout.addView(section);
    }

    private static final class CardPlayer {
        final String name;
        final String role;
        final LinearLayout view;

        CardPlayer(String name, String role, LinearLayout view) {
            this.name = name;
            this.role = role;
            this.view = view;
        }
    }

    /** State of one player list: the cards that already exist and their last content. */
    private static final class ContentList {
        final Map<String, CardPlayer> card = new LinkedHashMap<String, CardPlayer>();
        String data;
        TextView empty;
    }

    @SuppressLint("SetTextI18n")
    private void PlayerList(LinearLayout linLayout, final String featName) {
        LinearLayout container = new LinearLayout(getContext);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(dp(2), dp(2), dp(2), dp(4));

        LinearLayout titleRow = new LinearLayout(getContext);
        titleRow.setOrientation(LinearLayout.HORIZONTAL);
        titleRow.setGravity(Gravity.CENTER_VERTICAL);
        titleRow.setBackground(Theme.row(getContext));
        titleRow.setPadding(0, 0, dp(5), 0);
        titleRow.setMinimumHeight(dp(Theme.TARGET_DP));
        LinearLayout.LayoutParams paramsTitle = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsTitle.setMargins(0, dp(4), 0, dp(4));
        titleRow.setLayoutParams(paramsTitle);

        titleRow.addView(new FeatureIcon(getContext, IconPattern.PLAYER, true),
                new LinearLayout.LayoutParams(dp(FeatureIcon.WIDTH_DP), MATCH_PARENT));

        // Feature name with the player count below it, so the reload button on
        // the right has room for its label.
        LinearLayout columnTitle = new LinearLayout(getContext);
        columnTitle.setOrientation(LinearLayout.VERTICAL);
        columnTitle.setGravity(Gravity.CENTER_VERTICAL);
        columnTitle.setPadding(0, dp(4), 0, dp(4));
        TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        columnTitle.addView(title, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        final TextView count = Theme.text(getContext, "", Theme.DESC_SP, Theme.MUTED);
        count.setSingleLine(true);
        columnTitle.addView(count, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        LinearLayout.LayoutParams paramsColumn = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        paramsColumn.setMargins(dp(12), 0, dp(8), 0);
        titleRow.addView(columnTitle, paramsColumn);
        container.addView(titleRow);

        TextView hint = Theme.text(getContext,
                "Ketuk kartu untuk memilih. Ketuk Kick untuk sementara, tahan untuk permanen. "
                        + "Kick host ada di kartu host.",
                Theme.DESC_SP, Theme.MUTED);
        hint.setPadding(dp(4), dp(2), dp(4), dp(6));
        container.addView(hint);

        final LinearLayout list = new LinearLayout(getContext);
        list.setOrientation(LinearLayout.VERTICAL);
        container.addView(list);

        // The list is already re-read every second. This button reads at once
        // and gives a clear sign (loading, then "Diperbarui") that it is fresh.
        Button kickAll = smallButton("Kick semua", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                confirm("Kick semua", "Kick semua tamu secara permanen? Host dan kamu "
                                + "tidak ikut.", "Kick semua", new Runnable() {
                    @Override
                    public void run() {
                        boolean accepted;
                        try {
                            accepted = KickAll();
                        } catch (Throwable t) {
                            accepted = false;
                        }
                        if (!accepted)
                            Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                    Toast.LENGTH_SHORT).show();
                    }
                });
            }
        }, false);
        kickAll.setContentDescription("Kick semua tamu");
        titleRow.addView(kickAll);

        ReloadButton reload = new ReloadButton(getContext, IconPattern.RELOAD, new Runnable() {
            @Override
            public void run() {
                ContentList content = (ContentList) list.getTag();
                if (content != null)
                    content.data = null;
                fillPlayerList(list, count);
            }
        });
        titleRow.addView(reload, new LinearLayout.LayoutParams(WRAP_CONTENT, dp(Theme.TARGET_DP)));
        installExplanation(titleRow, 0, featName);

        linLayout.addView(container);
        fillPlayerList(list, count);

        // The list follows the Menu tick: re-read every second only while this
        // section is actually visible, and immediately when the menu or section
        // opens. Changes (join, leave, name/role change) show without a reload
        // button.
        registerMonitor(new Runnable() {
            @Override
            public void run() {
                if (list.isShown())
                    fillPlayerList(list, count);
            }
        });
    }

    @SuppressLint("SetTextI18n")
    private void fillPlayerList(LinearLayout list, TextView count) {
        String data = getPlayerList();

        ContentList content = (ContentList) list.getTag();
        if (content == null) {
            content = new ContentList();
            list.removeAllViews();
            list.setTag(content);
        }
        if (data.equals(content.data))
            return;
        content.data = data;

        Map<String, String[]> fresh = new LinkedHashMap<String, String[]>();
        for (String row : data.split("\n")) {
            String[] part = row.split("\t", 3);
            if (part.length < 3 || part[0].length() == 0 || part[1].length() == 0)
                continue;
            fresh.put(part[1], new String[]{part[0], part[2]});
        }

        // Players who left are removed first. When the one who left is the
        // current choice, the choice is released so it does not point at a
        // missing key.
        Iterator<Map.Entry<String, CardPlayer>> it = content.card.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, CardPlayer> entry = it.next();
            if (!fresh.containsKey(entry.getKey())) {
                list.removeView(entry.getValue().view);
                if (entry.getKey().equals(keySelected))
                    keySelected = null;
                it.remove();
            }
        }

        if (content.empty != null) {
            list.removeView(content.empty);
            content.empty = null;
        }

        int index = 0;
        for (Map.Entry<String, String[]> entry : fresh.entrySet()) {
            String key = entry.getKey();
            String name = entry.getValue()[0];
            String role = entry.getValue()[1];
            CardPlayer card = content.card.get(key);
            if (card != null && card.name.equals(name) && card.role.equals(role)) {
                // Same content: the card is reused, only its order is fixed up.
                if (list.getChildAt(index) != card.view) {
                    list.removeView(card.view);
                    list.addView(card.view, Math.min(index, list.getChildCount()));
                }
            } else {
                LinearLayout view = playerRow(content, key, name, role);
                if (card != null)
                    list.removeView(card.view);
                list.addView(view, Math.min(index, list.getChildCount()));
                content.card.put(key, new CardPlayer(name, role, view));
            }
            index++;
        }

        count.setText(index == 0 ? "" : index + " pemain");
        if (index == 0) {
            TextView empty = Theme.text(getContext, "Tidak ada pemain di room ini",
                    Theme.DESC_SP, Theme.MUTED);
            empty.setGravity(Gravity.CENTER);
            empty.setPadding(0, dp(14), 0, dp(14));
            list.addView(empty);
            content.empty = empty;
        }
    }

    private String getPlayerList() {
        try {
            String data = PlayerList();
            return data == null ? "" : data;
        } catch (Throwable t) {
            return "";
        }
    }

    // One player becomes one card: name and role badge on top, actions below.
    // The card itself has no action because copying or kicking yourself is
    // useless, and kick only exists for guests.
    private LinearLayout playerRow(final ContentList content, final String key,
                                     final String name, String role) {
        LinearLayout row = new LinearLayout(getContext);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(dp(10), dp(8), dp(10), dp(8));
        LinearLayout.LayoutParams paramsRow =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsRow.setMargins(0, 0, 0, dp(6));
        row.setLayoutParams(paramsRow);
        row.setBackground(key.equals(keySelected)
                ? Theme.cardSelected(getContext)
                : Theme.card(getContext));

        LinearLayout header = new LinearLayout(getContext);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);

        TextView label = Theme.text(getContext, name, Theme.ROW_SP, Theme.TEXT);
        label.setSingleLine(true);
        label.setEllipsize(TextUtils.TruncateAt.END);
        label.setLayoutParams(new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));
        header.addView(label);
        header.addView(badgeRole(role));
        row.addView(header);

        row.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                keySelected = key.equals(keySelected) ? null : key;
                refreshChoice(content);
            }
        });

        if ("kamu".equals(role))
            return row;

        LinearLayout action = new LinearLayout(getContext);
        action.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams paramsAction =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsAction.setMargins(0, dp(6), 0, 0);
        action.setLayoutParams(paramsAction);
        action.addView(smallButton("Copy skin", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean accepted;
                try {
                    accepted = CopyPlayerSkin(key, name);
                } catch (Throwable t) {
                    accepted = false;
                }
                // The real result is reported by a toast from the game thread;
                // here only the queue can be rejected immediately.
                if (!accepted)
                    Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                            Toast.LENGTH_SHORT).show();
            }
        }));
        if ("host".equals(role)) {
            action.addView(smallButton("Kick host", new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    confirmKickHost(name);
                }
            }));
        } else if ("tamu".equals(role)) {
            Button kick = smallButton("Kick", new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    boolean accepted;
                    try {
                        accepted = KickPlayer(key, name, false);
                    } catch (Throwable t) {
                        accepted = false;
                    }
                    if (!accepted)
                        Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                Toast.LENGTH_SHORT).show();
                }
            });
            kick.setContentDescription("Kick sementara " + name);
            kick.setOnLongClickListener(new View.OnLongClickListener() {
                @Override
                public boolean onLongClick(View v) {
                    boolean accepted;
                    try {
                        accepted = KickPlayer(key, name, true);
                    } catch (Throwable t) {
                        accepted = false;
                    }
                    Toast.makeText(Menu.this.getContext, accepted
                                    ? "Kick permanen diantrekan"
                                    : "Antrean penuh, coba lagi",
                            Toast.LENGTH_SHORT).show();
                    return true;
                }
            });
            action.addView(kick);
        }
        row.addView(action);
        return row;
    }

    private void confirmKickHost(final String name) {
        confirm("Kick host", "Paket kick akan dikirim berulang ke host. Host bisa lag lalu "
                + "keluar dari room, dan room akan ditutup. Lanjutkan?", "Kick host",
                new Runnable() {
                    @Override
                    public void run() {
                        boolean accepted;
                        try {
                            accepted = KickHost(name);
                        } catch (Throwable t) {
                            accepted = false;
                        }
                        if (!accepted)
                            Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                    Toast.LENGTH_SHORT).show();
                    }
                });
    }

    private void confirm(String title, String message, String label, final Runnable action) {
        menuDialog.display(title, message, null, null,
                new MenuDialog.Action(label, action), null,
                new MenuDialog.Action("Batal", null), true);
    }

    /** Warning shown before auto dump is switched on; the switch stays off until accepted. */
    private void confirmDump(final AnimatedSwitch target, final boolean[] confirmed) {
        confirm("Skin & Portrait Auto Dump",
                "Setiap tamu yang masuk akan disimpan otomatis: skin ke slot model kosong dan "
                        + "potret ke slot potret kosong. Room yang ramai berarti banyak berkas "
                        + "baru, jadi game bisa terasa lebih berat saat tamu bergabung. Aktifkan?",
                "Aktifkan", new Runnable() {
                    @Override
                    public void run() {
                        confirmed[0] = true;
                        target.setChecked(true);
                    }
                });
    }

    private void refreshChoice(ContentList content) {
        for (Map.Entry<String, CardPlayer> entry : content.card.entrySet()) {
            entry.getValue().view.setBackground(entry.getKey().equals(keySelected)
                    ? Theme.cardSelected(getContext)
                    : Theme.card(getContext));
        }
    }

    private Button smallButton(String text, View.OnClickListener listener) {
        return smallButton(text, listener, true);
    }

    private Button smallButton(String text, View.OnClickListener listener, boolean weight) {
        Button button = new Button(getContext);
        button.setText(text);
        button.setTextSize(13f);
        button.setAllCaps(false);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTypeface(Theme.font(getContext));
        // 48dp touch height, the visible box stays as small as before.
        button.setBackground(Theme.smallButton(getContext, BUTTON_SMALL_INSET_DP));
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(dp(Theme.TARGET_DP));
        button.setMinimumHeight(dp(Theme.TARGET_DP));
        button.setPadding(dp(8), 0, dp(8), 0);
        button.setStateListAnimator(null);
        button.setElevation(0f);
        LinearLayout.LayoutParams params = weight
                ? new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f)
                : new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        params.setMargins(0, 0, dp(6), 0);
        button.setLayoutParams(params);
        button.setOnClickListener(listener);
        return button;
    }

    @SuppressLint("SetTextI18n")
    private TextView badgeRole(String role) {
        int color;
        if ("host".equals(role))
            color = Theme.ACCENT;
        else if ("kamu".equals(role))
            color = Theme.ICON_BOX;
        else
            color = Theme.TRACK;

        int textColor = color == Theme.ACCENT ? Theme.TEXT : Theme.ACTIVE_BORDER;
        TextView badge = Theme.text(getContext, role, 12f, textColor);
        badge.setGravity(Gravity.CENTER);
        badge.setPadding(dp(6), dp(2), dp(6), dp(2));
        badge.setBackground(Theme.badge(getContext, color));
        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        params.setMargins(dp(6), 0, 0, 0);
        badge.setLayoutParams(params);
        return badge;
    }

    private void Category(LinearLayout linLayout, String text) {
        if (linLayout == mSettings) {
            // Settings use a light group heading, not the brown ribbon.
            TextView title = Theme.text(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.MUTED);
            title.setPadding(dp(4), dp(12), dp(4), dp(2));
            linLayout.addView(title);
            return;
        }
        LinearLayout row = new LinearLayout(getContext);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setBackground(Theme.sectionHeader(getContext));
        row.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(10), 0, dp(2));
        row.setLayoutParams(params);

        TextView textView = Theme.text(getContext, Html.fromHtml(text), Theme.SECTION_SP, Theme.TITLE_TEXT);
        textView.setGravity(Gravity.CENTER);
        textView.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        row.addView(textView);
        linLayout.addView(row);
    }

    private void TextView(LinearLayout linLayout, String text) {
        TextView textView = Theme.text(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.MUTED);
        textView.setPadding(dp(10), dp(4), dp(10), dp(2));
        linLayout.addView(textView);
    }

    /**
     * RichWebView no longer uses WebView. Basic tags (b, i, u, font, a, br) are
     * rendered by a plain TextView, and links can still be tapped.
     */
    private void WebTextView(LinearLayout linLayout, String text) {
        TextView textView = Theme.text(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.TEXT);
        textView.setMovementMethod(LinkMovementMethod.getInstance());
        textView.setLinkTextColor(Theme.ICON_BOX);
        textView.setPadding(dp(10), dp(4), dp(10), dp(4));
        linLayout.addView(textView);
    }

    /**
     * Control row: icon box attached to the left edge (only the left corners
     * are rounded), label in the middle, and control on the right. Minimum
     * height 48dp so the whole row is comfortable to touch.
     */
    private LinearLayout row(String[] pattern, View content, View control) {
        LinearLayout row = new LinearLayout(getContext);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setBackground(Theme.row(getContext));
        row.setMinimumHeight(dp(Theme.TARGET_DP));
        row.setPadding(0, 0, dp(8), 0);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(4), 0, dp(4));
        row.setLayoutParams(params);

        // The icon box is fully attached to the left edge and row height, without margin.
        row.addView(new FeatureIcon(getContext, pattern, true),
                new LinearLayout.LayoutParams(dp(FeatureIcon.WIDTH_DP), MATCH_PARENT));

        LinearLayout.LayoutParams fillParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        fillParams.setMargins(dp(12), dp(6), dp(8), dp(6));
        row.addView(content, fillParams);

        if (control != null)
            row.addView(control, new LinearLayout.LayoutParams(dp(52), dp(Theme.TARGET_DP)));
        return row;
    }

    /**
     * Player position row: tile and world coordinates are re-read every second
     * while the row is visible.
     */
    private void RowPosition(LinearLayout linLayout, String featName) {
        LinearLayout column = new LinearLayout(getContext);
        column.setOrientation(LinearLayout.VERTICAL);

        TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final TextView value = Theme.text(getContext, "Menunggu posisi...", Theme.DESC_SP,
                Theme.ICON_BOX);
        column.addView(title);
        column.addView(value);

        final LinearLayout row = row(IconPattern.POSITION, column, null);
        installExplanation(row, 0, featName);
        linLayout.addView(row);

        registerMonitor(new Runnable() {
            @Override
            public void run() {
                if (row.isShown())
                    value.setText(PositionText());
            }
        });
    }

    private String PositionText() {
        String data;
        try {
            data = PlayerPosition();
        } catch (Throwable t) {
            data = null;
        }
        if (data == null || data.length() == 0)
            return "Posisi belum terbaca";

        String[] part = data.split("\t", -1);
        if (part.length < 4)
            return "Posisi belum terbaca";
        if (part[0].length() > 0 && part[1].length() > 0)
            return "Tile UI " + part[0] + ", " + part[1] + "   Dunia " + part[2] + ", "
                    + part[3];
        return "Dunia " + part[2] + ", " + part[3];
    }

    private Button buttonText(CharSequence text) {
        Button button = new Button(getContext);
        button.setText(text);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTextSize(Theme.ROW_SP);
        button.setTypeface(Theme.font(getContext));
        button.setAllCaps(false); //Disable caps to support html
        button.setGravity(Gravity.CENTER);
        button.setBackground(Theme.pressOverlay(getContext));
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(0);
        button.setMinimumHeight(0);
        button.setPadding(dp(6), 0, dp(6), 0);
        button.setStateListAnimator(null);
        button.setElevation(0f);
        return button;
    }

    private LinearLayout boxButton(String[] pattern, View content) {
        LinearLayout box = new LinearLayout(getContext);
        box.setOrientation(LinearLayout.HORIZONTAL);
        box.setGravity(Gravity.CENTER_VERTICAL);
        box.setBackground(Theme.button(getContext));
        box.setPadding(dp(3), dp(3), dp(8), dp(3));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, dp(50));
        params.setMargins(0, dp(4), 0, dp(4));
        box.setLayoutParams(params);

        // The icon is attached fully inside the button outline.
        box.addView(new FeatureIcon(getContext, pattern, true),
                new LinearLayout.LayoutParams(dp(FeatureIcon.WIDTH_DP - 3), MATCH_PARENT));

        LinearLayout.LayoutParams fillParams = new LinearLayout.LayoutParams(0, MATCH_PARENT, 1f);
        box.addView(content, fillParams);
        return box;
    }

    /**
     * Long-pressing a feature row opens the explanation dialog. The listener is
     * installed on the row and on controls that can swallow touches (button,
     * switch, spinner), so long-press still works anywhere.
     */
    private void installExplanation(View view, final int featNum, final String featName) {
        if (view == null)
            return;
        view.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                showExplanation(featNum, featName);
                return true;
            }
        });
    }

    private void showExplanation(int featNum, String featName) {
        menuDialog.explanation(featNum, featName);
    }

    private boolean isViewCollapsed() {
        return rootFrame == null || mCollapsed.getVisibility() == View.VISIBLE;
    }

    private int dp(float value) {
        return (int) Theme.dp(getContext, value);
    }

    public void setVisibility(int view) {
        if (rootFrame != null) {
            rootFrame.setVisibility(view);
        }
    }

    public void onDestroy() {
        destroyed = true;
        tickRun = false;
        sourceInstalled = null;
        handlerTick.removeCallbacksAndMessages(null);
        menuDialog.close();
        if (iconImage != null)
            releaseImage();
        if (rootFrame != null && mWindowManager != null) {
            try {
                mWindowManager.removeView(rootFrame);
            } catch (IllegalArgumentException e) {
                // The window is either not attached yet or already detached.
                Log.d(TAG, "removeView skipped: " + e);
            }
        }
    }
}
