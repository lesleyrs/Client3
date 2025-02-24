# RuneScape 2 revision #225 (18 May 2004) C99 port
Portable single-threaded C client for early RS2, the last update before a new cache format and ondemand protocol.

Compatible with [2004Scape](https://github.com/2004Scape/Server), the most accurate runescape remake

Features:
- should run on any 32 bit system with 64 MB of RAM on lowmem.
- webassembly build to avoid javascript code being optimized out by the browser.
- WIP ports for nintendo consoles that support networking and can simulate clicks (mouse, touch, or wiimote).
- optional config.ini file to change client behaviour, see [config.ini.example](config.ini.example) for options. To avoid passing command line arguments each time you can create an empty config.ini.

See [docs](/docs) for more info and media.

## disclaimer
During limited playtesting the client seems to be stable, but it could crash at any time! Run sanitizer or tcc debug backtrace build to share errors

See [known issues](#known-issues) before reporting an issue.

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

## known issues
```
server cache changes would require manual cache update in client for now, it isn't supposed to change but as of right now there's an issue with client map crcs being changed when server maps get updated (also the cache has some interface changes rn for quest tab and another one) enable crc again after fixes, search "wait authentic cache". Maybe also use emscripten indexeddb api to store data file?

figure out rsaenc bug(s), i'm thinking there are multiple issues (chance of failing login), RSA_BIGINT can still fail due to wrong enc length, but the others fail due to other reasons too... Also fix connecting on desktop to servers with higher than 512 bit rsa, need bigger result array but not for web?

emscripten wasm on firefox has memleaks related to midi, gets cleaned up by pressing GC in about:memory but why does this happen? Chromium based browsers are ok

auto-generated js by emscripten is blocking default browser shortcuts why exactly, also pressing fkeys types uppercase letters even if it doesn't steal input

the most complete platform layer is SDL2, but keyboard input is just an unfinished hack but usable except ctrl doesn't work for running yet.

no midi fading, old js code for IE: https://github.com/2004Scape/Server/blob/61bf21fb3755c14b5cf6d47c9d974dee5783beda/view/javaclient.ejs new ts code: https://github.com/2004Scape/Client2/commit/92e74f1f134ea82e48dd608dcca3422777a7a986 (client-ts has more some fade fixes)

locs like fires have no animations as pushLocs is disabled for now, it constantly allocates memory due to always calling model_copy_faces in loctype which requires a different approach. The leaks get worse if the dynamic model cache can't fit all sequences (animations) of the models in an area, disable the allocator to see origins.

wordfilter isn't ported yet, so you will see your own swear words but others don't as it gets filtered by the server still.

the game uses 3 titles: "RuneScape - the massive online adventure game by Jagex Ltd" (website), "RuneScape Game" (html) and "Jagex" (jar), maybe show the first as it was most commonly seen

some bits from signlink missing (uid, reporterror, findcachedir, openurl, opensocket etc, move map loading to cacheload?

remove the refcounting from model/pix24/lrucache for components and do smth else (kept to avoid leak spam rn) as components get assigned models from packets which are put into lrucaches, so global component doesn't own the memory anymore

there are a few more small memleaks to work out, but they shouldn't become a problem outside of very memory constrained environments.
```

## non issues (expected or unfixable)
Firefox "lag" is due to setTimeout being broken with wasm as using the firefox profiler increases fps. Asyncify is not the problem here I've had it happen in wasm without emscripten, https://github.com/lesleyrs/web-gbc?tab=readme-ov-file#limitations

The "fix" is to use `emscripten_set_main_loop_arg` with 0 fps which calls requestAnimationFrame instead. The function to be called should be the contents of the while loop in gameshell_run (delete the while loop itself) which will get you full FPS. If it still lags close devtools and refresh page. Downsides are that if the tab goes inactive it will speed up when returning, it doesn't seem fixable the same way as in Client2 since they still use setTimeout. Also Firefox won't have the benefit of the websocket never timing out anymore.

emscripten wasm has speedup bug in lowmem if the tab was unfocused due to no audio playing. Could be fixed but would make the code ugly and most people want highmem on web.

dnslookup on web just shows your public ip instead of dns, this is expected and the same applies to Client2. If dnslookup fails to resolve and welcome screen lags you can set `hide_dns = 1` in config.ini to skip it.

SDL3 has high dpi support so window size might be smaller than SDL1/2 https://github.com/libsdl-org/SDL/blob/main/docs/README-highdpi.md

Outside of SDL3 or TCC the SDL header is exposed on windows ONLY to have SDL_main replace entrypoint for windows subsystem, don't use it

On windows we aren't loading system gm.dls but use a similar sf2 soundfont instead

emrun causes extra batch job message on windows sigint, can swap it for `py -m http.server` or so to avoid it

Recompile is needed to change between different RSA key lengths, RSA_BUF_LEN needs to be set at compile time because it's needed for stack allocated arrays and BN_ARRAY_SIZE define.

Mobile input is unfinished (only left clicks on postmarketos/android), as touchscreen only input doesn't feel good to play. Use keyboard and mouse. Missing: right clicking, rotating camera, onscreen keyboard input, TODO: single press uses last mouse pos? Console touchscreens are fine because they have other buttons to use in combination.

instead of a clean target, try: `git clean -fXdn`, remove n to delete files for real

## game history info
* https://github.com/2004Scape/Server/wiki/FAQ
* https://runescape.wiki/w/Build_number
* https://oldschool.runescape.wiki/w/Graphical_updates_(historical)
* https://oldschool.runescape.wiki/w/User:Hlwys

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
### Windows 95 to Windows 11
build.bat(32 bit): tcc (included), mingw-gcc, emcc

run.ps1: cl, clang, tcc, mingw-gcc, emcc

You might want the updated [PowerShell](https://github.com/PowerShell/PowerShell) for run.ps1

NOTE: currently bignum lib isn't working with tcc on windows and gives [invalid memory access](https://lists.nongnu.org/archive/html/tinycc-devel/2024-12/msg00020.html) so we use openssl

TODO:
confirm win9x work still, maybe add screenshot to /docs
do we assume windows 95 has -lws2_32, otherwise re-add -lwsock32 and remove -DMODERN_POSIX in batch file
make win9x compatible batch file (no delayed expansion?) right now needs to build from more modern system
clean up ps1 script so it doesn't need to be modified

### Linux GNU or musl
Makefile: gcc, clang, tcc, mingw-gcc, emcc

If tcc from your package manager isn't working you should build latest [tcc](https://github.com/TinyCC/tinycc) from source

TODO:
```
macos, bsds, android https://wiki.libsdl.org/SDL2/Android
+ add default helvetica-like system ttf font in gameshell_draw_string for each
```

### Web (Emscripten)
install [emsdk](#tools)
run `emmake make`/`make CC=emcc` or `build.bat -c emcc` for windows

Linux wasm/js output seems to be quite a bit smaller than on Windows

If you pass args in the html file the ip address and http port are from the URL itself.

If not passing args make sure to set http_port to 8888 on linux (or whatever it's configured as in server).

NOTE: bring back worldlist loading in [shell.html](https://github.com/lesleyrs/Client3/commit/5da924b9f766005e82163d899e52a5df2f771584#diff-c878553ed816480a5e85ff602ff3c5d38788ca1d21095cd8f8ebc36a4dbc07ee) if it gets re-added for live servers

TODO: maybe take webworker server compat from Client2: https://emscripten.org/docs/api_reference/wasm_workers.html
TODO: possibly target wasm directly with clang instead of emscripten, but then we don't have a libc at all

### Nintendo consoles (devkitPro)
These are unfinished and need testing on real hardware so expect some issues.

TODO: 3ds/wiiu/switch currently logs in automatically due to input issues in emulators?
TODO: RSA is disabled because tiny-bignum is slow (try mbedtls devkitpro package?)

Install [devkitpro](#tools) with (wii/3ds/wiiu/switch)-dev package and run `make -f (wii/3ds/wiiu/switch).mk -j$(nproc) -B`.

Wii U and Switch also need the (wiiu/switch)-sdl2 package.

For all consoles besides NDS you have to move the `cache/` `Roboto/`, `SCC1_Florestan.sf2`, and `config.ini` to sdcard (config needed as we can't pass cli args, but can be empty).

TODO: 3ds/wiiu/switch have an option to build with romfs (read-only memory file system) which won't require an sdcard. Would have to build the config into the binary or load from sd card still.

TODO: on wii use the remaining wiimote buttons for switching views due to the entire game not fitting in 640x480 and same for 3ds at 320x240

TODO: touchscreens systems need a way to right click and rotate camera, use buttons in combination

#### NDS/3DS DSi mode
Not working right now but 32MB ram could work: lowmem + lower bump_allocator_init cap (see ::perf, avoid towns or stay in the Wilderness)

NDS
```
with 32 mb ram gba expansion pak + 4MB system ram
original game will probably run at 1 fps if it works at all
would need major changes to run smoothly, need separate client entrypoint for it
```

3DS DSi mode
```
has the full 32MB ram instead of 16MB on normal DSi (IS-TWL-DEBUGGER consoles have 32MB too)
will probably work right now but the full game will run at a few fps, but the 3ds already has it's own build so it's not as cool.
need to test this on a real 3DS as melonDS and other emulators don't support wifi in DSi mode or the increased 3ds/debugger RAM
```

#### Wii
in dolphin emulator you can find the sdcard path in `options>configuration>wii>sd card` settings and after moving the files there you have to click `Convert Folder to File Now` to format it.

wiimote IR pointer works as mouse, Dpad works as arrow keys, home button to exit.

TODO: lowmem is recommended, highmem only barely fits in wii's memory and crashes quite fast. Might become better if the last small leaks are fixed.

TODO: virtual keyboard to type, for now set user and pass in config.ini, audio, add game offset on real HW?, support usb keyboard and mouse on wii

#### 3DS
in citra emulator click `file>open citra folder` for sdmc dir https://citra-emulator.com/wiki/user-directory/

The game in it's original state might not have a playable frame rate outside of the "New" 3ds/2ds line for higher clock rate.

#### Wii U
in cemu emulator click `file>open mlc folder`, go 1 directory up to see sdcard dir

TODO: Touch input isn't working in cemu, or is their SDL2 port the issue?

TODO: highmem seems to not start due to tinysoundfont tsf_load failing, it works on all other consoles though...

#### Switch
in suyu emulator (yuzu fork) click `file->open suyu folder` for sdmc dir

## libraries
* [micro-bunzip](https://landley.net/code/) | https://landley.net/code/bunzip-4.1.c
* [isaac](https://burtleburtle.net/bob/rand/isaacafa.html) | https://burtleburtle.net/bob/c/readable.c
* [tiny-bignum-c](https://github.com/kokke/tiny-bignum-c)
* [ini](https://github.com/rxi/ini)
* [TinySoundFont](https://github.com/schellingb/TinySoundFont)
* [stb_image and stb_truetype](https://github.com/nothings/stb)

## optional libraries
* [OpenSSL](https://github.com/openssl/openssl) | https://wiki.openssl.org/index.php/Binaries

32 bit libeay32.dll from: [gnuwin32](https://gnuwin32.sourceforge.net/packages/openssl.htm)

emcc libcrypto.a from: [get-openssl-wasm.sh](get-openssl-wasm.sh)

* [SDL-1.2](https://github.com/libsdl-org/SDL-1.2) | [SDL-2/SDL-3](https://github.com/libsdl-org/SDL) | https://libsdl.org/release/

Using prebuilt SDL releases but removed tests dir, removed dotfiles from mingw SDL1 and fixes in VC SDL1 for tcc.
Latest SDL1 already contains the tcc fix but they don't make new releases for it.

## tools
* [tcc](https://github.com/TinyCC/tinycc) | https://bellard.org/tcc/
* [emsdk](https://github.com/emscripten-core/emsdk) | https://emscripten.org/docs/getting_started/downloads.html
* [devkitpro](https://github.com/devkitPro) | https://devkitpro.org/

## references
* https://github.com/2003scape/rsc-c - did a lot of the dirty work in advance (finding libs, networking, input)
* https://github.com/2004Scape/Client - renamed java client deob that the port was based on
* https://github.com/2004Scape/Client2 | https://github.com/LostCityRS/Client-TS - typescript port
* https://github.com/Pazaz/RS2-225
* https://github.com/RuneWiki/rs-deob
