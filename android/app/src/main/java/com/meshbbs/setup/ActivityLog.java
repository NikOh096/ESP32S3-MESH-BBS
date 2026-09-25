package com.meshbbs.setup;
import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.Date;
import java.util.Locale;

/** RAM-only diagnostics. Callers must never supply a PIN or raw settings payload. */
public final class ActivityLog {
    private static final ArrayDeque<String> lines=new ArrayDeque<>();
    private static long revision;
    private ActivityLog(){}
    public static synchronized void add(String category,String text) {
        String time=new SimpleDateFormat("HH:mm:ss",Locale.ROOT).format(new Date());
        lines.addLast(time+" ["+category+"] "+text.replace('\n',' ').replace('\r',' '));
        while(lines.size()>300)lines.removeFirst();
        revision++;
    }
    public static synchronized void clear(){lines.clear();revision++;}
    public static synchronized long revision(){return revision;}
    public static synchronized String snapshot(){return String.join("\n",lines);}
}
