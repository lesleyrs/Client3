# RuneScape 2 revision #225 (18 May 2004) C99 port
Portable single-threaded C client for early RS2, the last update before a new cache format and ondemand protocol.

Compatible with [2004Scape](https://github.com/2004Scape/Server), the most accurate runescape remake

Features:
- should work on any 32 bit system with 64 MB of RAM on lowmem, networking and a (read-only) filesystem.
- webassembly build to avoid javascript code being optimized out by the browser.
- WIP ports for most game consoles from 1998 until 2013! Ones with incomplete input are set up to auto-connect.
- optional [config.ini](config.ini.example) file to change client behaviour. Create an empty config.ini to avoid passing cli args.

See [docs](/docs) for more info and media.

## known issues
```
server cache changes would require manual cache+checksums update in client for now, the server has an issue with client map crcs being changed when server maps get updated (also the cache has some interface changes rn for quest tab and another one), remove original cache at bin/archives when the cache matches exactly

no midi fading, old js code for IE: https://github.com/2004Scape/Server/blob/61bf21fb3755c14b5cf6d47c9d974dee5783beda/view/javaclient.ejs new ts code: https://github.com/2004Scape/Client2/commit/92e74f1f134ea82e48dd608dcca3422777a7a986 (client-ts has more some fade fixes)

locs like fires have no animations as pushLocs is disabled for now, it constantly allocates memory due to always calling model_copy_faces in loctype which requires a different approach. The leaks get worse if the dynamic model cache can't fit all sequences (animations) of the models in an area, disable the allocator to see origins.

wordfilter isn't ported yet, so you will see your own swear words but others don't as it gets filtered by the server still.

some bits from signlink missing: uid, reporterror, findcachedir, openurl, opensocket, cacheload, cachesave, etc

remove the refcounting from model/pix24/lrucache for components and do smth else (kept to avoid leak spam rn) as components get assigned models from packets which are put into lrucaches, so global component doesn't own the memory anymore

there are a few more memleaks to work out, also make sure playground doesn't leak anymore after attempting to fix this. Examples: inputtracking (when flagged which happens on report now lol), model_calculate_normals (on interfaces too like newcomer map)

cleanup:
global search TODO, NOTE, and all console defines, check clientstream and keycodes for accuracy (from rsc-c), look for missing/dupe with different casing client struct members and client funcs
change a bunch of functions and function prototypes to static
func args might partially differ in order to the Client repo due to being based off rs2-225: animbase, animframe, pix2d, pix3d, gameshell, jagfile, model, packet, pix8, pixfont, pixmap, redo them?
inconsistent naming: used both world3d and scene for world3d, rename world3d to scene or at least for args? COLLISIONMAP_LEVELS could be added more?
```

## quickstart for windows
All you need to build for 32 bit windows is included:
* tinycc (C compiler, built with `TCC_C=..\tcc.c` env var and some removed libs for smaller distribution)
* libeay32.dll for faster rsa https://gnuwin32.sourceforge.net/packages/openssl.htm
* all 32 bit SDL dlls, only SDL1 works prior to windows XP and is always 32 bit unlike the others

To build simply run `build.bat` in cmd to get the client.exe, it depends on the cache + SDL.dll, libeay32.dll, and optionally .sf2 soundfont/config.ini

`build.bat -h` shows options, EG `-v 1|2|3` sets SDL version and `-c tcc|gcc|emcc` for system tcc, gcc, emcc

Both tcc and old mingw-gcc can target windows 9x with SDL 1. This (unofficial) release doesn't require msys install: https://github.com/fsb4000/gcc-for-Windows98/releases. mingw-gcc 11 optimizations seem to only be slightly faster than tcc though.

type `::perf` command ingame to see fps and lrucache size

### Troubleshooting:
- If the client fails to start it most likely means you are using a SDL dll for the wrong architecture. Delete them and it should copy during next build
- If the server changes cache it has to be manually updated in cache/ directory for now (still happens if new mapdata is found). Crc is disabled for maps and archives for time being.

### Changing between worlds (aka nodeid) using config.ini:
1. You first have to put a `#` in front of all lines that contain the fields below to ignore them
```
# nodeid = 1
# portoff = 0
# server = localhost
# http_port = 80
```
2. Then remove the `#` from the grouped fields of the world you want to access, some worlds don't need all fields and use the defaults.

## Java client
The 2004 jar is stored for comparisons, run with EG: `java -cp bin/runescape.jar client 10 0 highmem members` but:
- there is no audio, it saves audio files for the browser to play which is no longer applicable
- right clicking breaks past java 8
- window insets on modern systems are causing the sides of the game to be cut off slightly
- outside of windows it saves the cache to `/tmp` so every reboot you may have to redownload it
- it only connects to localhost if it's not running as applet
- server http port needs to be set to 80 (2004scape on linux defaults to 8888 right now to avoid sudo)
- TODO confirm: to connect to local java servers on WSL from Windows you might need to add `-Djava.net.preferIPv6Addresses=true` when running client

## Platforms and Compilers
To move the executable you have to take the `cache/` and optionally `config.ini`, `SCC1_Florestan.sf2` and `Roboto/` along with it. The consoles will load it from sdcard if they don't embed the files already.

```
TODO: macos, bsds
TODO: add default helvetica-like system ttf font in gameshell_draw_string when available to avoid Roboto dependency
TODO: all home consoles (wii, dreamcast, xbox) should be able to run the game at higher res or even full res on PAL TVs so you don't have to pan and simplifies set_pixels, but this isn't set up right now and emulators don't seem to support many video modes.
TODO: copy original bzip from java maybe allows for O3 optimization on more consoles unlike current bzip?
TODO: icon/metadata/title etc for the different platforms: title+taskbar+desktop
TODO: add CI: run make check/scan + artifacts
```

### Windows 95 to Windows 11
build.bat(32 bit): tcc (included), mingw-gcc, emcc

run.ps1: cl, clang, tcc, mingw-gcc, emcc

You might want the updated [PowerShell](https://github.com/PowerShell/PowerShell) for run.ps1

```
TODO: confirm win9x work still (with old openssl or mingw-gcc with libtom), maybe add screenshot to /docs
TODO: do we assume windows 95 has -lws2_32, otherwise re-add -lwsock32 and remove -DMODERN_POSIX in batch file
TODO: make win9x compatible batch file (no delayed expansion?) right now needs to build from more modern system
TODO: clean up ps1 script so it doesn't need to be modified
```

### Linux GNU or musl
Makefile: gcc, clang, tcc, mingw-gcc, emcc

If tcc from your package manager isn't working you should build latest [tcc](https://github.com/TinyCC/tinycc) from source

### Web (Emscripten)
install [emsdk](#tools)
run `emmake make`/`make CC=emcc` or `build.bat -c emcc` for windows

emrun causes extra batch job message on windows sigint, can swap it for `py -m http.server` or so to avoid it

enable cors in server web.ts with `res.setHeader('Access-Control-Allow-Origin', '*');` or integrate it like the java/js clients

SDL ports aren't used by default to avoid lag issues on Firefox, reduce output size, and prevent browser shortcuts being disabled. It's fixable by switching between requestAnimationFrame and setTimeout based on if the tab is focused, but using emscripten directly requires no client changes.

If 4 args are passed in shell.html the ip + port will be from the URL instead of config

If not passing args make sure to set http_port to 8888 on linux (or whatever it's configured as in server).

```
TODO: avoiding SDL might also fix firefox memleaks + compare each platforms wasm/js output sizes again after finishing it
NOTE: Linux wasm/js output may be smaller than on Windows
TODO: emscripten wasm on firefox has memleaks related to midi, gets cleaned up by pressing GC in about:memory but why does this happen? Chromium based browsers are ok. Happens on both SDL2 and SDL3.
TODO: ability to set secured websocket at runtime instead of compile
TODO: maybe use emscriptens indexeddb api to store data files and emscripten_wget download cache/checksums instead of preload-file
TODO: fix speedup when tab becomes active
TODO: link webclient in about when it's hosted somewhere
TODO: maybe try adding web worker server compat again: https://emscripten.org/docs/api_reference/wasm_workers.html
TODO: fullscreen option
TODO: mobile controls + osk + mice

NOTE: bring back worldlist loading in [shell.html](https://github.com/lesleyrs/Client3/commit/5da924b9f766005e82163d899e52a5df2f771584#diff-c878553ed816480a5e85ff602ff3c5d38788ca1d21095cd8f8ebc36a4dbc07ee) if it gets re-added for live servers
```

### Android
https://github.com/libsdl-org/SDL/blob/SDL2/docs/README-android.md - from "For more complex projects"

1. `mkdir ~/android && cd ~/android` and download [android command line tools](#tools) to it + accept licenses whenever it asks
2. enable developer options by tapping build number and connect `$HOME/android/platform-tools/adb connect IP:PORT`
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

```
TODO: onscreen keyboard SDL_StartTextInput?
TODO: long press right click? click on touch release? hold in viewport to rotate? share code with postmarketos/vita
```
### Nintendo consoles (devkitPro)
Install [devkitpro](#tools) with (nds/wii/3ds/wiiu/switch)-dev package and run `make -f (nds/wii/3ds/wiiu/switch).mk -j$(nproc) -B`.

The NDS target only works on 3DS with DSI emulation as it should have 32MB RAM there (untested, melonDS doesn't emulate extra ram yet).

Wii U and Switch also need the (wiiu/switch)-sdl2 package.

If you own a console and want to improve a port look at rsc-c for reference: https://github.com/2003scape/rsc-c

#### Wii
in dolphin emulator you can find the sdcard path in `options>configuration>wii>sd card` settings and after moving the files there you have to click `Convert Folder to File Now` to format it.

Controls: wiimote IR pointer works as mouse, A for left click, B for right click, Dpad works as arrow keys, minus for control, plus to pan by moving your wiimote to a side of the screen, 1 to center screen, home button to exit. The nunchuck joystick can also be used as arrow keys.

```
TODO: virtual cursor icon
TODO: add game offset on real HW?
TODO: support usb keyboard and mouse on wii
TODO: virtual keyboard to type, for now set user and pass in config.ini
TODO: audio
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
NOTE: there's weird issues: highmem seems to not start due to tinysoundfont tsf_load failing, and libtom encryption fails (tiny-bignum is ok)
```

#### Switch
in suyu emulator (yuzu fork) click `file->open suyu folder` for sdmc dir

### Sony PSP
Install [pspdev](#tools) and run `make -f psp.mk -j$(nproc) -B`.

ppsspp emulator loads relative dir as memstick, so the filesystem works automatically. Also you should probably enable printf logging with `settings>tools>developer tools>logging channels>printf` to EG verbose

Controls: move cursor with analog stick, O for left click, X for right click, /\ for control, Dpad as arrow keys, Rtrigger + analog stick to pan, Ltrigger to reset screen position

Works on real hardware but requires at least model 2000 due to the full 32 MB not being accessible, only lowmem fits into memory so we force lowmem in custom.c

```
TODO: Model 1000 has the same CPU just less memory, might be worth trying to make it work in kernel mode for 28MB BUT is it safe to do so?
TODO: Could enable audio in lowmem for 2000+ models if there's enough memory remaining?
TODO: virtual cursor icon
```

### Sony PS Vita
Install [vitasdk](#tools) and run `make -f vita.mk -j$(nproc) -B`. Add `SDL=0` to use vita.c.

can test with Vita3K, and to avoid waiting for the vpk to decompress all files you can just copy them manually

Controls: touch as mouse, X for right click, /\ for control, Dpad as arrow keys

```
TODO: draw game at offset to center it, and offset touch input based on that.
TODO: touch input on the back, confirm touch works correctly
TODO: update sce_sys assets
TODO: allow setting sdl version to 3 in makefile when it works
TODO: check if vita.c native touch fixes minimap offset, and make it playable
```

### Sega Dreamcast
Install [kallistios and mkdcdisc](#tools) and run `make -f dreamcast.mk -j$(nproc) -B`. Necessary files are built into the cdi.

To try on real hardware you'd need the 32 MB ram expansion mod, which seems involved and maybe less compatible with some other games

Flycast seems to be the best emulator, supports ram expansion and networking.

Not fully playable due to limited RAM, you can only load areas that use 2 MB of space in lrucaches (wildy etc)

Controls: joystick = move cursor, Dpad = arrow keys, B = left click, A = right click, Y = control, Ltrig = center screen, Rtrig+joystick = pan screen

```
NOTE: local servers don't work on emulator? only live works
TODO: fopen path was changed due to the mkdcdisc tool adding dots to files without extension https://gitlab.com/simulant/mkdcdisc/-/issues/14
TODO: check if it's making use of vram currently, dreamcast vram is quite big (8mb)
TODO: instead of INIT_DEFAULT choose the flags we want to use, Mouse could be used in flycast too if they hide system cursor. For mouse and keyboard in flycast you have to set the physical device ports to dreamcast device port.
TODO: virtual cursor icon
```

### Microsoft Xbox
Install [nxdk](#tools) and run `make -f xbox.mk -j$(nproc) -B`. Necessary files are built into the iso.

To run with xemu use `-dvd_path client.iso` as args.

Controls: none yet, it just automatically logs in

```
NOTE: local servers don't work on emulator? only live works
NOTE: default.xbe stays around in rom dir when it's junk for other consoles that embed that directory. Can remove it after building.
TODO: fopen had to be separated due to the need for backwards slashes, also there's no chdir equivalent?
TODO: virtual cursor icon
TODO: controls (nxdk devs said they would refactor get_ticks name to resolve conflict)
```

## libraries
* [micro-bunzip](https://landley.net/code/) | https://landley.net/code/bunzip-4.1.c
* [isaac](https://burtleburtle.net/bob/rand/isaacafa.html) | https://burtleburtle.net/bob/c/readable.c
* [TinySoundFont](https://github.com/schellingb/TinySoundFont) - with fix for attack1.mid by skipping RIFF header and android support
* [tiny-bignum-c](https://github.com/kokke/tiny-bignum-c) - prefer libtom/openssl/bigint, but works fine with smaller exponent
* [ini](https://github.com/rxi/ini)
* [stb_image and stb_truetype](https://github.com/nothings/stb)

## optional libraries
* [OpenSSL](https://github.com/openssl/openssl) | https://wiki.openssl.org/index.php/Binaries

32 bit libeay32.dll from: [gnuwin32](https://gnuwin32.sourceforge.net/packages/openssl.htm)

emcc libcrypto.a from: [get-openssl-wasm.sh](get-openssl-wasm.sh)

* [LibTomMath](https://github.com/libtom/libtommath) | https://github.com/libtom/libtommath/releases/latest

libtommath releases include a gen.pl script to generate a single mpi.c file from the whole source. Move the unresolved rand symbols behind define to fix windows building.

* [SDL-1.2](https://github.com/libsdl-org/SDL-1.2) | [SDL-2/SDL-3](https://github.com/libsdl-org/SDL) | https://libsdl.org/release/

Using prebuilt SDL releases but removed tests dir, removed dotfiles from mingw SDL1 and fixes in VC SDL1 for tcc.
Latest SDL1 already contains the tcc fix but they don't make new releases for it.

## tools
* [tcc](https://github.com/TinyCC/tinycc) | https://bellard.org/tcc/
* [emsdk](https://github.com/emscripten-core/emsdk) | https://emscripten.org/docs/getting_started/downloads.html
* [devkitpro](https://github.com/devkitPro) | https://devkitpro.org/
* [pspdev](https://github.com/pspdev/pspdev) | https://pspdev.github.io/
* [vitasdk](https://github.com/vitasdk/vdpm) | https://vitasdk.org/
* [kallistios](https://github.com/KallistiOS/KallistiOS) | [mkdcdisc](https://gitlab.com/simulant/mkdcdisc)
* [nxdk](https://github.com/XboxDev/nxdk)
* [android command line tools](https://developer.android.com/studio)
