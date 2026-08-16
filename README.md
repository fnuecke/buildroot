# Sedna Buildroot

A [Buildroot](https://buildroot.org) fork to bake RISC-V Linux images we can use in [Sedna](https://github.com/fnuecke/sedna). Goal is to keep them as small as possible, since my main end-usecase is using them in a Minecraft mod. The images contain: OpenSBI firmware, a kernel image and an
ext2 root filesystem.

The Gradle wrapper packages `make`'s output as a jar of resources, accessible via `li.cil.sedna.buildroot.Buildroot`.

## Maven

Published to a static Maven repo (via GitHub Pages), since this is mostly binary blobs, licenses are a wild mix and the build isn't byte-reproducible (mkfs does mkfs things I guess).

```kotlin
repositories {
    exclusiveContent {
        forRepository { maven("https://fnuecke.github.io/maven") }
        filter { includeModule("li.cil.sedna", "sedna-buildroot") }
    }
}

dependencies {
    implementation("li.cil.sedna:sedna-buildroot:0.0.9")
}
```

## Building

Based on Buildroot 2024.02.3 (Linux 6.6, GCC 12.3, OpenSBI 1.3). Since we want a small kernel and rootfs and things just keep growing, we stay a few releases behind rather than tracking the latest.

Buildroot of this vintage does not build on a modern host — the toolchain and several host packages don't like current glibc. `./gradlew build` therefore runs `make` inside the container Buildroot's own CI used, pinned in [.gitlab-ci.yml](.gitlab-ci.yml); override it with `-PbuildrootDockerImage=…`.

Our config is [configs/sedna-riscv64_defconfig](configs/sedna-riscv64_defconfig). Run `./config-sedna.sh` to regenerate the top-level `.config` from it.
