//Please don't replace listeners with lambda!

package com.android.support;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.BitmapFactory;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.text.Html;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextUtils;
import android.text.method.DigitsKeyListener;
import android.util.Base64;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
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

import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;
import static android.view.WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
import static android.widget.RelativeLayout.ALIGN_PARENT_RIGHT;

public class Menu {
    public static final String TAG = "Mod_Menu"; //Tag for logcat

    // Tampilan panel kayu. Semua warna ada di Theme.
    int MENU_WIDTH = 300;
    int MENU_HEIGHT = 210;
    int POS_X = 0;
    int POS_Y = 100;
    int ICON_SIZE = 45; //Change both width and height of image
    float ICON_ALPHA = 0.7f; //Transparent

    RelativeLayout mCollapsed, mRootContainer;
    LinearLayout mExpanded, mods, mSettings, mCollapse;
    LinearLayout.LayoutParams scrlLLExpanded, scrlLL;
    WindowManager mWindowManager;
    WindowManager.LayoutParams vmParams;
    ImageView startimage;
    FrameLayout rootFrame;
    ScrollView scrollView;
    boolean stopChecking, overlayRequired;
    Context getContext;

    //initialize methods from the native library
    native void Init(Context context, TextView title, TextView subTitle);

    native String Icon();

    native String IconWebViewData();

    native String[] GetFeatureList();

    native String[] SettingsList();

    native boolean IsGameLibLoaded();

    // Section Room: daftar tamu berformat "nama<TAB>kunci<TAB>peran" per baris.
    native String DaftarPemain();

    // Posisi pemain lokal: "tileX<TAB>tileY<TAB>duniaX<TAB>duniaY". Bagian tile
    // kosong bila game tidak menyediakan field tile.
    native String PosisiPemain();

    native boolean TendangPemain(String kunci, String nama);

    native boolean SalinSkinPemain(String kunci, String nama);

    // Slider yang punya tombol reset, dikunci nomor fitur slider-nya.
    final Map<Integer, SeekBar> sliderFitur = new LinkedHashMap<Integer, SeekBar>();
    final Map<Integer, Integer> minFitur = new LinkedHashMap<Integer, Integer>();
    final Map<Integer, Integer> desimalFitur = new LinkedHashMap<Integer, Integer>();

    // Kunci pemain yang kartunya sedang dipilih. Pilihan disimpan sebagai teks
    // kunci, bukan indeks, jadi tetap menunjuk pemain yang sama saat daftar
    // berubah urutan dan otomatis dilepas saat pemain itu keluar.
    String kunciTerpilih;

