# RuneScape 2 revision #225 (18 May 2004) C99 port
Portable single-threaded C client for early RS2, the last update before a new cache format and ondemand protocol.

Compatible with [2004Scape](https://github.com/2004Scape/Server), the most accurate runescape remake

Features:
- should work on any 32 bit system with 64 MB of RAM on lowmem, networking and a (read-only) filesystem.
- webassembly build to avoid javascript code being optimized out by the browser.
- WIP ports for most game consoles from 1998 until 2013! Ones with incomplete input are set up to auto-connect.
- optional [config.ini](example.ini) file to change client behaviour. Create an empty config.ini to avoid passing cli args.

See [docs](/docs) for more info, media, and TODOs.

## known issues
```
(non-wasm): server cache changes requires manual cache+archive_checksums update in the client since it doesn't download, also the server has an issue with client map crcs changing when only server maps get updated.

no midi fading, old js code for IE: https://github.com/2004Scape/Server/blob/61bf21fb3755c14b5cf6d47c9d974dee5783beda/view/javaclient.ejs new ts code: https://github.com/2004Scape/Client2/commit/92e74f1f134ea82e48dd608dcca3422777a7a986 https://github.com/LostCityRS/Client-TS/pulls?q=is%3Apr+is%3Aclosed+midi

wordfilter isn't ported yet, so you will see your own swear words but others don't as it gets filtered by the server still.

locs like fires have no animations as pushLocs is disabled for now, it constantly allocates memory due to always calling model_copy_faces in loctype which requires a different approach. The leaks get worse if the dynamic model cache can't fit all sequences (animations) of the models in an area, disable the allocator to see origins.
```

## quickstart for windows
All you need to build for 32 bit windows is included:
* tinycc (C compiler, built with `TCC_C=..\tcc.c` env var and removed bcheck lib)
* all 32 bit SDL dlls, only SDL1 works prior to windows XP and is always 32 bit unlike the others

To build simply run `build.bat` in cmd to get the client.exe, optionally set SDL ver `-v 1|2|3` and C compiler `-c tcc|gcc|emcc`.

SDL1 is default for tcc and old mingw-gcc to target windows 9x, but only SDL2/3 have sfx right now. This (unofficial) release doesn't require msys install: https://github.com/fsb4000/gcc-for-Windows98/releases. mingw-gcc 11 optimizations seem to only be slightly faster than tcc though.

If the client fails to start you either aren't passing cli args and don't have a config.ini OR you are using a SDL dll for the wrong architecture. Delete it and it'll be copied during next build

## Platforms and Compilers
To move the executable you have to take the correct `SDL.dll`, `cache/` and optionally `config.ini`, `SCC1_Florestan.sf2` and `Roboto/` along with it. The consoles will load it from sdcard if they don't embed the files already.

type `::perf` command ingame to see fps and lrucache size

all home consoles (wii, dreamcast, xbox) should be able to run the game at higher res or even full res on PAL TVs so you don't have to pan, but this isn't set up and emulators don't support many video modes.

When adding a new platform also add system ttf font closest to helvetica in gameshell_draw_string when available to avoid Roboto dependency.

To be able to run some emulators on WSL2 you may need to prefix `MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460`.

If tcc from your package manager isn't working you should build latest [tcc](#tools) from source

[v86](#tools) is a x86 PC emulator running in the browser, including older windows.

### Windows 95 to Windows 11
build.bat(32 bit): tcc (included), mingw-gcc, emcc

run.ps1: cl, clang, tcc, mingw-gcc, emcc

You might want the updated [PowerShell](#tools) for run.ps1

```
TODO: add wav sfx to complete SDL1 platform for win9x
TODO: make win9x compatible batch file (no delayed expansion?) right now needs to build from more modern system
TODO: clean up ps1 script so it doesn't need to be modified

NOTE: on v86 PC emulator the cursor flickers on win95, and colours on win9x are wrong? win2k is fine
```

### Linux GNU or musl
Makefile: gcc, clang, tcc, mingw-gcc, emcc

### FreeBSD
Install sdl1/sdl2 or sdl3+pkgconf and run `gmake SDL=1/2/3`

### MacOS
TODO

### Web (clang)
Install clang and get [wasmlite](#tools) (you need the libc and generated index.html)
then run `make -f wasm.mk run DEBUG=0` with correct sysroot path.

you must add `?client` to the URL and optionally append `&arg 1&arg 2&arg 3&arg 4`
you can configure ip and port in config.ini
The only needed files are the index.html + client.wasm and optionally the soundfont/config.ini relative to it.
enable cors in server web.ts with `res.setHeader('Access-Control-Allow-Origin', '*');`

```
TODO nuke emscripten full sdl2 targets makefile+readme+fix emscripten defines to apply to just wasm
TODO midi+playwave
TODO js issues: game speedup when tabbed out, wrong fps, reconnect on dc, each refresh increases memory?

TODO add to build.bat/ps1
NOTE: wasm output can be made smaller by using js bigints instead of mpi.c
```

### Web (emscripten)
Install [emsdk](#tools)
run `emmake make`/`make CC=emcc` or `build.bat -c emcc` for windows

For make you can append `run` to start a http server and `DEBUG=0` to optimize. Then go to `ip:port/client.html` (or another entrypoint)

Pass 4 args in shell.html to use the ip + port from URL instead of config, otherwise set http_port to 8888 in config for linux servers.

The only needed files are the index.`html,js,wasm` and optionally the soundfont/config.ini relative to it.

enable cors in server web.ts with `res.setHeader('Access-Control-Allow-Origin', '*');`

```
TODO: JSPI decreases output size a lot, but is locked behind browser flag for now on for firefox
TODO: midi fading + scape_main stutters during load so it's moved to post load + maybe replace SDL2 audio (check tinymidipcm) but it fixes inactive tab speedup
TODO: use emscriptens indexeddb api to store data files (add cacheload and cachesave)
TODO: try adding web worker server compat again: https://emscripten.org/docs/api_reference/wasm_workers.html
TODO: fullscreen option button
TODO: mobile controls: touch + rotate + osk + mice, PWA manifest

NOTE: Windows and Linux output size might differ and sigint on Windows will cause terminate batch job message if using emrun.
NOTE: SDL2/3 audio prevents the tab from speeding up when changing focus even in lowmem, the typescript client uses absolute time for idlecycles.
NOTE: SDL2/3 emscripten ports have setTimeout lag issues, block browser shortcuts in generated js, and memleaks on firefox that get cleaned up by pressing GC in about:memory but why does this happen? (not if only using audio)
NOTE: maybe bring back worldlist loading in [shell.html](https://github.com/lesleyrs/Client3/commit/5da924b9f766005e82163d899e52a5df2f771584#diff-c878553ed816480a5e85ff602ff3c5d38788ca1d21095cd8f8ebc36a4dbc07ee) if it gets re-added for live servers
```

### Android
1. `mkdir ~/android && cd ~/android` and download [android command line tools](#tools) to it + accept licenses whenever it asks
2. enable developer options by tapping build number, then you can pair and connect to the device through wifi:
`$HOME/android/platform-tools/adb pair IP:PORT`
`$HOME/android/platform-tools/adb connect IP:PORT`
3. In Client3/android-project run `ANDROID_HOME="$HOME/android/" ./gradlew installDebug`
4. The APK will be in android-project/app/build/outputs/apk/debug/ and installed on the device

- you can also start it remotely: `$HOME/android/platform-tools/adb shell am start -n org.libsdl.app/.SDLActivity`
- show error/fatal logging with:  `$HOME/android/platform-tools/adb logcat *:E | grep 'org.libsdl.app'`

#### Steps to update/reproduce the current android setup:
1. in Client3/android-project/app/jni run `mkdir SDL`
2. git clone SDL, git checkout SDL2 branch and run `cp -r Android.mk include src SDL`
3. symlink src and rom directories:
```sh
cd android-project/app/jni/src && ln -s ../../../../src src
cd android-project/app/src/main && ln -s ../../../../rom assets
```
5. set Client LOCAL_CFLAGS and LOCAL_SRC_FILES app/jni/src/Android.mk
6. enable networking in app/src/main/AndroidManifest.xml `<uses-permission android:name="android.permission.INTERNET" />`

https://github.com/libsdl-org/SDL/blob/SDL2/docs/README-android.md - from "For more complex projects"

https://github.com/libsdl-org/SDL/blob/SDL2/docs/README-touch.md
```
TODO: long press right click? click on touch release? hold in viewport to rotate? share code with postmarketos/vita
```
### Nintendo consoles (devkitPro)
Install [devkitpro](#tools) with (nds/wii/3ds/wiiu/switch)-dev package and run `make -f (nds/wii/3ds/wiiu/switch).mk -j$(nproc) -B`.

The NDS target only works on 3DS with DSI emulation as it should have 32MB RAM there (untested, melonDS doesn't emulate extra ram yet).

Wii U and Switch also need the (wiiu/switch)-sdl2 package.

#### Wii
in dolphin emulator you can find the sdcard path in `options>configuration>wii>sd card` settings and after moving the files there you have to click `Convert Folder to File Now` to format it.

Controls: wiimote IR pointer works as mouse, A for left click, B for right click, Dpad works as arrow keys, minus for control, plus to pan by moving your wiimote to a side of the screen, 1 to center screen, home button to exit. The nunchuck joystick can also be used as arrow keys.

```
TODO: support usb keyboard (dolphin doesn't emulate it yet)
TODO: add game offset expected for real hardware?
```

#### 3DS
in citra emulator click `file>open citra folder` for sdmc dir https://citra-emulator.com/wiki/user-directory/

Controls: Touch to left click, L + touch to right click, Dpad for arrow keys, X for control

```
TODO: undo touch changes in 3ds.c depending how it works on real hardware (vita seems to work fine on hw)
TODO: The "New" 3ds/2ds line for higher cpu clock rate does not seem to make much difference in citra?
TODO: add panning for both 3ds and nds, also allow toggling what's on top screen (maybe a separately panned view)
```

#### Wii U
in cemu emulator click `file>open mlc folder`, go 1 directory up to see sdcard dir

```
TODO: Touch input not working yet, might be fixed by last wiiu-sdl2 commit.
NOTE: libtom encryption fails when it works on old wii? (tiny-bignum is ok)
NOTE: highmem doesn't start due to tinysoundfont not working on powerpc
```

#### Switch
in suyu emulator (yuzu fork) click `file->open suyu folder` for sdmc dir

### Sony PSP
Install [pspdev](#tools) and run `make -f psp.mk -j$(nproc) -B`.

ppsspp emulator loads relative dir as memstick, so the filesystem works automatically. Also you should probably enable printf logging with `settings>tools>developer tools>logging channels>printf` to EG verbose

Controls: move cursor with analog stick, O for left click, X for right click, /\ for control, Dpad as arrow keys, Rtrigger + analog stick to pan, Ltrigger to reset screen position

Works on real hardware but requires at least model 2000 due to only 24MB (28MB with kernel mode not sure if safe to use?) being accessible on model 1000, only lowmem fits in memory so we force lowmem in custom.c

```
NOTE: Could add sfx and/or midi in lowmem, since it's the most important highmem feature
```

### Sony PS Vita
Install [vitasdk](#tools) and run `make -f vita.mk -j$(nproc) -B`. Add `SDL=0` to use vita.c.

can test with Vita3K, and to avoid waiting for the vpk to decompress all files you can just copy them manually

Controls: touch as mouse, X for right click, /\ for control, Dpad as arrow keys

```
TODO: check if vita.c native touch fixes minimap offset, check if osk works on real hw
TODO: find minimap touch offset issue, center game+offset touch input after.
TODO: vitagl
TODO: use touch input on the back?
TODO: allow setting sdl version to 3 in makefile when sdl2 is complete
TODO: update sce_sys assets
```

### Sega Dreamcast
Install [kallistios and mkdcdisc](#tools) and run `make -f dreamcast.mk -j$(nproc) -B`. Necessary files are built into the cdi.

To try on real hardware you'd need networking support and the 32 MB ram expansion mod, which seems involved and maybe less compatible with some other games. Flycast seems to be the best emulator and supports both.

See defines.h for inauthentic changes to get below 32MB RAM usage. The city of Ardougne isn't accessible as it uses up to 12MB ram in allocator.

Controls: joystick = move cursor, Dpad = arrow keys, B = left click, A = right click, Y = control, Ltrig = center screen, Rtrig+joystick = pan screen

There's currently no way to type. But it's not required to play the game and you can set your login details in rom/config.ini

```
TODO: try to make use of dreamcast vram or aram
TODO: support mouse/keyboard for dreamcast. For mouse and keyboard in flycast you have to set the physical device ports to dreamcast device port, but mouse is not very useful in emu until they hide the system cursor.

NOTE: if the cdi doesn't load you might have to remove --no-padding in Makefile? untested on hardware
NOTE: local servers don't work on emulator? only remote servers work
NOTE: fopen path was changed due to the mkdcdisc tool adding dots to files without extension https://gitlab.com/simulant/mkdcdisc/-/issues/14
```

### Microsoft Xbox
Install [nxdk](#tools) and run `make -f xbox.mk -j$(nproc) -B`. Necessary files are built into the iso.

To run with xemu use `-dvd_path client.iso` as args.

Controls: right analog stick to move the mouse, dpad to rotate camera, B = left click, A = right click, Y = control, X = toggle fps, back = logout, start = login, white = center screen pan, black = pan with right analog stick

```
TODO: audio on highmem (for 128mb ram expansion?)

NOTE: local servers don't work on emulator? only remote servers work
NOTE: default.xbe stays around in rom dir when it's junk for other consoles that embed that directory. Can remove it after building.
NOTE: fopen had to be separated due to the need for backwards slashes, also there's no chdir equivalent?
```

## Java client
The 2004 jar is stored for comparisons, run with EG: `java -cp bin/runescape.jar client 10 0 highmem members` but:
- there is no audio, it saves audio files for the browser to play which is no longer applicable
- right clicking breaks past java 8
- window insets on modern systems are causing the sides of the game to be cut off slightly
- outside of windows it saves the cache to `/tmp` so every reboot you may have to redownload it
- it only connects to localhost if it's not running as applet
- server http port needs to be set to 80 (2004scape on linux defaults to 8888 right now to avoid sudo)
- TODO confirm: to connect to local java servers on WSL from Windows you might need to add `-Djava.net.preferIPv6Addresses=true` when running client

## libraries
* [micro-bunzip](https://landley.net/code/) | https://landley.net/code/bunzip-4.1.c
* [isaac](https://burtleburtle.net/bob/rand/isaacafa.html) | https://burtleburtle.net/bob/c/readable.c
* [TinySoundFont](https://github.com/schellingb/TinySoundFont) - with fix for attack1.mid by skipping RIFF header and android support
* [tiny-bignum-c](https://github.com/kokke/tiny-bignum-c) - prefer libtom/openssl/bigint, but works fine with smaller exponent
* [LibTomMath](https://github.com/libtom/libtommath) | mpi.c is from gen.pl script in [releases](https://github.com/libtom/libtommath/releases/latest). with added ifdefs to fix non-gcc builds.
* [ini](https://github.com/rxi/ini)
* [stb_image and stb_truetype](https://github.com/nothings/stb)

## optional libraries
* [OpenSSL](https://github.com/openssl/openssl) | https://wiki.openssl.org/index.php/Binaries

* [SDL-1.2](https://github.com/libsdl-org/SDL-1.2) | [SDL-2/SDL-3](https://github.com/libsdl-org/SDL) | https://libsdl.org/release/

Using prebuilt SDL but removed tests, SDL1 mingw dotfiles + SDL1 tcc fixes in VC (fixed upstream but no new releases since 2012)

## tools
* [tcc](https://github.com/TinyCC/tinycc) | https://bellard.org/tcc/
* [emsdk](https://github.com/emscripten-core/emsdk) | https://emscripten.org/docs/getting_started/downloads.html
* [devkitpro](https://github.com/devkitPro) | https://devkitpro.org/
* [pspdev](https://github.com/pspdev/pspdev) | https://pspdev.github.io/
* [vitasdk](https://github.com/vitasdk/vdpm) | https://vitasdk.org/
* [kallistios](https://github.com/KallistiOS/KallistiOS) | [mkdcdisc](https://gitlab.com/simulant/mkdcdisc)
* [nxdk](https://github.com/XboxDev/nxdk)
* [android command line tools](https://developer.android.com/studio)
* [powershell](https://github.com/PowerShell/PowerShell)
* [v86](https://github.com/copy/v86.git) | https://copy.sh/v86/
* [wasmlite](https://github.com/lesleyrs/wasmlite)
