![Azahar Emulator](https://azahar-emu.org/resources/images/logo/azahar-name-and-logo.svg)

# Extendo — Switch UI Fork

This is Extendo, a personal fork of [Azahar](https://github.com/azahar-emu/azahar), a free and open-source
high-level Nintendo 3DS emulator. This fork replaces the stock game list with a custom
Switch-style home screen (tile shelf + pill dock bar), adds custom cover art, a custom
background, and an in-window settings popup.

*Extendo is not affiliated with or endorsed by Nintendo. This fork is not affiliated with the
upstream Azahar project.*

Repo: https://github.com/yuske014/Extendo

---

# Build from source

There are no prebuilt releases of this fork — build it yourself with the commands below. Pick
your OS.

## Requirements (all platforms)

- [Git](https://git-scm.com/) and [Git LFS](https://git-lfs.com/) (`git lfs install` after
  installing — one submodule needs it)
- [CMake](https://cmake.org/) 3.25+
- [Ninja](https://ninja-build.org/)
- A C++20 compiler (Clang/AppleClang on macOS, GCC/Clang on Linux, MSVC on Windows)
- [Qt 6](https://www.qt.io/) (6.5+)

## 1. Clone the repo (same on every OS)

```sh
git clone https://github.com/yuske014/Extendo.git extendo
cd extendo
git submodule update --init --recursive
```

---

## macOS

```sh
# Dependencies
xcode-select --install          # Xcode command line tools (compiler)
brew install cmake ninja qt@6

# Configure + build (arm64 Apple Silicon; use x86_64 on Intel Macs)
mkdir -p build/arm64 && cd build/arm64
cmake ../.. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64" \
    -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" \
    -DENABLE_LTO=OFF
ninja

# Build the .app bundle
export PATH="$(brew --prefix qt@6)/bin:$PATH"
ninja bundle

# Run it
open bundle/extendo.app
```

> First launch: macOS will warn the app is from an unidentified developer (it's unsigned).
> Right-click the app → Open, or run `xattr -cr bundle/extendo.app` once before launching.

---

## Linux (Debian/Ubuntu)

```sh
# Dependencies
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config git git-lfs \
    qt6-base-dev qt6-multimedia-dev qt6-tools-dev qt6-tools-dev-tools \
    libqt6svg6-dev libsdl2-dev libssl-dev libzip-dev libpng-dev \
    libvulkan-dev mesa-common-dev

# Configure + build
mkdir build && cd build
cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_LTO=OFF
ninja

# Run it
./bin/Release/extendo
```

For other distros, install the equivalent Qt6/SDL2/Vulkan dev packages via your package
manager (`dnf`, `pacman`, `zypper`, etc.) — same package names, minus the `-dev` suffix
convention differences (e.g. Arch uses plain `qt6-base`, `sdl2`).

---

## Windows (PowerShell)

The most reliable path is MSVC + [vcpkg](https://github.com/microsoft/vcpkg) for dependencies.

```powershell
# 1. Install Visual Studio Build Tools (includes MSVC) if you don't have it:
#    https://visualstudio.microsoft.com/visual-cpp-build-tools/
#    -> select "Desktop development with C++"

# 2. Install CMake, Ninja, Qt6, Git, Git LFS (via winget)
winget install Kitware.CMake Ninja-build.Ninja Git.Git qt.qt6 GitHub.GitLFS

# 3. Set up vcpkg (for SDL2/zlib/etc.)
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# 4. Configure + build (run from the extendo repo root, in a "Developer PowerShell for VS")
mkdir build; cd build
cmake .. -GNinja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
    -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64" `
    -DENABLE_FFMPEG=OFF `
    -DENABLE_LTO=OFF
ninja

# Run it
.\bin\Release\extendo.exe
```

> Adjust the Qt path (`C:\Qt\6.x.x\msvc2022_64`) to match whatever version `winget` installed —
> check `C:\Qt` after step 2. `-DENABLE_FFMPEG=OFF` skips the heavier FFmpeg/vcpkg build; drop
> it (and `vcpkg install ffmpeg[...]:x64-windows-release` first) if you want video dumping
> support.

---

# Notes on this fork

- First launch creates a `user/` folder next to Azahar's normal config directory
  (`~/Library/Application Support/Azahar/user/` on macOS, `~/.local/share/azahar/user/` on
  Linux, `%APPDATA%\Azahar\user\` on Windows) — custom covers, the home screen background, and
  the live-editable home screen stylesheet live there.
- Right-click a game tile to set/remove a custom cover image.
- The pill bar at the bottom of the home screen has: Profile, Settings, Add Game Directory,
  Wallpaper, Power.

---

# Upstream project

This fork tracks [azahar-emu/azahar](https://github.com/azahar-emu/azahar). See their
[wiki](https://github.com/azahar-emu/azahar/wiki) for emulator usage docs, compatibility
reporting, and general 3DS emulation questions unrelated to this fork's UI changes.
