# Screen Recorder (Flutter Linux)

Wayland screen recorder with PipeWire capture, FFmpeg encoding, and Linux desktop packaging.

## Features

- Screen recording on Linux/Wayland via PipeWire portal flow.
- Optional audio recording with automatic recommended source detection.
- Resolution presets from native display down to 480p.
- No-upscale behavior for small rectangular capture regions.
- FPS, save path, audio toggle, and resolution settings in-app.
- Linux release packaging with installer, desktop entry, and preflight scripts.
- Manual GitHub workflows for package-only and full release publishing.

## Recommended Audio Stack Setup

For the best audio capture reliability, use a modern PipeWire-based user stack.

- Run `screen-recorder-preflight` after install.
- This configures distro-specific dependencies/services for PipeWire + portals.
- It also helps auto-detect and persist the recommended monitor audio source.

If your distro already uses PipeWire, running preflight is still recommended to verify required services and portal integration.

## Required Dependencies (Manual Local Build)

If you build and run locally (without the packaged installer), install these first:

- Build tooling: `clang`, `cmake`, `ninja`, `pkg-config`
- Native build headers/libs: GTK3, PipeWire 0.3, SPA 0.2
- Runtime encoder: `ffmpeg` (required by recorder process)
- Runtime services: `pipewire`, `wireplumber`, `xdg-desktop-portal` and a portal backend for your desktop

Install commands:

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
  clang cmake ninja-build pkg-config libgtk-3-dev \
  libpipewire-0.3-dev libspa-0.2-dev \
  pipewire pipewire-pulse wireplumber \
  xdg-desktop-portal xdg-desktop-portal-gtk \
  pulseaudio-utils ffmpeg
```

Fedora/RHEL:

```bash
sudo dnf -y install \
  clang cmake ninja-build pkg-config gtk3-devel \
  pipewire-devel pipewire-jack-audio-connection-kit-devel \
  pipewire pipewire-alsa pipewire-pulseaudio wireplumber \
  xdg-desktop-portal xdg-desktop-portal-gtk pulseaudio-utils
sudo dnf -y install ffmpeg || sudo dnf -y install ffmpeg-free
```

Arch/EndeavourOS:

```bash
sudo pacman -S --needed \
  clang cmake ninja pkgconf gtk3 pipewire libpipewire wireplumber \
  xdg-desktop-portal ffmpeg
```

Then run:

```bash
flutter pub get
flutter build linux --release
```

## Local Development

From `flutter_app/`:

```bash
flutter pub get
flutter run -d linux
```

Checks:

```bash
flutter analyze
flutter build linux --debug
```

## Local Release Packaging

Build release and create distributable tarball:

```bash
flutter build linux --release
./packaging/create_release_bundle.sh 1.0.0
```

Output:

- `dist/screen-recorder-1.0.0-linux-x64.tar.gz`
- `dist/screen-recorder-1.0.0-linux-x64.tar.gz.sha256`

The tarball includes:

- `app/` (Flutter Linux bundle)
- `install.sh`
- `bin/screen-recorder-preflight`
- `bin/screen-recorder-detect-audio-device`
- `preflight/setup_debian_ubuntu.sh`
- `preflight/setup_fedora_rhel.sh`
- `preflight/setup_arch_endeavouros.sh`
- `desktop/screen-recorder.desktop`

## Install on a Target Machine

Extract the tarball and run:

```bash
cd screen-recorder-1.0.0
./install.sh
```

What installer does:

- Copies app to `/opt/screen-recorder`
- Adds commands to PATH via `/usr/local/bin/`:
  - `screen-recorder`
  - `screen-recorder-preflight`
  - `screen-recorder-detect-audio-device`
- Copies desktop entry to `~/.local/share/applications/screen-recorder.desktop`
- Runs distro preflight by default

Skip preflight during install:

```bash
RUN_PREFLIGHT=0 ./install.sh
```

Run preflight manually later:

```bash
screen-recorder-preflight
```

## Audio Device Selection

At startup, app resolves recommended audio source in this order:

1. `SCREEN_RECORDER_AUDIO_DEVICE` env var
2. `~/.config/screen-recorder/audio-device`
3. `pactl` default sink monitor
4. First monitor source from `pactl list short sources`
5. `default`

You can inspect current auto-detected source:

```bash
screen-recorder-detect-audio-device
```

## GitHub Actions (Manual Only)

Workflows are intentionally manual (`workflow_dispatch`):

- `.github/workflows/linux-package.yml`
- `.github/workflows/linux-release.yml`

### Package Artifact Only

Run **Linux Package** workflow and provide `version`.

### Publish GitHub Release

Run **Linux Release** workflow and provide:

- `version` (without `v`, for example `1.0.0`)
- `create_release=true`

It builds Linux release bundle, uploads artifacts, and creates tag/release `v<version>`.
