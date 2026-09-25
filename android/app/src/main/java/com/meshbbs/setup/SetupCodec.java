package com.meshbbs.setup;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.charset.CodingErrorAction;
import java.util.UUID;

public final class SetupCodec {
    public static final UUID SERVICE = UUID.fromString("8f3a0001-6ca2-4d15-9d49-75b974ad2716");
    public static final UUID STATUS = UUID.fromString("8f3a0002-6ca2-4d15-9d49-75b974ad2716");
    public static final UUID SETTINGS = UUID.fromString("8f3a0003-6ca2-4d15-9d49-75b974ad2716");
    public static final UUID RESTART = UUID.fromString("8f3a0004-6ca2-4d15-9d49-75b974ad2716");
    public static final UUID ACTIVITY = UUID.fromString("8f3a0005-6ca2-4d15-9d49-75b974ad2716");
    public static final UUID MESH_SERVICE = UUID.fromString("6ba1b218-15a8-461f-9fa8-5dcae273eafd");
    private SetupCodec() {}

    public static byte[] encode(String target, int pin, int hops) {
        byte[] name = target.getBytes(StandardCharsets.UTF_8);
        if (name.length < 1 || name.length > 63) throw new IllegalArgumentException("Node name/address must be 1–63 UTF-8 bytes.");
        for (int i = 0; i < target.length(); i++) {
            char c = target.charAt(i);
            if (c < 32 || c == 127) throw new IllegalArgumentException("Node name contains a control character.");
            if (Character.isSurrogate(c)) {
                if (!Character.isHighSurrogate(c) || i + 1 == target.length() || !Character.isLowSurrogate(target.charAt(++i)))
                    throw new IllegalArgumentException("Node name contains invalid text.");
            }
        }
        if (pin < -2 || pin > 999999 || hops < 0 || hops > 7) throw new IllegalArgumentException("Invalid PIN or hop limit.");
        return ByteBuffer.allocate(7 + name.length).order(ByteOrder.LITTLE_ENDIAN)
            .put((byte)1).put((byte)hops).put((byte)name.length).putInt(pin).put(name).array();
    }

    public static final class Status {
        public final boolean hasPin, saved;
        public final int hops;
        public final String target;
        Status(boolean p, boolean s, int h, String t) { hasPin=p; saved=s; hops=h; target=t; }
    }
    public static Status decode(byte[] data) {
        if (data == null || data.length < 4 || data[0] != 1 || (data[1] & ~3) != 0 ||
            (data[2] & 255) > 7 || (data[3] & 255) > 63 || data.length != 4 + (data[3] & 255))
            throw new IllegalArgumentException("Unsupported or malformed setup response.");
        try {
            String target = StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(data,4,data.length-4)).toString();
            return new Status((data[1]&1)!=0,(data[1]&2)!=0,data[2]&255,target);
        } catch (java.nio.charset.CharacterCodingException ex) {
            throw new IllegalArgumentException("Board returned invalid text.");
        }
    }
}
