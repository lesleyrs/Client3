![freebsd](freebsd.png)
![win2k](win2k.png)
![xbox](xbox.png)
![dreamcast](dreamcast.png)
![vita](vita.png)
![psp](psp.jpg)
![3ds](3ds.png)
![wii](wii.png)
![postmarketos](postmarketos.png)

## references
* https://github.com/2003scape/rscsundae - rsa encryption code (C RSC server)
* https://github.com/2003scape/rsc-c - libraries, networking, platform code (C RSC client)
* https://github.com/2004Scape/Client - renamed java deob that this port is based on
* https://github.com/LostCityRS/Client-TS - typescript port that encountered some of the same issues
* https://github.com/RuneWiki/rs-deob - unmodified java deobs

### old
* https://github.com/Pazaz/RS2-225 - renamed java deob with builtin server
* https://github.com/2003scape/rsc-client - bundled webworker/webrtc server idea in Client2 (old TS port)
* https://github.com/2004Scape/Client2 - https://lesleyrs.github.io/Client2/?world=999&detail=high&method=0
* https://github.com/galsjel/RuneScape-317 - partial 317 TS port https://github.com/lesleyrs/webclient317

## game history info
* https://github.com/2004Scape/Server/wiki/FAQ
* https://runescape.wiki/w/Build_number
* https://oldschool.runescape.wiki/w/Graphical_updates_(historical)
* https://oldschool.runescape.wiki/w/User:Hlwys

## TODO
```
- free dynamic models in world3d_draw_tile or world3d_set_ funcs?, and use original dynamic cache size + maybe create dynamicmodel struct for saving memory with func ptrs
- (non-wasm): server cache changes requires manual cache+archive_checksums update in the client since it doesn't download, also the server has an issue with client map crcs changing when only server maps get updated.
- midi fading, old js code for IE: https://github.com/2004Scape/Server/blob/61bf21fb3755c14b5cf6d47c9d974dee5783beda/view/javaclient.ejs new ts code: https://github.com/2004Scape/Client2/commit/92e74f1f134ea82e48dd608dcca3422777a7a986 https://github.com/LostCityRS/Client-TS/pulls?q=is%3Apr+is%3Aclosed+midi
- fix remaining touch screen platforms red clicks by copying the 3ds code (_Model.mouse_x/y are only updated in draw_scene after update())
- changing bzip_decompress + stbi_load_from_memory for other libs would allow for O3 optimization on consoles, maybe port jagex bzip2 too
- tinysoundfont seems to break on powerpc cpus (wii, wiiu) and libtom specfically on wiiu only?
- mapview from java client (change preload-file in makefile to cache/mapview for sdl2 emscripten)
- optional QOL changes from the java client teavm branch
- icon/metadata/title etc for the different platforms: title+taskbar+desktop (see rsc-c for examples)
- add CI: run make check/scan + artifacts
- clean up keycodes (from rsc-c, EG non-emscripten single/double quotes + fkey keycodes are defined for emscripten only)
- func args might partially differ in order to the Client repo due to being based off rs2-225: animbase, animframe, pix2d, pix3d, gameshell, jagfile, model, packet, pix8, pixfont, pixmap, redo them? inconsistent naming: used both world3d and scene for world3d, rename world3d to scene or at least for args?
- global search TODO, NOTE, and all platform defines, change many funcs+prototypes to static, look for missing/dupe client struct members and client funcs with different casing, finish debug command or remove it.
- playground models leak memory for some reason
- fix component "indirect" leaks model/pix24: they get modified in packets so the global component doesn't own that memory anymore. Most leak messages aren't a leak though, since it has to live for the duration of the program.
- cache/ and SCC1_Florestan.sf2 are copied into rom/ due to it being the root for most consoles, to clean up we have to chdir in sdl*.c only when just desktop makes use of it.
```

## Java and C differences + codestyle
```
- the 225 java client has client bugs which are all present in the C client: disappearing locs (doors/gates), stuck sequences (anims), etc
- errorhost, errorstarted code never runs
- java has 16 bit char, undefined keychar is 0xffff, pound char doesn't fit in ascii
- some keycodes differ from java virtual keycodes
- window insets removed outside of web, (gameframe/viewbox).java replaced with platform dir
- added pix error checking in client_load
- java static class members are added to a separate global struct
- java private static class members are now C global static vars
- for stack allocated arrays/strings we have to check if idx[0] != '\0' or idx[0] for short
- original unused vars/params casted to (void) to avoid spam with warnings on
- some input (EG client_update_title username/password) is changed to fit C better
- some buffer sizes used are just arbitrary due there not being a strict limit
- try catch turned into if (!var) break in load() or goto is used for login error message
- on windows we aren't loading system gm.dls but use a similar sf2 soundfont instead
- the game uses 3 titles: "RuneScape - the massive online adventure game by Jagex Ltd" (website), "RuneScape Game" (html) and "Jagex" (jar)
- init() moved to main() as that's emscriptens entrypoint
- dnslookup on web just shows your public ip instead of dns, this is expected and the same applies to the typescript client. If dnslookup fails to resolve and welcome screen lags you can set `hide_dns = 1` in config.ini to skip it.
- no client_load_archive message for file streaming since the files are fetched all at once
- http requests with openurl for checksums/cache/midis and cacheload + cachesave only have an emscripten impl as they aren't supposed to change and saving files on consoles depends on if sdcard or romfs was used) another downside is that being "connected" in emulators generally stops you from being able to fast forward so load times will be slow. Other signlink functions like getuid, reporterror, findcachedir and saving audio files for the browser to play are no longer that useful.

- networking/midi/login flames run on the same thread
- synchronized is unused and there's no run() function in client.c
- can't create threads on web without sharedarraybuffer if we wanted to: https://emscripten.org/docs/porting/pthreads.html
- c11 threads aren't supported with sanitizers, valgrind tools, tcc and macos
- no official pthreads on windows, the thirdparty one dropped old windows support?

- int is assumed to be 32 bits, the few uses of shorts are ignored
- use int8_t for java byte arrays, char isn't guaranteed to be signed
- use in64_t for longs as long is 32 bits on windows
- few uses of size_t and other uints for interacting with libs/unsigned bitshift
- *.length checks replaced with *_count vars (EG label_vertices) or *_LENGTH define if it doesn't change
- replaced labelled break/continue with goto
- removed explicit null checks with with var and !var
- prefixed function names with the struct they take as first arg
- signed integer underflow/overflow is undefined (-fwrapv doesn't exist for cl/tcc, but there's no known issue) loopcycle should be ok

- msvc doesn't support VLAs (variable length arrays), use heap allocations
- all the casts to char* and int in clientstream are just to stop windows warnings
- changed camelCase into snake_case, lowercase hex values
- no code width limit as virtual text wrapping is superior for selecting complete lines
- only moved files to dirs with little includes to avoid complicating builds with -I
- outside of SDL3 or TCC the SDL header is exposed on some platforms ONLY to have SDL_main replace entrypoint for subsystem:windows/android etc, don't use it
```
