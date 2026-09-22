# Dolphin SIGBUS raw evidence

Dispatch configuration: `DOLPHIN_HEADLESS_PLATFORM=headless`, with `QT_QPA_PLATFORM`, `QT_XCB_NO_XI2`, and `QT_WAYLAND_RECONNECT` unset.

Exact result: exit code `135`, terminated by signal `SIGBUS` (7). The launcher emitted `Bus error (core dumped)`.

## launcher.txt

```text
binary=/paperclip/instances/default/projects/7151eec8-70ef-482e-bef7-0734a4b9be1b/32b9a7e6-bb95-4a1b-b983-2489d1c68f82/_default/shared/tools/dolphin/AppDir/bin/dolphin-emu-nogui
826bb0da3824daca97d710e4120074fcbdde82550e98516e4f35c5e653611169  /paperclip/instances/default/projects/7151eec8-70ef-482e-bef7-0734a4b9be1b/32b9a7e6-bb95-4a1b-b983-2489d1c68f82/_default/shared/tools/dolphin/AppDir/bin/dolphin-emu-nogui
version:
WARNING: Cannot find CA Certificates in host!
Dolphin 2606a
help:
WARNING: Cannot find CA Certificates in host!
Usage: dolphin-emu-nogui [options]... [FILE]...

Options:
  --version             show program's version number and exit
  -h, --help            show this help message and exit
  -u USER, --user=USER  User folder path
  -m MOVIE, --movie=MOVIE
                        Play a movie file
  -e <file>, --exec=<file>
                        Load the specified file
  -n <16-character ASCII title ID>, --nand_title=<16-character ASCII title ID>
                        Launch a NAND title
  -C <System>.<Section>.<Key>=<Value>  Set a configuration option
  -s <file>, --save_state=<file>
                        Load the initial save state
  -v VIDEO_BACKEND, --video_backend=VIDEO_BACKEND
                        Specify a video backend
  -a AUDIO_EMULATION, --audio_emulation=AUDIO_EMULATION
                        Choose audio emulation from [%choices]
  -p PLATFORM, --platform=PLATFORM
                        Window platform to use [%choices]
linkage:
	statically linked
```

## stderr.log

```text
WARNING: Cannot find CA Certificates in host!
41:46:934 Core/ConfigManager.cpp:247 N[CORE]: Active title: GALE01
```

## stdout.log

Empty.

## coredumpctl.txt

```text
exit_code=135
../shared/tools/dolphin/dolphin-headless: line 45: coredumpctl: command not found
```

## Marker boundary

`movie_playback_entered`, `CHARACTER_SELECT_COMPLETE`, and `OFFLINE_MATCH_STARTED` were not observed. No markers were synthesized. The crash occurred after Dolphin reported the active title and before any gameplay output.
