# Hob2D
A 2D game engine written in C++.

![gif](https://raw.githubusercontent.com/dbrizov/hob2d/refs/heads/master/docs/media/hob2d.gif)

# Requirements
Project dependencies are downloaded and linked automatically via `vcpkg` and `CMake`.<br>
A C/C++ compiler with C++20 support (e.g. MSVC, GCC, or Clang)

## Windows
### vcpkg
1. Clone the `vcpkg` git repository.
```
git clone https://github.com/microsoft/vcpkg.git vcpkg
```
2. Run the `vcpkg` boostrap script.
```
cd vcpkg
.\bootstrap-vcpkg.bat
```
3. Set the `VCPKG_ROOT` environment variable and add it to `PATH`.
```
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
$env:PATH = "$env:VCPKG_ROOT;$env:PATH"
```
> [!NOTE]
> Setting environment variables in this manner only affects the current terminal session. To make these changes permanent across all sessions, set them through the Windows System Environment Variables panel.

4. Open a new terminal and verify.
```
vcpkg --version
```

### CMake
Go to [cmake.org](https://cmake.org/) and download the `x86-x64` Windows installer.<br>
The `cmake` environment variable will be configured automatically.

## GNU/Linux
1. Install prerequisites.
```
sudo apt update
sudo apt install build-essential cmake ninja-build pkgconf curl zip unzip tar
```
2. Install dependencies for building `SDL3` from source.

On Linux SDL3 requires additional features for integration with the desktop stack - `sdl3[core,ibus,x11,wayland,alsa]`.<br>
In order for `vcpkg` to build them from source you need to install additional dependencies.
```
sudo apt install python3 python3-venv python3-pip
sudo apt install autoconf autoconf-archive automake libtool
sudo apt install libibus-1.0-dev
sudo apt install libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxfixes-dev libxrender-dev libxss-dev libxxf86vm-dev libxtst-dev
sudo apt install libwayland-dev wayland-protocols libxkbcommon-dev libegl1-mesa-dev libdecor-0-dev
sudo apt install libasound2-dev libpulse-dev libpipewire-0.3-dev libjack-jackd2-dev
```
3. Clone the `vcpkg` git repository.
```
git clone https://github.com/microsoft/vcpkg.git ~/.vcpkg
```
4. Run the `vcpkg` boostrap script.
```
cd ~/.vcpkg
./bootstrap-vcpkg.sh
```
5. Set the `VCPKG_ROOT` environment variable and add it to `PATH`.
```
echo 'export VCPKG_ROOT="$HOME/.vcpkg"' >> ~/.bashrc
echo 'export PATH="$PATH:$VCPKG_ROOT"' >> ~/.bashrc
```
6. Reload and verify.
```
source ~/.bashrc
vcpkg --version
```

## macOS
1. Install prerequisites.
```
xcode-select --install
brew install cmake ninja pkgconf
```
2. Clone the `vcpkg` git repository.
```
git clone https://github.com/microsoft/vcpkg.git ~/.vcpkg
```
3. Run the `vcpkg` boostrap script.
```
cd ~/.vcpkg
./bootstrap-vcpkg.sh
```
4. Set the `VCPKG_ROOT` environment variable and add it to `PATH`.
```
echo 'export VCPKG_ROOT="$HOME/.vcpkg"' >> ~/.zshrc
echo 'export PATH="$PATH:$VCPKG_ROOT"' >> ~/.zshrc
```
5. Reload and verify.
```
source ~/.zshrc
vcpkg --version
```

# Build & Run
Configure with the preset for your toolchain, build, then run the executable.
```
# Configure (pick one preset)
cmake --preset debug-msvc      # Windows / MSVC
cmake --preset debug-gcc       # Linux / GCC
cmake --preset debug-clang     # macOS / Clang

# Build
cmake --build --preset windows-debug-msvc-x64

# Run (no arg loads the default project, 'sandbox')
./build/debug/windows-msvc/Debug/hob2d.exe

# Run a specific game project (bare name, or a path relative to the repo root)
./build/debug/windows-msvc/Debug/hob2d.exe --project sandbox
```

The engine loads one game project from `content/projects/<name>/` at launch. The bundled sample is
`content/projects/sandbox/`. Set the build's default game at configure time with `-DHOB_PROJECT=<name>`,
or select it at runtime with `--project <name>` (a bare name resolves under `content/projects/`).
