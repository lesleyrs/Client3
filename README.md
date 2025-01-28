# RuneScape 2 revision #225 (18 May 2004) C99 port
Portable single-threaded C client for early RS2, the last update before a new cache format and ondemand protocol.

Compatible with [2004Scape](https://github.com/2004Scape/Server), the most accurate runescape remake

Features:
- should run on any 32 bit system with 64 MB of RAM (preferably higher due to some small memory leaks for now)
- webassembly build to avoid javascript code being optimized out by the browser.
- optional config.ini file to change client behaviour, see [config.ini.example](config.ini.example) for options. To avoid passing command line arguments each time you can create an empty config.ini.

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
outside of web only: water appears to be lowmem always, going up or down ladders causes the scene to not load correctly

fetching worldlist for webclient live server access is temporarily not working, waiting for server update

auto-generated js by emscripten is blocking default browser shortcuts why exactly, also pressing fkeys types uppercase letters even if it doesn't steal input

server cache changes would require manual cache update in client for now, it isn't supposed to change but as of right now there's an issue with client map crcs being changed when server maps get updated (also the cache has some interface changes rn for quest tab and another one) enable crc again after fixes

figure out rsaenc bug(s), i'm thinking there are multiple issues (chance of failing login), RSA_BIGINT can still fail due to wrong enc length, but the others fail due to other reasons too...

the most complete platform layer is SDL2, but keyboard input is just an unfinished hack but usable except ctrl doesn't work for running yet.

locs like fires have no animations as pushLocs is disabled for now, it constantly allocates memory which requires a different approach

the 1 known place the client will crash due to lack of exceptions (in Pix3D) is baxtorian falls

set_pixels is using memcpy to copy surface pixels each time, but this is inefficient and very noticable on weak hardware. But causes crash on login/exit when the surface gets freed (apply this to all SDL if fixed)

no midi fading, causes death sound to cut off or other issue?

wordfilter not ported yet, so you will see your own swear words but others don't as it gets filtered by the server still.

the game uses 3 titles: "RuneScape - the massive online adventure game by Jagex Ltd" (website), "RuneScape Game" (html) and "Jagex" (jar), maybe show the first as it was most commonly seen

login screen flames aren't running on their own timer but use the game loop

some bits from signlink missing (uid, reporterror, findcachedir, openurl, opensocket etc, move map loading to cacheload?

there are a few more small memleaks to work out, but they shouldn't become a problem outside of very memory constrained environments.

check if ttf font is centered correctly (maybe few pixels more to the left?)
```

## non issues (expected or unfixable)
Firefox performance issues might be related to setTimeout not working correctly as using the firefox profiler increases fps. Asyncify is not the problem here I've had it happen in wasm without emscripten, https://github.com/lesleyrs/web-gbc?tab=readme-ov-file#limitations

dnslookup on web just shows your public ip instead of dns, this is expected and the same applies to Client2. If the login welcome screen lags you can set `hide_dns = 1` in config.ini to skip it.

WSL gui has crackling audio sometimes, or not crackling but delayed. Also WSL still allows resizing which freezes window.

SDL3 has high dpi support so window size might be smaller than SDL1/2 https://github.com/libsdl-org/SDL/blob/main/docs/README-highdpi.md

Outside of SDL3 or TCC the SDL header is exposed on windows ONLY to have SDL_main replace entrypoint for windows subsystem, don't use it

On windows we aren't loading system gm.dls but use a similar sf2 soundfont instead

emrun causes extra batch job message on windows sigint, can swap it for `py -m http.server` or so to avoid it

Recompile is needed to change between different RSA key lengths, RSA_BUF_LEN needs to be set at compile time because it's needed for stack allocated arrays and BN_ARRAY_SIZE define.

instead of a clean target, try: `git clean -fXdn`, remove n to delete files for real

## game history info
* https://github.com/2004Scape/Server/wiki/FAQ
* https://runescape.wiki/w/Build_number
* https://oldschool.runescape.wiki/w/Graphical_updates_(historical)
* https://oldschool.runescape.wiki/w/User:Hlwys

## supported compilers per platform
### Windows 95 to Windows 11
build.bat(32 bit): tcc (included), mingw-gcc, emcc

run.ps1: cl, clang, tcc, mingw-gcc, emcc

You might want the updated [PowerShell](https://github.com/PowerShell/PowerShell) for run.ps1

### Linux GNU or musl
Makefile: gcc, clang, tcc, mingw-gcc, emcc

If tcc isn't working you should build latest [tcc](https://github.com/TinyCC/tinycc) from source

## Java client
The 2004 jar is stored for comparisons, run with EG: `java -cp cache/runescape.jar client 10 0 highmem members` but:
- there is no audio, it saves audio files for the browser to play which is no longer applicable
- right clicking breaks past java 8
- outside of windows it saves the cache to `/tmp` so every reboot you may have to redownload it
- it only connects to localhost if it's not running as applet
- server http port needs to be set to 80 (linux defaults to 8888 right now to avoid sudo)
- TODO confirm: to connect to local java servers on WSL from Windows you might need to add `-Djava.net.preferIPv6Addresses=true` when running client

## TODO (add systems to the above list after):
```
check if macos works + add system ttf font in gameshell_draw_string
check if bsds works + add system ttf font in gameshell_draw_string
emscripten: see best initial memory or initial heap so memory growth doesn't kick in and change it in bat/ps1/makefile
32MB ram could work: lowmem + lower bump_allocator_init cap (::perf, avoid towns or stay in the Wilderness)
nintendo consoles using devkitpro
postmarketos mobile right clicking, rotating camera, onscreen keyboard input
possibly target wasm directly with clang, but then we don't have a libc at all
maybe add android build
check windows 95 doesn't require -lwsock32, otherwise add to batch file and remove -DMODERN_POSIX, wii needs it though?
make win98 compatible batch file (no delayed expansion?)
currently bignum lib isn't working with tcc on windows and gives invalid memory access so we use openssl https://lists.nongnu.org/archive/html/tinycc-devel/2024-12/msg00020.html
There might be some signed integer overflow/underflow UB with compilers that don't have a -fwrapv equivalent (cl/tcc). But there's no known issue, not a problem for loop_cycle
for setting ws/wss at runtime if needed: https://github.com/emscripten-core/emscripten/issues/22969 instead of compile time?
native websockets?
SDL3 is not officially released yet, need updated binaries when it is.
http requests for checksums/cache (not done as they aren't supposed to change and some systems don't support saving files)
change a bunch of functions and function prototypes to static
```

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
