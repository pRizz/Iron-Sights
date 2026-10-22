# Project Midas ADS

## Watch the gameplay demo

[![Watch Project Midas ADS: Dam gameplay and aim-down-sights demo at 60 FPS](.github/images/ads-gameplay-thumbnail.png)](https://youtu.be/pwP-z1nDtEo)

**[Watch on YouTube: Dam Gameplay & ADS Demo (60 FPS)](https://youtu.be/pwP-z1nDtEo)**

Project Midas ADS is an independent fork of [Project Midas](https://github.com/murkantor/Project-Midas), focused on adding aim-down-sights (ADS) controls to the GE PC port. This fork is maintained separately from the upstream Project Midas project.

It adds ADS by default, a choice between ADS and the original red reticle aiming mode, half-speed movement while aiming, and individual sight alignment for 20 supported firearms.

The underlying port is a work in progress built from the N64 decompilation. It runs on Windows and Linux and reads its game data from your own copy of the US cart.

## AI Disclaimer

Claude was used in the making of this project. If you don't agree with this, please do not download either the executables or the source.

## Playing

1. Put your GE (USA) cart image in the `Data` folder next to the executable. See `Data/README.txt` for the accepted formats and other ways to point the game at it.
2. Run `ProjectMidas.exe` (Windows) or `ProjectMidas` (Linux).

Files next to the executable:

| Path | What it is |
| --- | --- |
| `midas.ini` | Settings. Written on first run; delete it to restore the defaults. See `midas_ini_help.txt`. |
| `Data/` | Your cart image goes here. |
| `Data/Saves/eeprom.bin` | Save file. |
| `Data/Profiles/` | Player profiles (`.geprofile`) and lifetime multiplayer stats (`.gestats`). |
| `Data/WeaponSets/` | Custom multiplayer weapon sets (`.gewset`). |
| `Data/MatchSettings/` | Saved multiplayer settings (`.gemp`), from Load / Save Settings. |
| `Data/Devices/` | Per-device settings (`.gedevice`). Keyboard and mouse binds are in `kbm.gedevice`. |
| `gamecontrollerdb.txt` | SDL controller mappings. |
| `gamecontrollerdefault.txt` | Built-in controller layouts. |
| `gamecontrolleruser.txt` | Your saved controller layouts. |
| `Data/Logs/ge_direct.log` | The run log on Windows (the game opens no console window). |
| `Data/Logs/crash.log` | Written if the game crashes. |
| `Mods/` | Reserved for modded carts. This build loads the stock cart only. |

An older `ge.ini` is renamed to `midas.ini` automatically on first run.

**Mouse Right** aims down the sights by default. Change **Gameplay > General
Settings > Aim Mode** to **Red Reticle** for the original aiming mode. The
existing **Aim Control** setting selects hold or toggle for either mode.
**Mouse Middle** remains a separate hold-to-ADS binding, configurable in the
controls menu, including for controllers.
You can walk and strafe at half speed while aiming down sights.
ADS adds basic low-poly hands and forearms to the 13 firearms that lack them,
including the KF7 Soviet. Pistols retain their original hands.
Each firearm's ADS pose has a slight upward angle to reveal its front sight
and give a view along the top of the gun.
Sniper ADS raises the rifle into a zoomed scope view with the gun and hands hidden
and a centered red crosshair. Mouse movement turns the view while scoped;
the existing zoom controls still adjust magnification.

## Building

The build runs in Docker, which has every tool and library both builds need. You also need the GE (USA) cart as `baserom.u.z64` in the repository root:

```
sha1: abe01e4aeb033b6c0836819f549c791b26cfde83
```

### On Windows: use WSL

The commands below are for a Linux shell. On Windows, run them in WSL:

1. Install WSL with Ubuntu (`wsl --install` in PowerShell), and turn on WSL integration in Docker Desktop (Settings > Resources > WSL integration).
2. Open the Ubuntu terminal and clone the repository inside WSL, not under `/mnt/c` (much faster):
   ```
   cd ~
   git clone https://github.com/pRizz/Project-Midas.git
   cd Project-Midas
   ```
3. Copy your cart in, for example from your Windows Downloads folder:
   ```
   cp /mnt/c/Users/<you>/Downloads/baserom.u.z64 .
   ```

### Build steps

1. Build the image (once):
   ```
   docker build --network=host --build-arg HOST_UID=$(id -u) --build-arg HOST_GID=$(id -g) -t goldeneye .
   ```
2. Start a container in the repository:
   ```
   docker run --rm -it -v $(pwd):/home/dev/project goldeneye
   ```
3. Extract the assets (once):
   ```
   ./scripts/extract_baserom.u.sh
   make convert_props convert_chrs convert_guns combine_images
   ```
4. Build for Windows (x86):
   ```
   cmake -B build_win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DSDL2_MINGW_ROOT=$HOME/sdl2-mingw/SDL2-2.30.11/i686-w64-mingw32
   cmake --build build_win -j$(nproc)
   ```
5. Build for Linux:
   ```
   cmake -B build_pc -DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DSDL2_LINUX_ROOT=$HOME/sdl2-linux-i686
   cmake --build build_pc -j$(nproc)
   ```

| Build | Game | Next to it |
| --- | --- | --- |
| Windows | `build_win/ProjectMidas.exe` | `SDL2.dll`, the controller files, `Data/` |
| Linux | `build_pc/ProjectMidas` | the controller files, `Data/` |

To play, put a copy of `baserom.u.z64` in that build's `Data/` folder. On Windows, copy the whole `build_win` folder somewhere in Windows (from WSL: `explorer.exe build_win`) and run `ProjectMidas.exe` there. The Linux build needs 32-bit SDL2 and OpenGL on the machine that runs it. On Ubuntu or Debian:

```
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libsdl2-2.0-0:i386 libgl1:i386
```

## Known issues

- On some Linux systems, levels draw as a few large blank triangles. Starting the game with these environment variables works around it:
  ```
  GE_FULLHEIGHT=0 GE_EDGEINSET=1 ./ProjectMidas
  ```

## Contributing

For ADS changes and bug reports, use [this fork's issues](https://github.com/pRizz/Project-Midas/issues) and pull requests. The inherited [CONTRIBUTING.md](CONTRIBUTING.md) describes upstream contribution guidelines, including the rules for AI-assisted changes. General port development is coordinated in [upstream Project Midas](https://github.com/murkantor/Project-Midas).

## License

Project Midas ADS retains the upstream Project Midas port code's MIT license and attribution; see [LICENSE](LICENSE). The license does not cover the decompiled game code, GE itself or its assets. Bundled third-party code keeps its own license; see `tools/THIRD_PARTY_NOTICES.txt`.

## Acknowledgements

Project Midas ADS builds on [Project Midas](https://github.com/murkantor/Project-Midas) by murkantor and contributors, and the work of the people and projects credited below.

- [GoldenEye decompilation](https://github.com/n64decomp/007) by KholdFuzion and contributors
- [GoldenEye docs](https://github.com/kholdfuzion/goldeneye_docs)
- [Perfect Dark decompilation](https://github.com/n64decomp/perfect_dark) by Ryan Dwyer and contributors
- [Perfect Dark PC port](https://github.com/fgsfdsfgs/perfect_dark) by fgsfds
- [Perfect Dark netplay](https://github.com/murkantor/perfect_dark_netplay) (fork of fgsfds' `port-net`)
- [GE Plus](https://github.com/Joshua-1248/GoldenEye-007-Plus) by Joshua
- *The DOOM III Network Architecture* by J.M.P. van Waveren (id Software, 2006)
- Fast3D renderer by Emill and MaikelChan (MIT)
- [SDL2](https://github.com/libsdl-org/SDL) and [glad](https://github.com/Dav1dde/glad)
- [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)
- [ENet](https://github.com/lsalzman/enet)
- [xdelta3](https://github.com/jmacd/xdelta)
- [ido-static-recomp](https://github.com/decompals/ido-static-recomp), [rabbitizer](https://github.com/Decompollaboration/rabbitizer), [qemu-irix](https://github.com/n64decomp/qemu-irix) and [armips](https://github.com/Kingcom/armips)
- The 1964 emulator and its GE/Perfect Dark mouse injector
- [pj64raphnetraw](https://github.com/raphnet/pj64raphnetraw) by Raphael Assenat ([raphnet-tech](https://www.raphnet-tech.com)), the reference for the raphnet adapter protocol

GE is owned by Rare, Nintendo and the Bond rights holders. This project contains no game assets.
