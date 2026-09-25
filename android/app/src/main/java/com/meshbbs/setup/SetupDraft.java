package com.meshbbs.setup;

/** Phone-local draft. A failed or delayed board connection never blocks editing. */
public final class SetupDraft {
    public String target="", pin="";
    public int hops=3;
    public boolean noPin, edited;
    private boolean savedHasPin;
    private String savedTarget="";

    public void edit(String target, String pin, int hops, boolean noPin) {
        if (!this.target.equals(target)||!this.pin.equals(pin)||this.hops!=hops||this.noPin!=noPin) edited=true;
        this.target=target; this.pin=pin; this.hops=hops; this.noPin=noPin;
    }
    public boolean acceptRemote(SetupCodec.Status status, boolean replaceDraft) {
        savedHasPin=status.hasPin; savedTarget=status.target;
        if (edited&&!replaceDraft) return false;
        target=status.target; pin=""; hops=status.hops;
        noPin=!status.hasPin&&!status.target.isEmpty(); edited=false;
        return true;
    }
    public boolean canKeepPin() { return savedHasPin&&target.trim().equals(savedTarget); }
    public void disconnected() { savedHasPin=false; savedTarget=""; }
    public int pinValue() {
        if(noPin)return -1;
        if(pin.isEmpty()&&canKeepPin())return -2;
        if(!pin.matches("[0-9]{6}"))throw new IllegalArgumentException("Enter this node's six-digit PIN, or select no PIN if it uses that mode.");
        return Integer.parseInt(pin);
    }
    public byte[] encode() { return SetupCodec.encode(target.trim(),pinValue(),hops); }
    public static boolean editingAllowed(String operation) {
        return !operation.equals("save")&&!operation.equals("verify")&&!operation.equals("restart");
    }
}
