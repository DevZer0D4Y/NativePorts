# NativePorts for iOS and Windows

Native iPhone, iPad and Windows ports of five classic games, each running its original program through an embedded emulator core. They share the frontend of my [NBA Hangtime port](https://github.com/DevZer0D4Y/NBAHangtime-iOS): touch and controller support, a NewPixie CRT filter and a customizable touch layout.

| Game | Original | Core | Players | Game file | Version |
|---|---|---|---:|---|---|
| **Super Street Fighter II Turbo** | Capcom CPS2 arcade, Euro 940223 | [MAME 2003-Plus](https://github.com/libretro/mame2003-plus-libretro) | 2 | `ssf2t.zip` | 0.3.0 |
| **Ultimate Mortal Kombat 3** | Midway Wolf-unit arcade, rev 1.2 | [MAME 2003-Plus](https://github.com/libretro/mame2003-plus-libretro) | 2 | `umk3.zip` | 0.3.0 |
| **Slam 'n Jam '96** | PlayStation | [PCSX ReARMed](https://github.com/libretro/pcsx_rearmed) | 4 | `slamjam96.chd` | 0.3.0 |
| **Tony Hawk's Pro Skater 2** | Dreamcast | [Flycast](https://github.com/libretro/flycast) | 2 | `thps2.chd` | 0.3.0 |
| **NBA Showtime: NBA on NBC** | PlayStation USA | [PCSX ReARMed](https://github.com/libretro/pcsx_rearmed) | 4 | `nbashowt.bin` | 0.3.0 |

**NBA Showtime uses the supplied PlayStation USA edition (SLUS-00948), not the arcade/MAME edition.** Extract its BIN/CUE archive and import the raw MODE2/2352 BIN as `nbashowt.bin`.

**You need your own copy of each game. ROMs and disc images are not included.**

**[Latest release](https://github.com/DevZer0D4Y/NativePorts/releases/latest)**

[Download all five Windows builds](https://github.com/DevZer0D4Y/NativePorts/releases/download/v0.3.0/NativePorts-Windows-x64.zip)

| Game | iOS IPA |
|---|---|
| SSF2T 0.3.0 | [Download](https://github.com/DevZer0D4Y/NativePorts/releases/download/v0.3.0/SSF2T-0.3.0-unsigned.ipa) |
| Slam n Jam 0.3.0 | [Download](https://github.com/DevZer0D4Y/NativePorts/releases/download/v0.3.0/SlamJam96-0.3.0-unsigned.ipa) |
| UMK3 0.3.0 | [Download](https://github.com/DevZer0D4Y/NativePorts/releases/download/v0.3.0/UMK3-0.3.0-unsigned.ipa) |
| NBA Showtime 0.3.0 | [Download](https://github.com/DevZer0D4Y/NativePorts/releases/download/v0.3.0/NBAShowtime-0.3.0-unsigned.ipa) |
| THPS2 0.3.0 | [Download](https://github.com/DevZer0D4Y/NativePorts/releases/download/v0.3.0/THPS2-0.3.0-unsigned.ipa) |


If you want to support me and remain up-to-date about this and other projects, you can in different ways:

Ko-Fi: https://ko-fi.com/dev_zer0

Discord: https://discord.gg/uFChheZEWX

YouTube: https://www.youtube.com/@develop_erZ

## Features

- The original games, with their own graphics and sound, running through a libretro core built into each app. Nothing is downloaded at runtime.
- On-screen touch controls with a floating stick, haptics and a customizable layout. Each port has its own six action buttons, labelled for its game (punches and kicks, ollie, flip, grind and grab, and so on).
- Controller support: Xbox, PlayStation, MFi and Switch Pro on iOS, and XInput and SDL gamepads on Windows, with remappable buttons. Two players for SSF2T, UMK3 and THPS2, four for Slam 'n Jam '96 and NBA Showtime.
- **True widescreen** in all five games, on by default: more of the stage or court is shown during play, with nothing stretched. See [Widescreen](#widescreen).
- **NewPixie CRT filter**, on by default, with sharp or smooth pixels and ARCADE, SQUARE or FULL screen modes. COLOR SIDES fills the unused side areas with color instead of black.
- The arcade ROM sets are checked by checksum when they're imported, whatever the files are called. Missing or wrong files are listed.
- High scores, saves and settings are kept automatically. The game pauses when you switch apps or lock the phone.
- A Windows build of each game with keyboard and gamepad support, remappable keys, fullscreen and drag-and-drop loading.

## Requirements

- **iOS**: an iPhone or iPad running **iOS/iPadOS 16 or later**, and a sideloading tool to sign and install the IPA, such as ESign, AltStore, SideStore or Sideloadly.
- **Windows**: Windows 10 or 11, 64-bit.
- Your own game files:

| Game | What you need |
|---|---|
| Super Street Fighter II Turbo | MAME arcade ROM set `ssf2t` (Euro 940223), as a ZIP with the 24 files listed in [`ports/ssf2t/ios/Hangtime/ROMAudit.c`](ports/ssf2t/ios/Hangtime/ROMAudit.c) |
| Ultimate Mortal Kombat 3 | MAME arcade ROM set `umk3` (rev 1.2), as a ZIP with the 26 files listed in [`ports/umk3/rom-manifest.json`](ports/umk3/rom-manifest.json) |
| Slam 'n Jam '96 | A CHD image of the PlayStation disc |
| Tony Hawk's Pro Skater 2 | A CHD image of the Dreamcast disc. No Dreamcast BIOS is needed. |
| NBA Showtime | PlayStation USA raw MODE2/2352 BIN (`nbashowt.bin`, SLUS-00948). |

## Installation

### iPhone and iPad

1. Download the IPA for your game (for example `SSF2T-0.3.0-unsigned.ipa`) from the [latest release](https://github.com/DevZer0D4Y/NativePorts/releases/latest), or [build it from source](#building).
2. Sign and install it with your sideloading tool. With ESign, import the IPA, then sign and install it inside ESign.
3. Open the app, tap **IMPORT** (for example **IMPORT ssf2t.zip**) and pick your game file in the Files app. You can also copy it, under the name from the table above, into **Files → On My iPhone → *the app's folder***, then quit and reopen the app.

Each game is a separate app with its own bundle identifier: `com.devz.ssf2t`, `com.devz.umk3`, `com.devz.slamjam96`, `com.devz.thps2` and `com.devz.nbashowt`. To update an existing installation, sign with the same identity and keep the bundle identifier unchanged, so the app keeps its data.

### Windows

1. Download [NativePorts-Windows-x64.zip](https://github.com/DevZer0D4Y/NativePorts/releases/download/v0.3.0/NativePorts-Windows-x64.zip) from the [latest release](https://github.com/DevZer0D4Y/NativePorts/releases/latest) and extract it. Keep each `.exe` together with every DLL in its folder; THPS2 also needs `libc++.dll` and `libunwind.dll`.
2. Run the game's `.exe`, then click **OPEN ROM ARCHIVE** or **OPEN DISC IMAGE** (or press **O**), or drag your game file into the window.
3. For the arcade games, press **5** for a credit, then **1** to start. For the console games, press **1** (Start).

If Windows shows "Windows protected your PC", click **More info › Run anyway**. The executables are not code-signed.

## Playing and settings

On iOS, tap **❚❚** to pause, or hold the controller's Menu button. The pause menu has the same tabs as NBA Hangtime: **GAME**, **DISPLAY** (WIDESCREEN, CRT filter, COLOR SIDES, screen mode, pixels), **TOUCH** (layout, size, opacity, vibration, **CUSTOMIZE LAYOUT**) and **GAMEPAD** (remap every action).

On Windows, the default keys are the arrow keys, **X C Z A S D** for the six actions, **5** for arcade coins and **1** for Start. Slam n Jam and NBA Showtime use **Tab** for PS1 Select and have no coin input. Every key can be remapped in **CONTROLS**.

| Key | Action |
|---|---|
| **Esc** or **P** | Pause menu |
| **F1** | Controls |
| **F8** | Widescreen |
| **F9** | COLOR SIDES |
| **F10** | Screen mode |
| **F11** or **Alt+Enter** | Fullscreen |
| **O** | Open a game file |

## Widescreen

WIDESCREEN (iOS: **DISPLAY** tab; Windows: pause menu or **F8**) is on by default. Like NBA Hangtime's, it shows more of the game rather than stretching it. Each game needed its own approach:

| Game | How it works |
|---|---|
| Super Street Fighter II Turbo | The output stays at 512 columns throughout gameplay. Finite stage scenery is clipped inside that canvas, with colored margins at unsupported edges; fighters and HUD no longer change scale as the camera moves. Menus are centered in a 512-column frame with colored sides. |
| Ultimate Mortal Kombat 3 | During fights the 112 hidden columns of the arcade's 512-pixel rows are shown. The game's display window is widened so the scenery just off screen is drawn, and the floor and repeating backgrounds are extended to the new edges. Solid sky/gradient clears also cover the extra columns, fixing the rectangular gap seen in Scorpion's Lair. |
| Slam 'n Jam '96 | Gameplay uses a 426-column native drawing canvas instead of the original 320 columns. The SLES-00076 software crowd/court clipping checks are widened too, allowing previously rejected geometry to reach the canvas while players, text and the centered HUD retain their original scale. Movies stay 4:3. |
| Tony Hawk's Pro Skater 2 | Flycast's widescreen patch for the game, shown at 16:9. |
| NBA Showtime | Native 368-column gameplay expands to 496 columns, approximately 16:9, with unchanged pixel proportions and a centered HUD. The 8-pixel alignment preserves texture/dither phase. Picture menus retain 4:3 with colored sides. |

In SSF2T and UMK3 the fighters stay within the original screen edges, so the fights play exactly as before, and the HUD stays where it was. Menus keep their 4:3 picture, with COLOR SIDES filling the sides. Turn WIDESCREEN off for the original picture.

## Status

| Game | Version | Platforms |
|---|---|---|
| Super Street Fighter II Turbo | 0.3.0 | iOS, Windows |
| Ultimate Mortal Kombat 3 | 0.3.0 | iOS, Windows |
| Slam 'n Jam '96 | 0.3.0 | iOS, Windows |
| Tony Hawk's Pro Skater 2 | 0.3.0 | iOS, Windows |
| NBA Showtime (PlayStation) | 0.3.0 | iOS, Windows |

Known limitations:

- NBA Showtime: gameplay, HUD, live widescreen toggle and four-slot input routing have been checked locally; hardware multiplayer and sustained performance on iPhone/Windows still need device testing. CRT starts on, can be toggled in the pause menu, and has a direct top-right iPhone button.

- THPS2 on iOS runs the Dreamcast CPU through an interpreter, since iOS doesn't allow a JIT, so it may not reach full speed on every device. Windows uses the x86-64 JIT.
- Slam 'n Jam '96: in some camera angles a corner of the widescreen picture stays dark, where the game itself draws nothing.

Version 0.3.0 was checked with local game captures: SSF2T menus and both camera limits, Slam court pans, Scorpion's Lair in UMK3, THPS2 Hangar gameplay, and NBA Showtime gameplay. THPS2's score, timer, meters and stance overlays now use the actual UI depth band and stay centered at 4:3. Its normal follow-camera offset is increased by 25% through a signature-checked boot patch, verified on the USA disc. Other executables are left untouched if their code signature differs. Full stage coverage and physical iPhone/Windows play testing remain necessary.

## Building

Requires a Mac with **Xcode 16 or newer**, **Python 3** and **Make**. The release builds used Xcode 27.0. For the Windows and desktop builds you also need SDL2 (`brew install sdl2` for the macOS build).

The first build downloads the pinned core sources into the ignored `vendor/` folder and applies the patches in [`patches/`](patches/). You can also run that step by itself:

```sh
python3 scripts/fetch_cores.py
```

| Core | Upstream commit | Patch |
|---|---|---|
| MAME 2003-Plus | [`e05caba`](https://github.com/libretro/mame2003-plus-libretro/tree/e05caba0aa33cae580d104c45c6caa6406617e3b) | [`mame2003-plus.patch`](patches/mame2003-plus.patch): SSF2T and UMK3 split-core drivers, static linking on Apple platforms, NVRAM saving, SSF2T and UMK3 widescreen, the UMK3 palette fix |
| Flycast (libretro) | [`497de25`](https://github.com/libretro/flycast/tree/497de2593bc8bef1a31a3105a0a57f6a059b6ca7) | [`flycast.patch`](patches/flycast.patch): iOS build without JIT or memory-fault handling, texture cache, GLES and timing fixes |
| PCSX ReARMed | [`c881679`](https://github.com/libretro/pcsx_rearmed/tree/c8816799b50388e61cfe237fe2cdbb7d8175f20a) | [`pcsx.patch`](patches/pcsx.patch): widescreen in the GPU renderer |

Builds use two parallel jobs. Game names: `ssf2t`, `umk3`, `slamjam96`, `thps2`, `nbashowt`.

### iOS

```sh
python3 scripts/build_ios.py ssf2t     # builds the core, then the app, unsigned
python3 scripts/package_ipas.py ssf2t  # writes dist/ios/SSF2T-0.3.0-unsigned.ipa
```

Leave out the game name in `package_ipas.py` to package all five. Packaging strips the signature and refuses to include game files, signing files or private build paths. To sign during the build instead, pass `--team YOURTEAMID` to `build_ios.py`, or open `ports/<game>/ios/Hangtime.xcodeproj` in Xcode and choose your team in Signing & Capabilities (build the core first with `python3 scripts/build_core.py <game> iphoneos`).

### Windows

Cross-compiled on the Mac with [LLVM-MinGW 20260616](https://github.com/mstorsjo/llvm-mingw/releases/tag/20260616) (`llvm-mingw-20260616-ucrt-macos-universal`) and [SDL2 2.32.10](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10) (`SDL2-devel-2.32.10-mingw`), both extracted into `vendor/`. Set `PORT_TOOLCHAIN` and `PORT_SDL` to use them from somewhere else.

```sh
python3 scripts/build_desktop.py ssf2t windows   # writes dist/ssf2t/windows/ssf2t.exe and SDL2.dll
python3 scripts/build_desktop.py ssf2t macos     # native macOS build of the same frontend, for testing
python3 scripts/package_windows.py             # package all five previously built games and runtime DLLs
```

### Tests

Build each game's macOS core before running the controller suite (for example `python3 scripts/build_core.py ssf2t macos`). The rendering helper tests use the fetched sources directly:

```sh
python3 tests/run.py                    # every port's desktop input code with virtual controllers
python3 tests/test_audit.py             # ROM importer: broken, incomplete and forged archives
python3 tests/test_basketball_actions.py
python3 tests/run_thps2_vram.py         # THPS2 texture cache, built from the real core source
python3 tests/test_thps2_shaders.py
python3 tests/test_render_regressions.py # captured HUD classification, executed camera patch, gradient clears
python3 tests/test_stage_edges.py
python3 tests/test_slam_canvas.py
```

None of the tests need game files.

## Source layout

| Path | Contents |
|---|---|
| [`ports/<game>/ios/`](ports/) | iOS app (Swift, UIKit): touch controls, gamepads, menus, Metal renderer with the CRT filter, and the C bridge to the core and the ROM importer |
| [`ports/<game>/desktop/`](ports/) | SDL2 frontend for Windows (and macOS, for testing) |
| [`ports/<game>/game.json`](ports/) | The game's name, file, button labels and default mapping |
| [`scripts/`](scripts/) | Core download, core, iOS and desktop builds, IPA packaging, shader generation |
| [`patches/`](patches/) | Changes to the upstream cores |
| [`shared/newpixie.glsl`](shared/newpixie.glsl) | NewPixie CRT shader source; `scripts/generate_shaders.py` turns it into each port's GLSL and Metal versions |
| [`tests/`](tests/) | Input, ROM importer, bridge and THPS2 renderer tests |
| [`licenses/`](licenses/) | Third-party licenses |

## Credits

- [MAME 2003-Plus](https://github.com/libretro/mame2003-plus-libretro) and the MAME team, whose CPS2 and Midway Wolf-unit drivers run SSF2T and UMK3.
- [Flycast](https://github.com/libretro/flycast) (libretro), for the Dreamcast.
- [PCSX ReARMed](https://github.com/libretro/pcsx_rearmed), for the PlayStation.
- The NewPixie CRT shader by Mattias Gustavsson, from its Slang version by hunterk ([MIT](licenses/NewPixie-MIT.txt)).
- [SDL](https://www.libsdl.org) and [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw), for the Windows builds.

## License

The ports' own code (the iOS apps, the desktop frontends, the bridges and the scripts) is licensed under the [MIT License](LICENSE). The cores, and the changes to them in [`patches/`](patches/), keep their own licenses:

- MAME 2003-Plus: [MAME 2003-Plus license](licenses/MAME-2003-Plus-LICENSE.md), which forbids commercial use. The SSF2T and UMK3 builds include it, so they can't be sold.
- Flycast: [GNU GPL v2](licenses/flycast-LICENSE.txt). The THPS2 builds include it, so they're distributed under the GPL with this source.
- PCSX ReARMed: [GNU GPL v2](licenses/pcsx-rearmed-COPYING.txt). The same applies to the Slam 'n Jam '96 and NBA Showtime builds.

This is an unofficial fan project, not affiliated with or endorsed by Capcom, Midway, Warner Bros. Games, Acclaim, Konami, Activision, Neversoft, Sony, Sega or the NBA. The games and their content belong to their respective owners. No ROMs, disc images, BIOS files, signing certificates or provisioning profiles are distributed with this source.

## Support and community

[Ko-Fi](https://ko-fi.com/dev_zer0) · [Discord](https://discord.gg/uFChheZEWX) · [YouTube](https://www.youtube.com/@develop_erZ)
