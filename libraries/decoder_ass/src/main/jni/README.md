# Build libass for Android

The following script, `build_libass.sh`, as the name suggests, builds libass and its dependencies
for Android platforms. It automates the process of cross-compiling libass and all its dependencies
for Android. This enables Android applications to render complex subtitle formats with advanced
styling and positioning.

The script builds the following libraries in sequence:

1. [HarfBuzz (v11.0.0)](https://github.com/harfbuzz/harfbuzz) - An OpenType text shaping engine
2. [FreeType (v2.13.3)](https://freetype.org/) - A font rendering library
3. [FriBidi (v1.0.16)](https://github.com/fribidi/fribidi/) - A library implementing the Unicode
   Bidirectional Algorithm
4. [UniBreak (v6.1)](https://github.com/adah1972/libunibreak/) - A line breaking library
   implementing the Unicode Line Breaking Algorithm
5. [Expat (v2.7.1)](https://github.com/libexpat/libexpat) - An XML parser library
6. [Fontconfig (v2.16.0)](https://gitlab.freedesktop.org/fontconfig/fontconfig) - A library for font
   customization and configuration
7. [libass (v0.17.3)](https://github.com/libass/libass) - The subtitle rendering library

All libraries are built as static libraries for four Android architectures:

* x86_64
* x86 (i686)
* armeabi-v7a (armv7a)
* arm64-v8a (aarch64)

## Build instructions

### Prerequisites

Before running the build script for libass, you will need the following dependencies installed on
your system. You may use your whatever package manager to install these.

* build-essential (or equivalent build tools on non-Debian systems)
* pkg-config
* autoconf
* automake
* libtool
* wget
* meson and its dependencies (ninja-build, python3, etc.)

### Building on Linux or macOS

1. In a terminal, navigate to this current directory, to make sure the build of libass is done in
   the correct directory, as it will create a `ass` directory right directly inside `jni`:

   ```bash
   cd your_path\media\libraries\decoder_ass\src\main\jni
   ```

2. Locate the path of your Android NDK. You can manually download it from the
   official [Android NDK website](https://developer.android.com/ndk/downloads):

   ```bash
   NDK_PATH="<path to Android NDK>"
   ```

3. Set the host platform:

    * For Linux:

   ```bash
   HOST_PLATFORM="linux-x86_64"
   ```

    * For macOS:

   ```bash
   HOST_PLATFORM="darwin-x86_64"
   ```

4. Set the ABI version for native code (typically equal to your minSdk, and must not exceed it):

   ```bash
   ANDROID_ABI_VERSION=21
   ```

5. Execute `build_libass.sh` to build libass for `armeabi-v7a`, `arm64-v8a`,
   `x86` and `x86_64`. The script can be edited if you need to build for
   different architectures:

   ```bash
   ./build_libass.sh "${NDK_PATH}" "${HOST_PLATFORM}" "${ANDROID_ABI_VERSION}"
   ```

## Build instructions (Windows)

We do not provide official support for building this module on Windows. However, it should be
possible to follow the Linux instructions
using [WSL2](https://learn.microsoft.com/en-us/windows/wsl/install) with the appropriate tools
installed.

By doing so, make sure to download the [Android NDK](https://developer.android.com/ndk/downloads)
for linux, and set the NDK_PATH to the location of the NDK in your WSL2 environment.

## Troubleshooting

If you encounter issues during the build process:

1. Ensure all prerequisites are properly installed
2. Verify your NDK path is correct and the NDK version is compatible
3. Check that the HOST_PLATFORM matches your system
4. Make sure ANDROID_ABI_VERSION is appropriate for your project

For issues with specific dependencies, individual build functions in the script can be modified as
needed.