#!/usr/bin/env bash
set -euo pipefail

log() { printf '[install] %s\n' "$*"; }
err() { printf '[install][error] %s\n' "$*" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="${SCRIPT_DIR}/app"
TARGET_DIR="/opt/screen-recorder"
BIN_DIR="/usr/local/bin"
DESKTOP_DIR="${HOME}/.local/share/applications"
RUN_PREFLIGHT="${RUN_PREFLIGHT:-1}"

[[ -d "${APP_DIR}" ]] || err "Missing app directory in bundle: ${APP_DIR}"
[[ -x "${APP_DIR}/flutter_app" ]] || err "Missing executable: ${APP_DIR}/flutter_app"

if ! command -v sudo >/dev/null 2>&1; then
  err "sudo is required"
fi

log "Installing app bundle to ${TARGET_DIR}"
sudo mkdir -p "${TARGET_DIR}"
sudo rm -rf "${TARGET_DIR:?}/"*
sudo cp -a "${APP_DIR}/." "${TARGET_DIR}/"

log "Installing helper commands"
sudo install -Dm755 "${SCRIPT_DIR}/bin/screen-recorder-detect-audio-device" "${TARGET_DIR}/bin/screen-recorder-detect-audio-device"
sudo install -Dm755 "${SCRIPT_DIR}/bin/screen-recorder-preflight" "${TARGET_DIR}/bin/screen-recorder-preflight"

if [[ -d "${SCRIPT_DIR}/preflight" ]]; then
  sudo mkdir -p "${TARGET_DIR}/preflight"
  sudo rm -rf "${TARGET_DIR}/preflight/"*
  sudo cp -a "${SCRIPT_DIR}/preflight/." "${TARGET_DIR}/preflight/"
fi

sudo ln -sf "${TARGET_DIR}/flutter_app" "${BIN_DIR}/screen-recorder"
sudo ln -sf "${TARGET_DIR}/bin/screen-recorder-detect-audio-device" "${BIN_DIR}/screen-recorder-detect-audio-device"
sudo ln -sf "${TARGET_DIR}/bin/screen-recorder-preflight" "${BIN_DIR}/screen-recorder-preflight"

mkdir -p "${DESKTOP_DIR}"
install -m 0644 "${SCRIPT_DIR}/desktop/screen-recorder.desktop" "${DESKTOP_DIR}/screen-recorder.desktop"

log "Desktop entry installed at ${DESKTOP_DIR}/screen-recorder.desktop"

if [[ "${RUN_PREFLIGHT}" == "1" ]]; then
  log "Running distro preflight"
  screen-recorder-preflight || err "Preflight failed"
fi

log "Install complete"
log "Run with: screen-recorder"
