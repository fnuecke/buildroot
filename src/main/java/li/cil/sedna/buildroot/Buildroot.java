package li.cil.sedna.buildroot;

import java.io.IOException;
import java.io.InputStream;
import java.io.UncheckedIOException;
import java.util.Properties;

public final class Buildroot {
    public static InputStream getSednaFirmware() {
        return open("generated/firmware.bin");
    }

    public static int getSednaFirmwareRegionSize() {
        final Properties properties = new Properties();
        try (final InputStream stream = open("generated/firmware.properties")) {
            properties.load(stream);
        } catch (final IOException e) {
            throw new UncheckedIOException(e);
        }
        final String value = properties.getProperty("regionSize");
        if (value == null) {
            throw new IllegalStateException("Missing firmware region size.");
        }
        return Integer.parseInt(value);
    }

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
