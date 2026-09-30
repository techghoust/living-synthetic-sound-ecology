# LIVING SYNTHETIC SOUND ECOLOGY (LSSE)

## SOUND DESIGN EFFECTS FOR FL STUDIO

a VST3 sound-design plugin suite for FL Studio focused on synthetic, procedural, and evolving sound

current version: `1.1.0`

---

## THE IDEA

LSSE is not one giant effect

it is a family of nine separate sound-design plugins; each plugin has its own character, parameters, state and DSP, but they share one visual language and one build system

the suite is independent; it does not connect to Sound Storyboard and it does not require another LSSE application to run

---

## THE PLUGINS

### MEMORY

captures recent audio and recalls fragments of it as an unstable memory layer

### TEXTURE

reshapes surface detail with capture, grains, spectral wear, and deterministic movement

### MACHINE

turns audio into rhythmic mechanisms, pulses, and controlled mechanical repetition

### MATERIAL

models resonant physical qualities such as hardness, size, damping, and inharmonicity

### IMPACT

builds transient, body, and tail behaviour around incoming attacks

### CREATURE

adds gesture, motion, and organic reactions that feel less static than a conventional effect

### MOTION

moves sound through repeatable paths, space, and modulation

### ENVIRONMENT

creates evolving beds and environmental layers around the source

### CONVOLUTION

uses two impulse references, editing, and morphing to create spaces that can change over time

---

## PLATFORMS

- Windows 10/11 — x64 VST3 for FL Studio;
- macOS 11 or newer — universal VST3 for Apple Silicon and Intel, intended for FL Studio;
- Linux — native x86_64 VST3 for compatible Linux hosts

FL Studio itself is officially available for Windows and macOS; the Linux build is a native VST3 build for Linux hosts. an FL Studio + Wine workflow is not a supported or tested target here

---

## INSTALL

release packages are not published yet; build them from source using the instructions below

copy every `.vst3` bundle you want to use into the VST3 folder for your system:

### Windows

```text
C:\Program Files\Common Files\VST3\
```

### macOS

```text
~/Library/Audio/Plug-Ins/VST3/
```

or for every user:

```text
/Library/Audio/Plug-Ins/VST3/
```

### Linux

```text
~/.vst3/
```

then open the plugin manager in the host and rescan installed plugins

---

## BUILD

requirements:

- CMake 3.22 or newer;
- Git;
- a C++20 compiler;
- Ninja on Linux;
- Xcode on macOS;
- Visual Studio 2022 on Windows

JUCE 8.0.15 is downloaded by CMake during configuration

### Windows

```powershell
cmake --preset vs2022
cmake --build --preset windows-release
ctest --preset windows-release
```

### macOS universal

```bash
cmake --preset macos-universal
cmake --build --preset macos-universal
ctest --preset macos-universal
```

### Linux

install the JUCE development dependencies listed in `.github/workflows/build.yml`, then run:

```bash
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

---

## PACKAGE

after a successful build:

```bash
python tools/package_release.py --build-dir build/windows --platform windows-x64 --configuration Release
```

the script finds all nine VST3 bundles, creates one platform package and writes SHA-256 checksums; use `macos-universal` or `linux-x64` as the platform name for those builds

---

## TESTING

the test suite checks:

- individual DSP engines;
- processor state save and restore;
- parameter identity;
- VST3 discovery and loading;
- editor construction;
- multiple plugin instances;
- changing sample rates and block sizes;
- NaN, infinity, and output ceiling failures

GitHub Actions builds and tests Windows, macOS and Linux separately; it does not publish releases

---

## PROJECT STRUCTURE

```text
plugin/              MEMORY
texture/             TEXTURE
machine/             MACHINE
material/            MATERIAL
impact/              IMPACT
creature/            CREATURE
motion/              MOTION
environment/         ENVIRONMENT
convolution_lab/     CONVOLUTION implementation
shared/              shared UI, state and audio helpers
horizontal_tests/    full-suite host and stress tests
tools/               release packaging helper
```

the internal `convolution_lab` directory and `ConvolutionLab` C++ target are intentionally unchanged; the user-facing plugin name is `CONVOLUTION`

---

## IMPORTANT LIMITATIONS

- Linux binaries build successfully on Ubuntu 24.04 and all 21 automated tests pass;
- Linux host compatibility outside the automated JUCE host tests still needs broader testing;
- macOS binaries are not signed or notarized yet;
- the macOS build still needs final validation in FL Studio;
- VST3 identifiers are kept stable so existing FL Studio projects can continue to restore the plugins

---

## LICENSE

LSSE is free and open-source software licensed under the GNU Affero General Public License v3.0 (`AGPL-3.0-only`)

JUCE 8.0.15 is used under its AGPLv3 option; the VST3 SDK components used by JUCE are licensed separately under the MIT licence. see `LICENSE` and `THIRD_PARTY_NOTICES.md` for details

---

## REPOSITORY

[living-synthetic-sound-ecology](https://github.com/techghoust/living-synthetic-sound-ecology)
