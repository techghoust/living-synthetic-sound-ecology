# LSSE INSTALLATION

LSSE contains nine independent VST3 effects; install all of them or only the ones you need

## Windows

copy the `.vst3` bundles to:

```text
C:\Program Files\Common Files\VST3\
```

## macOS

copy the `.vst3` bundles to either:

```text
~/Library/Audio/Plug-Ins/VST3/
```

or:

```text
/Library/Audio/Plug-Ins/VST3/
```

unsigned development builds may be blocked by macOS; public macOS releases will need signing and notarization before they can be treated as normal end-user packages

## Linux

copy the `.vst3` bundles to:

```text
~/.vst3/
```

the Linux build is intended for native Linux VST3 hosts; FL Studio has no official native Linux version, so FL Studio through Wine is not part of the supported test matrix

## FL Studio scan

1. open `Options > Manage plugins`;
2. select `Find installed plugins`;
3. find the LSSE plugin by its individual name;
4. load it into a Mixer effect slot

existing projects should continue to restore because the internal plugin identifiers remain unchanged
