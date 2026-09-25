package com.meshbbs.setup;
import java.util.Collection;
import java.util.Locale;
import java.util.UUID;

public final class DeviceDiscovery {
    private DeviceDiscovery() {}
    public static boolean isBoard(String name,Collection<UUID> services) {
        return services.contains(SetupCodec.SERVICE)||name.toUpperCase(Locale.ROOT).startsWith("MESHBBS-");
    }
    public static boolean isNode(String name,Collection<UUID> services,boolean showAll) {
        if(isBoard(name,services))return false;
        return showAll||services.contains(SetupCodec.MESH_SERVICE)||name.toLowerCase(Locale.ROOT).startsWith("meshtastic");
    }
}
