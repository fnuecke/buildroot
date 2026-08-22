package li.cil.sedna.buildroot;

import java.io.InputStream;

public final class Buildroot {
    public static InputStream getFirmware() {
        return open("generated/fw_jump.bin");
    }

    public static InputStream getLinuxImage() {
        return open("generated/Image");
    }

    public static InputStream getRootFilesystem() {
        return open("generated/rootfs.ext2");
    }

    private static InputStream open(final String resource) {
        final InputStream stream = Buildroot.class.getClassLoader().getResourceAsStream(resource);
        if (stream == null) {
            throw new IllegalStateException("Missing resource [" + resource + "].");
        }
        return stream;
    }
}
