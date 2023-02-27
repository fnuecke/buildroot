package li.cil.sedna.buildroot;

import java.io.InputStream;

public final class Buildroot {
    public static InputStream getFirmware() {
        return Buildroot.class.getClassLoader().getResourceAsStream("generated/fw_jump.bin");
    }

    public static InputStream getLinuxImage() {
        return Buildroot.class.getClassLoader().getResourceAsStream("generated/Image");
    }

    public static InputStream getRootFilesystem() {
        return Buildroot.class.getClassLoader().getResourceAsStream("generated/rootfs.ext2");
    }
}