    //Here we write the code for our Menu
    // Reference: https://www.androidhive.info/2016/11/android-floating-widget-like-facebook-chat-head/
    public Menu(Context context) {

        getContext = context;
        Preferences.context = context;
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
        int applyDimension = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, ICON_SIZE, context.getResources().getDisplayMetrics()); //Icon size
        startimage.getLayoutParams().height = applyDimension;
        startimage.getLayoutParams().width = applyDimension;
        startimage.setScaleType(ImageView.ScaleType.FIT_XY);
        byte[] decode = Base64.decode(Icon(), 0);
        startimage.setImageBitmap(BitmapFactory.decodeByteArray(decode, 0, decode.length));
        ((ViewGroup.MarginLayoutParams) startimage.getLayoutParams()).topMargin = convertDipToPixels(10);
        //Initialize event handlers for buttons, etc.
        startimage.setOnTouchListener(onTouchListener());
        startimage.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.GONE);
                mExpanded.setVisibility(View.VISIBLE);
            }
        });

        //********** The icon in Webview to open mod menu **********
        WebView wView = new WebView(context); //Icon size width=\"50\" height=\"50\"
        wView.setLayoutParams(new RelativeLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        int applyDimension2 = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, ICON_SIZE, context.getResources().getDisplayMetrics()); //Icon size
        wView.getLayoutParams().height = applyDimension2;
        wView.getLayoutParams().width = applyDimension2;
        wView.loadData("<html>" +
                "<head></head>" +
                "<body style=\"margin: 0; padding: 0\">" +
                "<img src=\"" + IconWebViewData() + "\" width=\"" + ICON_SIZE + "\" height=\"" + ICON_SIZE + "\" >" +
                "</body>" +
                "</html>", "text/html", "utf-8");
        wView.setBackgroundColor(0x00000000); //Transparent
        wView.setAlpha(ICON_ALPHA);
        wView.getSettings().setCacheMode(WebSettings.LOAD_NO_CACHE);
        wView.setOnTouchListener(onTouchListener());

        //********** Settings **********
        mSettings = new LinearLayout(context);
        mSettings.setOrientation(LinearLayout.VERTICAL);
        mSettings.setPadding(dp(10), dp(6), dp(10), dp(8));
        featureList(SettingsList(), mSettings);

        //********** Header panel: judul di tengah, setelan dan X di kanan **********
        RelativeLayout header = new RelativeLayout(context);
        header.setBackground(Theme.header(context));
        header.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(56)));

        RelativeLayout.LayoutParams rlTitle = new RelativeLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT);
        TextView title = Theme.teks(context, "", Theme.TITLE_SP, Theme.TITLE_TEXT);
        title.setGravity(Gravity.CENTER);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        title.setPadding(dp(78), 0, dp(78), 0);
        title.setLayoutParams(rlTitle);

        Theme.Ikon setelan = new Theme.Ikon(context, Theme.Ikon.SETELAN, Theme.ICON_BOX,
                Theme.TAN, 2f, Theme.TITLE_TEXT, 30);
        RelativeLayout.LayoutParams rlSetelan = new RelativeLayout.LayoutParams(dp(30), dp(30));
        rlSetelan.addRule(ALIGN_PARENT_RIGHT);
        rlSetelan.addRule(RelativeLayout.CENTER_VERTICAL);
        rlSetelan.rightMargin = dp(42);
        setelan.setLayoutParams(rlSetelan);
        setelan.setContentDescription("Setelan");
        setelan.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    if (mSettings.getParent() == scrollView) {
                        scrollView.removeView(mSettings);
                        scrollView.addView(mods);
                    } else {
                        scrollView.removeView(mods);
                        scrollView.addView(mSettings);
                        scrollView.scrollTo(0, 0);
                    }
                } catch (IllegalStateException ignored) {
                }
            }
        });

        Theme.Ikon tutup = new Theme.Ikon(context, Theme.Ikon.TUTUP, Theme.OUTLINE,
                Theme.ACTIVE_BORDER, 3f, Theme.ACTIVE_BORDER, 30);
        RelativeLayout.LayoutParams rlTutup = new RelativeLayout.LayoutParams(dp(30), dp(30));
        rlTutup.addRule(ALIGN_PARENT_RIGHT);
        rlTutup.addRule(RelativeLayout.CENTER_VERTICAL);
        rlTutup.rightMargin = dp(6);
        tutup.setLayoutParams(rlTutup);
        tutup.setContentDescription("Kecilkan menu");
        tutup.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(ICON_ALPHA);
                mExpanded.setVisibility(View.GONE);
            }
        });

        header.addView(title);
        header.addView(setelan);
        header.addView(tutup);

        //********** Sub title **********
        TextView subTitle = Theme.teks(context, "", Theme.DESC_SP, Theme.MUTED);
        subTitle.setEllipsize(TextUtils.TruncateAt.MARQUEE);
        subTitle.setMarqueeRepeatLimit(-1);
        subTitle.setSingleLine(true);
        subTitle.setSelected(true);
        subTitle.setGravity(Gravity.CENTER);
        subTitle.setPadding(dp(10), dp(4), dp(10), dp(4));
        subTitle.setBackgroundColor(Theme.BODY);

        //********** Mod menu feature list **********
        scrollView = new ScrollView(context);
        scrlLL = new LinearLayout.LayoutParams(MATCH_PARENT, dp(MENU_HEIGHT));
        scrlLLExpanded = new LinearLayout.LayoutParams(mExpanded.getLayoutParams());
        scrlLLExpanded.weight = 1.0f;
        scrollView.setLayoutParams(Preferences.isExpanded ? scrlLLExpanded : scrlLL);
        scrollView.setBackgroundColor(Theme.BODY);
        mods = new LinearLayout(context);
        mods.setOrientation(LinearLayout.VERTICAL);
        mods.setPadding(dp(10), dp(6), dp(10), dp(8));

        //********** Footer dengan tombol kayu **********
        LinearLayout footer = new LinearLayout(context);
        footer.setOrientation(LinearLayout.HORIZONTAL);
        footer.setBackground(Theme.footer(context));
        footer.setPadding(dp(8), dp(6), dp(8), dp(6));

        final Button hideBtn = teksTombol("Sembunyikan (tahan = keluar)");
        hideBtn.setBackground(Theme.tombol(context));
        hideBtn.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(42)));
        hideBtn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(0);
                mExpanded.setVisibility(View.GONE);
                Toast.makeText(view.getContext(), "Ikon disembunyikan. Ingat posisi ikon tersembunyi", Toast.LENGTH_LONG).show();
            }
        });
        hideBtn.setOnLongClickListener(new View.OnLongClickListener() {
            public boolean onLongClick(View view) {
                Toast.makeText(view.getContext(), "Menu ditutup", Toast.LENGTH_LONG).show();
                rootFrame.removeView(mRootContainer);
                mWindowManager.removeView(rootFrame);
                return false;
            }
        });
        footer.addView(hideBtn);

        //********** Adding view components **********
        mRootContainer.addView(mCollapsed);
        mRootContainer.addView(mExpanded);
        if (IconWebViewData() != null) {
            mCollapsed.addView(wView);
        } else {
            mCollapsed.addView(startimage);
        }
        mExpanded.addView(header);
        Theme.Zigzag zigAtas = new Theme.Zigzag(context);
        zigAtas.setBackgroundColor(Theme.BODY);
        mExpanded.addView(zigAtas, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(subTitle, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        scrollView.addView(mods);
        mExpanded.addView(scrollView);
        Theme.Zigzag zigBawah = new Theme.Zigzag(context);
        zigBawah.setBackgroundColor(Theme.FOOTER);
        mExpanded.addView(zigBawah, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(footer);

        Init(context, title, subTitle);
        Theme.applyFont(mExpanded);
        Theme.applyFont(mSettings);
    }

    public void ShowMenu() {
        rootFrame.addView(mRootContainer);

        final Handler handler = new Handler();
        handler.postDelayed(new Runnable() {
            boolean viewLoaded = false;

            @Override
            public void run() {
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
                    handler.postDelayed(this, 600);
                } else {
                    mods.removeAllViews();
                    featureList(GetFeatureList(), mods);
                    Theme.applyFont(mods);
                }
            }
        }, 500);
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerWindowService() {
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
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerActivity() {
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

    private View.OnTouchListener onTouchListener() {
        return new View.OnTouchListener() {
            final View collapsedView = mCollapsed;
            final View expandedView = mExpanded;
            private float initialTouchX, initialTouchY;
            private int initialX, initialY;

            public boolean onTouch(View view, MotionEvent motionEvent) {
                switch (motionEvent.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = vmParams.x;
                        initialY = vmParams.y;
                        initialTouchX = motionEvent.getRawX();
                        initialTouchY = motionEvent.getRawY();
                        return true;
                    case MotionEvent.ACTION_UP:
                        int rawX = (int) (motionEvent.getRawX() - initialTouchX);
                        int rawY = (int) (motionEvent.getRawY() - initialTouchY);
                        mExpanded.setAlpha(1f);
                        mCollapsed.setAlpha(1f);
                        //The check for Xdiff <10 && YDiff< 10 because sometime elements moves a little while clicking.
                        //So that is click event.
                        if (rawX < 10 && rawY < 10 && isViewCollapsed()) {
                            //When user clicks on the image view of the collapsed layout,
                            //visibility of the collapsed layout will be changed to "View.GONE"
                            //and expanded view will become visible.
                            try {
                                collapsedView.setVisibility(View.GONE);
                                expandedView.setVisibility(View.VISIBLE);
                            } catch (NullPointerException ignored) {
                            }
                        }
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        mExpanded.setAlpha(0.5f);
                        mCollapsed.setAlpha(0.5f);
                        //Calculate the X and Y coordinates of the view.
                        vmParams.x = initialX + ((int) (motionEvent.getRawX() - initialTouchX));
                        vmParams.y = initialY + ((int) (motionEvent.getRawY() - initialTouchY));
                        //Update the layout with new X & Y coordinate
                        mWindowManager.updateViewLayout(rootFrame, vmParams);
                        return true;
                    default:
                        return false;
                }
            }
        };
    }

    private void featureList(String[] listFT, LinearLayout linearLayout) {
        //Currently looks messy right now. Let me know if you have improvements
        int featNum, subFeat = 0;
        LinearLayout llBak = linearLayout;

        for (int i = 0; i < listFT.length; i++) {
            boolean switchedOn = false;
            //Log.i("featureList", listFT[i]);
            String feature = listFT[i];
            if (feature.contains("_True")) {
                switchedOn = true;
                feature = feature.replaceFirst("_True", "");
            }

            linearLayout = llBak;
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
                    BarisPosisi(linearLayout, strSplit[1]);
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
        }
    }

    private void Switch(LinearLayout linLayout, final int featNum, final String featName, boolean swiOn) {
        final AnimatedSwitch switchR = new AnimatedSwitch(getContext);
        switchR.setContentDescription(featName);
        switchR.setChecked(Preferences.loadPrefBool(featName, featNum, swiOn));
        switchR.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton compoundButton, boolean bool) {
                Preferences.changeFeatureBool(featName, featNum, bool);
                switch (featNum) {
                    case -1: //Save perferences
                        Preferences.with(switchR.getContext()).writeBoolean(-1, bool);
                        if (!bool) Preferences.with(switchR.getContext()).clear(); //Clear perferences if switched off
                        break;
                    case -3:
                        Preferences.isExpanded = bool;
                        scrollView.setLayoutParams(bool ? scrlLLExpanded : scrlLL);
                        break;
                }
            }
        });

        TextView label = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final LinearLayout baris = baris(IkonFitur.pola(featNum), label, switchR);
        baris.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                switchR.toggle();
            }
        });
        linLayout.addView(baris);
    }

    private void SeekBar(LinearLayout linLayout, final int featNum, final String featName,
                         final int min, final int max, final int def, final int desimal) {
        // Nilai 0 adalah nilai sah untuk gravitasi, jadi default hanya dipakai
        // bila fitur ini belum pernah disimpan.
        boolean tersimpan = Preferences.sudahDisimpan(featName, featNum);
        int nilaiAwal = tersimpan ? Preferences.loadPrefInt(featName, featNum) : def;
        if (nilaiAwal < min)
            nilaiAwal = min;
        if (nilaiAwal > max)
            nilaiAwal = max;
        if (!tersimpan)
            Preferences.changeFeatureInt(featName, featNum, nilaiAwal);

        LinearLayout kolom = new LinearLayout(getContext);
        kolom.setOrientation(LinearLayout.VERTICAL);
        kolom.setGravity(Gravity.CENTER_VERTICAL);

        final TextView textView = Theme.teks(getContext, "", Theme.ROW_SP, Theme.TEXT);
        textView.setText(Html.fromHtml(featName + ": <font color='" + Theme.warnaHex(Theme.ICON_BOX) + "'>"
                + nilaiTeks(nilaiAwal, desimal) + "</font>"));

        SeekBar seekBar = new SeekBar(getContext);
        seekBar.setProgressDrawable(Theme.seekbarProgress(getContext));
        seekBar.setThumb(Theme.seekbarThumb(getContext));
        seekBar.setThumbOffset(0);
        seekBar.setPadding(dp(2), dp(4), dp(2), dp(4));
        // Rentang digeser ke nol supaya setMin yang hanya ada di Oreo ke atas
        // tidak dibutuhkan.
        seekBar.setMax(max - min);
        seekBar.setProgress(nilaiAwal - min);
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            public void onStopTrackingTouch(SeekBar seekBar) {
            }

            public void onProgressChanged(SeekBar seekBar, int i, boolean z) {
                int nilai = i + min;
                if (nilai < min)
                    nilai = min;
                if (nilai > max)
                    nilai = max;
                Preferences.changeFeatureInt(featName, featNum, nilai);
                textView.setText(Html.fromHtml(featName + ": <font color='" + Theme.warnaHex(Theme.ICON_BOX) + "'>"
                        + nilaiTeks(nilai, desimal) + "</font>"));
            }
        });

        sliderFitur.put(featNum, seekBar);
        minFitur.put(featNum, min);
        desimalFitur.put(featNum, desimal);

        kolom.addView(textView, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        kolom.addView(seekBar, new LinearLayout.LayoutParams(MATCH_PARENT, dp(34)));
        linLayout.addView(baris(IkonFitur.pola(featNum), kolom, null));
    }

    /** Angka slider ditampilkan dengan jumlah desimal yang ditulis di feature list. */
    private String nilaiTeks(int nilai, int desimal) {
        if (desimal <= 0)
            return String.valueOf(nilai);
        return String.format(Locale.US, "%." + desimal + "f", nilai / 10f);
    }

    /** Mengembalikan slider ke nilai bawaan lewat jalur yang sama dengan geseran. */
    private void aturUlangSlider(int featNum, int nilai) {
        SeekBar seekBar = sliderFitur.get(featNum);
        Integer min = minFitur.get(featNum);
        if (seekBar == null || min == null)
            return;
        seekBar.setProgress(Math.max(0, nilai - min));
    }

    private void Button(LinearLayout linLayout, final int featNum, final String featName) {
        final Button button = teksTombol(Html.fromHtml(featName));
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                switch (featNum) {

                    case 25:
                        aturUlangSlider(23, 10);
                        break;
                    case 26:
                        aturUlangSlider(24, 10);
                        break;
                    case -6:
                        scrollView.removeView(mSettings);
                        scrollView.addView(mods);
                        break;
                    case -100:
                        stopChecking = true;
                        break;
                }
                Preferences.changeFeatureInt(featName, featNum, 0);
            }
        });

        linLayout.addView(kotakTombol(IkonFitur.pola(featNum), button));
    }

    private void ButtonLink(LinearLayout linLayout, final String featName, final String url) {
        final Button button = teksTombol(Html.fromHtml(featName));
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.setData(Uri.parse(url));
                getContext.startActivity(intent);
            }
        });
        // Tautan tidak punya nomor fitur, jadi ikonnya tetap roda gigi.
        linLayout.addView(kotakTombol(IkonFitur.PENGATURAN, button));
    }

    private void ButtonOnOff(LinearLayout linLayout, final int featNum, String featName, boolean switchedOn) {
        final Button button = teksTombol("");
        final String finalfeatName = featName.replace("OnOff_", "");
        boolean isOn = Preferences.loadPrefBool(featName, featNum, switchedOn);
        final LinearLayout kotak = kotakTombol(IkonFitur.pola(featNum), button);
        if (isOn) {
            button.setText(Html.fromHtml(finalfeatName + ": ON"));
            kotak.setBackground(Theme.tombolAktif(getContext));
            isOn = false;
        } else {
            button.setText(Html.fromHtml(finalfeatName + ": OFF"));
            kotak.setBackground(Theme.tombolMati(getContext));
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
                    kotak.setBackground(Theme.tombolAktif(getContext));
                    isOn = false;
                } else {
                    button.setText(Html.fromHtml(finalfeatName + ": OFF"));
                    kotak.setBackground(Theme.tombolMati(getContext));
                    isOn = true;
                }
            }
        });
        linLayout.addView(kotak);
    }

    private void Spinner(LinearLayout linLayout, final int featNum, final String featName, final String list) {
        Log.d(TAG, "spinner " + featNum + " " + featName + " " + list);
        final List<String> lists = new LinkedList<>(Arrays.asList(list.split(",")));

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
        spinner.setSelection(Preferences.loadPrefInt(featName, featNum));
        if (spinner.getBackground() != null)
            spinner.getBackground().setColorFilter(Theme.TEXT, PorterDuff.Mode.SRC_ATOP);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parentView, View selectedItemView, int position, long id) {
                Preferences.changeFeatureInt(spinner.getSelectedItem().toString(), featNum, position);
                ((TextView) parentView.getChildAt(0)).setTextColor(Theme.TEXT);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        linLayout.addView(baris(IkonFitur.pola(featNum), spinner, null));
    }

    private void InputNum(LinearLayout linLayout, final int featNum, final String featName, final int maxValue) {
        final Button button = teksTombol("");
        int num = Preferences.loadPrefInt(featName, featNum);
        button.setText(Html.fromHtml(featName + ": " + num));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder alertName = new AlertDialog.Builder(getContext);
                final EditText editText = new EditText(getContext);
                if (maxValue != 0)
                    editText.setHint("Nilai maksimum: " + maxValue);
                editText.setInputType(InputType.TYPE_CLASS_NUMBER);
                editText.setKeyListener(DigitsKeyListener.getInstance("0123456789-"));
                InputFilter[] FilterArray = new InputFilter[1];
                FilterArray[0] = new InputFilter.LengthFilter(10);
                editText.setFilters(FilterArray);
                editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (hasFocus) {
                            imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY);
                        } else {
                            imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                        }
                    }
                });
                editText.requestFocus();

                alertName.setTitle("Masukkan angka");
                alertName.setView(editText);
                LinearLayout layoutName = new LinearLayout(getContext);
                layoutName.setOrientation(LinearLayout.VERTICAL);
                layoutName.addView(editText); // displays the user input bar
                alertName.setView(layoutName);

                alertName.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        int num;
                        try {
                            String inp = editText.getText().toString();
                            num = Integer.parseInt(inp.isEmpty() ? "0" : inp);
                            if (maxValue != 0 && num >= maxValue)
                                num = maxValue;
                        } catch (NumberFormatException ex) {
                            if (maxValue != 0)
                                num = maxValue;
                            else
                                num = Integer.MAX_VALUE;
                        }

                        button.setText(Html.fromHtml(featName + ": " + num));
                        Preferences.changeFeatureInt(featName, featNum, num);
                        editText.setFocusable(false);
                    }
                });

                alertName.setNegativeButton("Batal", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        // dialog.cancel(); // closes dialog
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                    }
                });

                tampilkanDialog(alertName);
            }
        });

        linLayout.addView(kotakTombol(IkonFitur.pola(featNum), button));
    }

    private void InputLNum(LinearLayout linLayout, final int featNum, final String featName, final long maxValue) {
        final Button button = teksTombol("");
        long num = Preferences.loadPrefLong(featName, featNum);
        button.setText(Html.fromHtml(featName + ": " + num));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder alertName = new AlertDialog.Builder(getContext);
                final EditText editText = new EditText(getContext);
                if (maxValue != 0)
                    editText.setHint("Nilai maksimum: " + maxValue);
                editText.setInputType(InputType.TYPE_CLASS_NUMBER);
                editText.setKeyListener(DigitsKeyListener.getInstance("0123456789-"));
                InputFilter[] FilterArray = new InputFilter[1];
                FilterArray[0] = new InputFilter.LengthFilter(20);
                editText.setFilters(FilterArray);
                editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (hasFocus) {
                            imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY);
                        } else {
                            imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                        }
                    }
                });
                editText.requestFocus();

                alertName.setTitle("Masukkan angka");
                alertName.setView(editText);
                LinearLayout layoutName = new LinearLayout(getContext);
                layoutName.setOrientation(LinearLayout.VERTICAL);
                layoutName.addView(editText); // displays the user input bar
                alertName.setView(layoutName);

                alertName.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        long num;
                        try {
                            String inp = editText.getText().toString();
                            num = Long.parseLong(inp.isEmpty() ? "0" : inp);
                            if (maxValue != 0 && num >= maxValue)
                                num = maxValue;
                        } catch (NumberFormatException ex) {
                            if (maxValue != 0)
                                num = maxValue;
                            else
                                num = Long.MAX_VALUE;
                        }

                        button.setText(Html.fromHtml(featName + ": " + num));
                        Preferences.changeFeatureLong(featName, featNum, num);

                        editText.setFocusable(false);
                    }
                });

                alertName.setNegativeButton("Batal", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        // dialog.cancel(); // closes dialog
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                    }
                });

                tampilkanDialog(alertName);
            }
        });

        linLayout.addView(kotakTombol(IkonFitur.pola(featNum), button));
    }

    private void InputText(LinearLayout linLayout, final int featNum, final String featName) {
        final Button button = teksTombol("");
        String string = Preferences.loadPrefString(featName, featNum);
        button.setText(Html.fromHtml(featName + ": " + string));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder alertName = new AlertDialog.Builder(getContext);

                final EditText editText = new EditText(getContext);
                editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (hasFocus) {
                            imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY);
                        } else {
                            imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                        }
                    }
                });
                editText.requestFocus();

                alertName.setTitle("Masukkan teks");
                alertName.setView(editText);
                LinearLayout layoutName = new LinearLayout(getContext);
                layoutName.setOrientation(LinearLayout.VERTICAL);
                layoutName.addView(editText); // displays the user input bar
                alertName.setView(layoutName);

                alertName.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        String str = editText.getText().toString();
                        button.setText(Html.fromHtml(featName + ": " + str));
                        Preferences.changeFeatureString(featName, featNum, str);
                        editText.setFocusable(false);
                    }
                });

                alertName.setNegativeButton("Batal", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        //dialog.cancel(); // closes dialog
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                    }
                });

                tampilkanDialog(alertName);
            }
        });

        linLayout.addView(kotakTombol(IkonFitur.pola(featNum), button));
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
                if (checkBox.isChecked()) {
                    Preferences.changeFeatureBool(featName, featNum, isChecked);
                } else {
                    Preferences.changeFeatureBool(featName, featNum, isChecked);
                }
            }
        });
        linLayout.addView(baris(IkonFitur.pola(featNum), checkBox, null));
    }

    @SuppressLint("SetTextI18n")
    private void RadioButton(LinearLayout linLayout, final int featNum, String featName, final String list) {
        //Credit: LoraZalora
        final List<String> lists = new LinkedList<>(Arrays.asList(list.split(",")));

        final TextView textView = Theme.teks(getContext, featName + ":", Theme.ROW_SP, Theme.TEXT);

        final RadioGroup radioGroup = new RadioGroup(getContext);
        radioGroup.setPadding(dp(10), dp(6), dp(10), dp(6));
        radioGroup.setOrientation(LinearLayout.VERTICAL);
        radioGroup.setBackground(Theme.baris(getContext));
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
                            + Theme.warnaHex(Theme.ICON_BOX) + "'>" + radioName));
                    Preferences.changeFeatureInt(finalfeatName, featNum, radioGroup.indexOfChild(Radioo));
                }
            };
            System.out.println(lists.get(i));
            Radioo.setText(lists.get(i));
            Radioo.setTextColor(Theme.TEXT);
            Radioo.setTextSize(Theme.ROW_SP);
            Radioo.setTypeface(Theme.font(getContext));
            Radioo.setButtonTintList(ColorStateList.valueOf(Theme.ICON_BOX));
            Radioo.setOnClickListener(first_radio_listener);
            radioGroup.addView(Radioo);
        }

        int index = Preferences.loadPrefInt(featName, featNum);
        if (index > 0) { //Preventing it to get an index less than 1. below 1 = null = crash
            textView.setText(Html.fromHtml(featName + ": <font color='" + Theme.warnaHex(Theme.ICON_BOX)
                    + "'>" + lists.get(index - 1)));
            ((RadioButton) radioGroup.getChildAt(index)).setChecked(true);
        }
        linLayout.addView(radioGroup);
    }

    @SuppressLint("SetTextI18n")
    private void Collapse(LinearLayout linLayout, final String text, final boolean expanded) {
        LinearLayout section = new LinearLayout(getContext);
        section.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams sectionParams = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        sectionParams.setMargins(0, dp(8), 0, 0);
        section.setLayoutParams(sectionParams);

        LinearLayout kepala = new LinearLayout(getContext);
        kepala.setOrientation(LinearLayout.HORIZONTAL);
        kepala.setGravity(Gravity.CENTER_VERTICAL);
        kepala.setBackground(Theme.sectionHeader(getContext));
        kepala.setPadding(dp(8), 0, dp(8), 0);
        kepala.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(42)));

        View strip = new View(getContext);
        strip.setBackgroundColor(Theme.ACCENT);
        LinearLayout.LayoutParams stripParams = new LinearLayout.LayoutParams(dp(5), MATCH_PARENT);
        stripParams.setMargins(0, dp(6), dp(8), dp(6));
        kepala.addView(strip, stripParams);

        final TextView judul = Theme.teks(getContext, text, Theme.SECTION_SP, Theme.TITLE_TEXT);
        judul.setSingleLine(true);
        judul.setEllipsize(TextUtils.TruncateAt.END);
        LinearLayout.LayoutParams judulParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        kepala.addView(judul, judulParams);

        final Theme.Ikon panah = Theme.Ikon.polos(getContext,
                expanded ? Theme.Ikon.PANAH_BAWAH : Theme.Ikon.PANAH_KANAN,
                Theme.TITLE_TEXT, 20);
        kepala.addView(panah, new LinearLayout.LayoutParams(dp(20), dp(20)));

        final LinearLayout isi = new LinearLayout(getContext);
        isi.setOrientation(LinearLayout.VERTICAL);
        isi.setPadding(dp(6), dp(6), dp(6), dp(6));
        isi.setBackground(Theme.sectionBody(getContext));
        mCollapse = isi;

        final LinearLayout pembungkus = new LinearLayout(getContext);
        pembungkus.setOrientation(LinearLayout.VERTICAL);
        pembungkus.addView(new Theme.Zigzag(getContext),
                new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        pembungkus.addView(isi);

        if (expanded) {
            pembungkus.setVisibility(View.VISIBLE);
            panah.setJenis(Theme.Ikon.PANAH_BAWAH);
        } else {
            pembungkus.setVisibility(View.GONE);
            panah.setJenis(Theme.Ikon.PANAH_KANAN);
        }

        kepala.setOnClickListener(new View.OnClickListener() {
            boolean terbuka = expanded;

            @Override
            public void onClick(View v) {
                terbuka = !terbuka;
                pembungkus.setVisibility(terbuka ? View.VISIBLE : View.GONE);
                panah.setJenis(terbuka ? Theme.Ikon.PANAH_BAWAH : Theme.Ikon.PANAH_KANAN);
            }
        });

        section.addView(kepala);
        section.addView(pembungkus);
        linLayout.addView(section);
    }

    private static final class KartuPemain {
        final String nama;
        final String peran;
        final LinearLayout view;

        KartuPemain(String nama, String peran, LinearLayout view) {
            this.nama = nama;
            this.peran = peran;
            this.view = view;
        }
    }

    /** Keadaan satu daftar pemain: kartu yang sudah ada dan isi terakhirnya. */
    private static final class IsiDaftar {
        final Map<String, KartuPemain> kartu = new LinkedHashMap<String, KartuPemain>();
        String data;
        TextView kosong;
    }

    @SuppressLint("SetTextI18n")
    private void PlayerList(LinearLayout linLayout, final String featName) {
        LinearLayout container = new LinearLayout(getContext);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(dp(2), dp(2), dp(2), dp(4));

        LinearLayout judulBaris = new LinearLayout(getContext);
        judulBaris.setOrientation(LinearLayout.HORIZONTAL);
        judulBaris.setGravity(Gravity.CENTER_VERTICAL);
        judulBaris.setBackground(Theme.baris(getContext()));
        judulBaris.setPadding(0, 0, dp(8), 0);
        LinearLayout.LayoutParams paramsJudul = new LinearLayout.LayoutParams(MATCH_PARENT, dp(48));
        paramsJudul.setMargins(0, dp(3), 0, dp(3));
        judulBaris.setLayoutParams(paramsJudul);

        LinearLayout.LayoutParams paramsIkon = new LinearLayout.LayoutParams(dp(40), dp(40));
        paramsIkon.setMargins(0, dp(4), dp(8), dp(4));
        judulBaris.addView(new IkonFitur(getContext(), IkonFitur.PEMAIN, true), paramsIkon);

        TextView judul = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        LinearLayout.LayoutParams paramsJudulTeks = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        paramsJudulTeks.setMargins(dp(4), 0, dp(6), 0);
        judulBaris.addView(judul, paramsJudulTeks);

        final TextView jumlah = Theme.teks(getContext, "", Theme.DESC_SP, Theme.MUTED);
        judulBaris.addView(jumlah);
        container.addView(judulBaris);

        TextView petunjuk = Theme.teks(getContext,
                "Ketuk kartu untuk memilih. Salin skin atau tendang tamu dari sini.",
                Theme.DESC_SP, Theme.MUTED);
        petunjuk.setPadding(dp(4), dp(2), dp(4), dp(6));
        container.addView(petunjuk);

        final LinearLayout daftar = new LinearLayout(getContext);
        daftar.setOrientation(LinearLayout.VERTICAL);
        container.addView(daftar);

        Button muatUlang = tombolKecil("Muat ulang daftar", Theme.STROKE_KONTROL,
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        IsiDaftar isi = (IsiDaftar) daftar.getTag();
                        if (isi != null)
                            isi.data = null;
                        isiDaftarPemain(daftar, jumlah);
                    }
                });
        LinearLayout.LayoutParams paramsMuat =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsMuat.setMargins(0, dp(6), 0, 0);
        muatUlang.setLayoutParams(paramsMuat);
        container.addView(muatUlang);

        linLayout.addView(container);
        isiDaftarPemain(daftar, jumlah);

        // Detak satu detik hanya membaca data native saat section ini benar-benar
        // terlihat. Saat menu ditutup atau section dilipat, tidak ada salinan
        // daftar yang dibuat dan tidak ada view yang disentuh.
        pantauSaatTerpasang(daftar, new Runnable() {
            @Override
            public void run() {
                if (daftar.isShown())
                    isiDaftarPemain(daftar, jumlah);
                daftar.postDelayed(this, 1000);
            }
        });
    }

    @SuppressLint("SetTextI18n")
    private void isiDaftarPemain(LinearLayout daftar, TextView jumlah) {
        String data = ambilDaftarPemain();

        IsiDaftar isi = (IsiDaftar) daftar.getTag();
        if (isi == null) {
            isi = new IsiDaftar();
            daftar.removeAllViews();
            daftar.setTag(isi);
        }
        if (data.equals(isi.data))
            return;
        isi.data = data;

        Map<String, String[]> baru = new LinkedHashMap<String, String[]>();
        for (String baris : data.split("\n")) {
            String[] bagian = baris.split("\t", 3);
            if (bagian.length < 3 || bagian[0].length() == 0 || bagian[1].length() == 0)
                continue;
            baru.put(bagian[1], new String[]{bagian[0], bagian[2]});
        }

        // Pemain yang keluar dibuang lebih dulu. Bila yang pergi adalah pilihan
        // sekarang, pilihannya dilepas supaya tidak menunjuk kunci yang hilang.
        Iterator<Map.Entry<String, KartuPemain>> it = isi.kartu.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, KartuPemain> entri = it.next();
            if (!baru.containsKey(entri.getKey())) {
                daftar.removeView(entri.getValue().view);
                if (entri.getKey().equals(kunciTerpilih))
                    kunciTerpilih = null;
                it.remove();
            }
        }

        if (isi.kosong != null) {
            daftar.removeView(isi.kosong);
            isi.kosong = null;
        }

        int indeks = 0;
        for (Map.Entry<String, String[]> entri : baru.entrySet()) {
            String kunci = entri.getKey();
            String nama = entri.getValue()[0];
            String peran = entri.getValue()[1];
            KartuPemain kartu = isi.kartu.get(kunci);
            if (kartu != null && kartu.nama.equals(nama) && kartu.peran.equals(peran)) {
                // Isi sama: kartunya dipakai lagi, hanya urutannya yang dirapikan.
                if (daftar.getChildAt(indeks) != kartu.view) {
                    daftar.removeView(kartu.view);
                    daftar.addView(kartu.view, Math.min(indeks, daftar.getChildCount()));
                }
            } else {
                LinearLayout view = barisPemain(isi, kunci, nama, peran);
                if (kartu != null)
                    daftar.removeView(kartu.view);
                daftar.addView(view, Math.min(indeks, daftar.getChildCount()));
                isi.kartu.put(kunci, new KartuPemain(nama, peran, view));
            }
            indeks++;
        }

        jumlah.setText(indeks == 0 ? "" : indeks + " pemain");
        if (indeks == 0) {
            TextView kosong = Theme.teks(getContext, "Tidak ada pemain di room ini",
                    Theme.DESC_SP, Theme.MUTED);
            kosong.setGravity(Gravity.CENTER);
            kosong.setPadding(0, dp(14), 0, dp(14));
            daftar.addView(kosong);
            isi.kosong = kosong;
        }
    }

    private String ambilDaftarPemain() {
        try {
            String data = DaftarPemain();
            return data == null ? "" : data;
        } catch (Throwable t) {
            return "";
        }
    }

    // Satu pemain menjadi satu kartu: nama dan lencana perannya di atas, aksi di
    // bawah. Kartu sendiri tidak punya aksi karena menyalin atau menendang diri
    // sendiri tidak berguna, dan tendang hanya ada untuk tamu.
    private LinearLayout barisPemain(final IsiDaftar isi, final String kunci,
                                     final String nama, String peran) {
        LinearLayout baris = new LinearLayout(getContext);
        baris.setOrientation(LinearLayout.VERTICAL);
        baris.setPadding(dp(10), dp(8), dp(10), dp(8));
        LinearLayout.LayoutParams paramsBaris =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsBaris.setMargins(0, 0, 0, dp(6));
        baris.setLayoutParams(paramsBaris);
        baris.setBackground(kunci.equals(kunciTerpilih)
                ? Theme.kartuTerpilih(getContext())
                : Theme.kartu(getContext()));

        LinearLayout kepala = new LinearLayout(getContext());
        kepala.setOrientation(LinearLayout.HORIZONTAL);
        kepala.setGravity(Gravity.CENTER_VERTICAL);

        TextView label = Theme.teks(getContext, nama, Theme.ROW_SP, Theme.TEXT);
        label.setSingleLine(true);
        label.setEllipsize(TextUtils.TruncateAt.END);
        label.setLayoutParams(new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));
        kepala.addView(label);
        kepala.addView(lencanaPeran(peran));
        baris.addView(kepala);

        baris.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                kunciTerpilih = kunci.equals(kunciTerpilih) ? null : kunci;
                segarkanPilihan(isi);
            }
        });

        if ("kamu".equals(peran))
            return baris;

        LinearLayout aksi = new LinearLayout(getContext());
        aksi.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams paramsAksi =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsAksi.setMargins(0, dp(6), 0, 0);
        aksi.setLayoutParams(paramsAksi);
        aksi.addView(tombolKecil("Salin skin", Theme.STROKE_KONTROL, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean diterima;
                try {
                    diterima = SalinSkinPemain(kunci, nama);
                } catch (Throwable t) {
                    diterima = false;
                }
                // Hasil sebenarnya dikabarkan toast dari thread game; di sini
                // hanya antrean yang bisa langsung ditolak.
                if (!diterima)
                    Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                            Toast.LENGTH_SHORT).show();
            }
        }));
        if ("tamu".equals(peran)) {
            aksi.addView(tombolKecil("Tendang", Theme.STROKE_KONTROL, new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    boolean diterima;
                    try {
                        diterima = TendangPemain(kunci, nama);
                    } catch (Throwable t) {
                        diterima = false;
                    }
                    if (!diterima)
                        Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                Toast.LENGTH_SHORT).show();
                }
            }));
        }
        baris.addView(aksi);
        return baris;
    }

    private void segarkanPilihan(IsiDaftar isi) {
        for (Map.Entry<String, KartuPemain> entri : isi.kartu.entrySet()) {
            entri.getValue().view.setBackground(entri.getKey().equals(kunciTerpilih)
                    ? Theme.kartuTerpilih(getContext())
                    : Theme.kartu(getContext()));
        }
    }

    private Button tombolKecil(String teks, int warnaGaris, View.OnClickListener pendengar) {
        Button tombol = new Button(getContext);
        tombol.setText(teks);
        tombol.setTextSize(13f);
        tombol.setAllCaps(false);
        tombol.setTextColor(Theme.TITLE_TEXT);
        tombol.setTypeface(Theme.font(getContext));
        tombol.setBackground(Theme.tombolKecil(getContext, warnaGaris));
        tombol.setMinWidth(0);
        tombol.setMinimumWidth(0);
        tombol.setMinHeight(0);
        tombol.setMinimumHeight(0);
        tombol.setPadding(dp(8), dp(6), dp(8), dp(6));
        tombol.setStateListAnimator(null);
        tombol.setElevation(0f);
        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        params.setMargins(0, 0, dp(6), 0);
        tombol.setLayoutParams(params);
        tombol.setOnClickListener(pendengar);
        return tombol;
    }

    @SuppressLint("SetTextI18n")
    private TextView lencanaPeran(String peran) {
        int warna;
        if ("host".equals(peran))
            warna = Theme.ACCENT;
        else if ("kamu".equals(peran))
            warna = Theme.ICON_BOX;
        else
            warna = Theme.TRACK;

        int warnaTeks = warna == Theme.ACCENT ? Theme.TEXT : Theme.ACTIVE_BORDER;
        TextView lencana = Theme.teks(getContext, peran, 12f, warnaTeks);
        lencana.setGravity(Gravity.CENTER);
        lencana.setPadding(dp(6), dp(2), dp(6), dp(2));
        lencana.setBackground(Theme.lencana(getContext, warna));
        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        params.setMargins(dp(6), 0, 0, 0);
        lencana.setLayoutParams(params);
        return lencana;
    }

    private void Category(LinearLayout linLayout, String text) {
        LinearLayout baris = new LinearLayout(getContext);
        baris.setOrientation(LinearLayout.HORIZONTAL);
        baris.setGravity(Gravity.CENTER_VERTICAL);
        baris.setBackground(Theme.sectionHeader(getContext));
        baris.setPadding(dp(8), dp(8), dp(8), dp(8));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(8), 0, 0);
        baris.setLayoutParams(params);

        View strip = new View(getContext);
        strip.setBackgroundColor(Theme.ACCENT);
        LinearLayout.LayoutParams stripParams = new LinearLayout.LayoutParams(dp(5), dp(24));
        stripParams.setMargins(0, dp(2), dp(8), dp(2));
        baris.addView(strip, stripParams);

        TextView textView = Theme.teks(getContext, Html.fromHtml(text), Theme.SECTION_SP, Theme.TITLE_TEXT);
        textView.setGravity(Gravity.CENTER);
        textView.setLayoutParams(new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));
        baris.addView(textView);
        linLayout.addView(baris);
    }

    private void TextView(LinearLayout linLayout, String text) {
        TextView textView = Theme.teks(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.MUTED);
        textView.setPadding(dp(10), dp(4), dp(10), dp(2));
        linLayout.addView(textView);
    }

    private void WebTextView(LinearLayout linLayout, String text) {
        WebView wView = new WebView(getContext);
        wView.loadData(text, "text/html", "utf-8");
        wView.setBackgroundColor(0x00000000); //Transparent
        wView.setPadding(0, 5, 0, 5);
        wView.getSettings().setCacheMode(WebSettings.LOAD_NO_CACHE);
        linLayout.addView(wView);
    }

    /**
     * Baris kontrol: kotak ikon menempel di tepi kiri (hanya sudut kiri yang
     * melengkung), label di tengah, dan kontrol di kanan. Tinggi minimum 48dp
     * supaya seluruh baris nyaman disentuh.
     */
    private LinearLayout baris(String[] pola, View isi, View kontrol) {
        LinearLayout baris = new LinearLayout(getContext);
        baris.setOrientation(LinearLayout.HORIZONTAL);
        baris.setGravity(Gravity.CENTER_VERTICAL);
        baris.setBackground(Theme.baris(getContext));
        baris.setMinimumHeight(dp(48));
        baris.setPadding(0, 0, dp(8), 0);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(3), 0, dp(3));
        baris.setLayoutParams(params);

        LinearLayout.LayoutParams ikonParams = new LinearLayout.LayoutParams(dp(40), dp(40));
        ikonParams.setMargins(0, dp(4), dp(8), dp(4));
        baris.addView(new IkonFitur(getContext(), pola, true), ikonParams);

        LinearLayout.LayoutParams isiParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        isiParams.setMargins(0, dp(4), dp(6), dp(4));
        baris.addView(isi, isiParams);

        if (kontrol != null)
            baris.addView(kontrol, new LinearLayout.LayoutParams(dp(52), dp(48)));
        return baris;
    }

    /**
     * Baris posisi pemain: koordinat tile dan dunia dibaca ulang tiap detik
     * selama barisnya terlihat.
     */
    private void BarisPosisi(LinearLayout linLayout, String featName) {
        LinearLayout kolom = new LinearLayout(getContext);
        kolom.setOrientation(LinearLayout.VERTICAL);

        TextView judul = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final TextView nilai = Theme.teks(getContext, "Menunggu posisi...", Theme.DESC_SP,
                Theme.ICON_BOX);
        kolom.addView(judul);
        kolom.addView(nilai);

        final LinearLayout baris = baris(IkonFitur.POSISI, kolom, null);
        linLayout.addView(baris);

        pantauSaatTerpasang(baris, new Runnable() {
            @Override
            public void run() {
                if (baris.isShown())
                    nilai.setText(PosisiTeks());
                baris.postDelayed(this, 1000);
            }
        });
    }

    /** Menjalankan detak hanya selama view menempel di jendela. */
    private void pantauSaatTerpasang(final View view, final Runnable detak) {
        view.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View v) {
                view.removeCallbacks(detak);
                view.postDelayed(detak, 1000);
            }

            @Override
            public void onViewDetachedFromWindow(View v) {
                view.removeCallbacks(detak);
            }
        });
        view.postDelayed(detak, 1000);
    }

    private String PosisiTeks() {
        String data;
        try {
            data = PosisiPemain();
        } catch (Throwable t) {
            data = null;
        }
        if (data == null || data.length() == 0)
            return "Posisi belum terbaca";

        String[] bagian = data.split("\t", -1);
        if (bagian.length < 4)
            return "Posisi belum terbaca";
        if (bagian[0].length() > 0 && bagian[1].length() > 0)
            return "Tile " + bagian[0] + ", " + bagian[1] + "   Dunia " + bagian[2] + ", " + bagian[3];
        return "Dunia " + bagian[2] + ", " + bagian[3];
    }

    private Button teksTombol(CharSequence teks) {
        Button button = new Button(getContext);
        button.setText(teks);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTextSize(Theme.ROW_SP);
        button.setTypeface(Theme.font(getContext));
        button.setAllCaps(false); //Disable caps to support html
        button.setGravity(Gravity.CENTER);
        button.setBackground(null);
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(0);
        button.setMinimumHeight(0);
        button.setPadding(dp(6), 0, dp(6), 0);
        button.setStateListAnimator(null);
        button.setElevation(0f);
        return button;
    }

    private LinearLayout kotakTombol(String[] pola, View isi) {
        LinearLayout kotak = new LinearLayout(getContext);
        kotak.setOrientation(LinearLayout.HORIZONTAL);
        kotak.setGravity(Gravity.CENTER_VERTICAL);
        kotak.setBackground(Theme.tombol(getContext));
        kotak.setPadding(0, 0, dp(8), 0);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, dp(48));
        params.setMargins(0, dp(6), 0, dp(6));
        kotak.setLayoutParams(params);

        LinearLayout.LayoutParams ikonParams = new LinearLayout.LayoutParams(dp(40), dp(40));
        ikonParams.setMargins(0, dp(4), dp(8), dp(4));
        kotak.addView(new IkonFitur(getContext(), pola, true), ikonParams);

        LinearLayout.LayoutParams isiParams = new LinearLayout.LayoutParams(0, MATCH_PARENT, 1f);
        kotak.addView(isi, isiParams);
        return kotak;
    }

    private void tampilkanDialog(AlertDialog.Builder builder) {
        AlertDialog dialog = builder.create();
        if (overlayRequired) {
            Objects.requireNonNull(dialog.getWindow()).setType(
                    Build.VERSION.SDK_INT >= 26 ? 2038 : 2002);
        }
        dialog.show();
        if (dialog.getWindow() != null)
            Theme.applyFont(dialog.getWindow().getDecorView());
    }

    private boolean isViewCollapsed() {
        return rootFrame == null || mCollapsed.getVisibility() == View.VISIBLE;
    }

    //For our image a little converter
    private int convertDipToPixels(int i) {
        return (int) ((((float) i) * getContext.getResources().getDisplayMetrics().density) + 0.5f);
    }

    private int dp(int i) {
        return (int) Theme.dp(getContext, i);
    }

    public void setVisibility(int view) {
        if (rootFrame != null) {
            rootFrame.setVisibility(view);
        }
    }

    public void onDestroy() {
        if (rootFrame != null) {
            mWindowManager.removeView(rootFrame);
        }
    }
}
