# Seed Quarry build guide

Seed Quarry is a Qt Widgets application. Release builds use Qt 6.8, while
the source remains compatible with Qt 5.15. A C/C++ compiler with GNU language
extensions is required because Cubiomes relies on GCC/Clang attributes and
wrapping signed arithmetic.

Obtain the complete Seed Quarry source archive. It already contains all bundled
dependencies required by the build.

```sh
unzip Seed-Quarry-4.2.dev0-Source.zip
cd Seed-Quarry-4.2.dev0-Source
```

## macOS

Install the Xcode command-line tools and Qt 6, then build out of tree:

```sh
xcode-select --install
brew install qt
mkdir -p build-macos
cd build-macos
$(brew --prefix qt)/bin/qmake CONFIG+=release ../seed-quarry.pro
make -j"$(sysctl -n hw.logicalcpu)"
```

The result is `seed-quarry.app`. Create a self-contained bundle and DMG
with Qt's deployment tool:

```sh
$(brew --prefix qt)/bin/macdeployqt seed-quarry.app -dmg
```

Official CI builds a universal `x86_64` + `arm64` application by additionally
passing `QMAKE_APPLE_DEVICE_ARCHS="x86_64 arm64"` to qmake. The project uses
Apple `libtool` for its embedded Cubiomes archive so both architectures remain
intact in the resulting application.

## Windows

Install Qt 6.8 with the 64-bit MinGW 13.1 component. Open the matching Qt MinGW
terminal and run:

```bat
mkdir build-windows
cd build-windows
qmake CONFIG+=release ..\seed-quarry.pro
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

Create a portable folder containing the executable, Qt libraries, the Windows
platform plugin, and the MinGW runtime:

```bat
mkdir seed-quarry
copy release\seed-quarry.exe seed-quarry\
windeployqt --release --compiler-runtime --no-translations --dir seed-quarry seed-quarry\seed-quarry.exe
```

`windeployqt` and `macdeployqt` are the supported Qt deployment paths. Do not
manually copy only the Qt libraries: GUI applications also need the platform
plugin (`qwindows.dll` on Windows and `libqcocoa.dylib` on macOS).

## Tests

The version, biome, spawn, and structure regressions do not require Qt:

```sh
make -C cubiomes test-versions
```

Use `mingw32-make` instead of `make` on Windows. Both release workflows run
this suite before compiling and packaging the GUI.
