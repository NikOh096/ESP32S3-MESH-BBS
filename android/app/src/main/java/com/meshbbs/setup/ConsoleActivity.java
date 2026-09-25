package com.meshbbs.setup;
import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.*;

public class ConsoleActivity extends Activity {
    private final Handler ui=new Handler(Looper.getMainLooper());
    private TextView content;
    private ScrollView scroll;
    private CheckBox follow;
    private long revision=-1;
    private final Runnable update=new Runnable(){public void run(){
        long current=ActivityLog.revision();
        if(current!=revision){
            revision=current; String log=ActivityLog.snapshot();
            content.setText(log.isEmpty()?"No activity yet. Return to setup and start a scan.":log);
            if(follow.isChecked())scroll.post(()->scroll.fullScroll(ScrollView.FOCUS_DOWN));
        }
        ui.postDelayed(this,250);
    }};
    private int dp(int n){return Math.round(n*getResources().getDisplayMetrics().density);}
    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(0xff0c1825);root.setPadding(dp(16),dp(16),dp(16),dp(16));
        setContentView(root);
        root.setOnApplyWindowInsetsListener((v,i)->{v.setPadding(dp(16)+i.getSystemWindowInsetLeft(),dp(16)+i.getSystemWindowInsetTop(),dp(16)+i.getSystemWindowInsetRight(),dp(16)+i.getSystemWindowInsetBottom());return i;});
        Button back=new Button(this);back.setText("Back to setup");back.setAllCaps(false);back.setOnClickListener(v->finish());root.addView(back);
        TextView title=new TextView(this);title.setText("Activity console");title.setTextSize(24);title.setTextColor(0xffe7f0f7);root.addView(title);
        TextView help=new TextView(this);help.setText("Live app events and imported BBS activity. Use Read saved BBS activity on the setup screen to fetch the board's latest report. BBS entries are a saved snapshot; setup pauses its node connection. PINs and message bodies are not logged.");help.setTextColor(0xffaac0cf);help.setPadding(0,dp(8),0,dp(12));root.addView(help);
        LinearLayout buttons=new LinearLayout(this);root.addView(buttons);
        Button copy=new Button(this);copy.setText("Copy");copy.setOnClickListener(v->{((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("MESHBBS activity",ActivityLog.snapshot()));Toast.makeText(this,"Activity copied",Toast.LENGTH_SHORT).show();});buttons.addView(copy,new LinearLayout.LayoutParams(0,-2,1));
        Button clear=new Button(this);clear.setText("Clear");clear.setOnClickListener(v->ActivityLog.clear());buttons.addView(clear,new LinearLayout.LayoutParams(0,-2,1));
        follow=new CheckBox(this);follow.setText("Follow latest");follow.setChecked(true);follow.setTextColor(0xffe7f0f7);root.addView(follow);
        scroll=new ScrollView(this);root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        content=new TextView(this);content.setTypeface(Typeface.MONOSPACE);content.setTextSize(12);content.setTextColor(0xff58dac2);content.setTextIsSelectable(true);content.setPadding(0,dp(8),0,dp(12));scroll.addView(content);
    }
    @Override protected void onStart(){super.onStart();ui.post(update);}
    @Override protected void onStop(){ui.removeCallbacks(update);super.onStop();}
}
