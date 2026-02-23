#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-}"
if [[ -z "${VERSION}" ]]; then
  echo "Usage: $0 <version>" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUNDLE_DIR="${ROOT_DIR}/build/linux/x64/release/bundle"
DIST_DIR="${ROOT_DIR}/dist"
OUT_DIR="${DIST_DIR}/screen-recorder-${VERSION}"
ARCHIVE_PATH="${DIST_DIR}/screen-recorder-${VERSION}-linux-x64.tar.gz"

[[ -d "${BUNDLE_DIR}" ]] || {
  echo "Release bundle not found at ${BUNDLE_DIR}. Run: flutter build linux --release" >&2
  exit 1
}

rm -rf "${OUT_DIR}"
mkdir -p "${OUT_DIR}" "${OUT_DIR}/bin" "${OUT_DIR}/desktop" "${OUT_DIR}/preflight"

cp -a "${BUNDLE_DIR}/." "${OUT_DIR}/app/"
cp "${ROOT_DIR}/packaging/install.sh" "${OUT_DIR}/install.sh"
cp "${ROOT_DIR}/packaging/bin/screen-recorder-detect-audio-device" "${OUT_DIR}/bin/screen-recorder-detect-audio-device"
cp "${ROOT_DIR}/packaging/bin/screen-recorder-preflight" "${OUT_DIR}/bin/screen-recorder-preflight"
cp "${ROOT_DIR}/packaging/desktop/screen-recorder.desktop" "${OUT_DIR}/desktop/screen-recorder.desktop"

if [[ -d "${ROOT_DIR}/../scripts" ]]; then
  cp "${ROOT_DIR}/../scripts/setup_debian_ubuntu.sh" "${OUT_DIR}/preflight/setup_debian_ubuntu.sh"
  cp "${ROOT_DIR}/../scripts/setup_fedora_rhel.sh" "${OUT_DIR}/preflight/setup_fedora_rhel.sh"
  cp "${ROOT_DIR}/../scripts/setup_arch_endeavouros.sh" "${OUT_DIR}/preflight/setup_arch_endeavouros.sh"
else
  echo "Warning: ../scripts not found; preflight scripts were not included" >&2
fi

chmod +x "${OUT_DIR}/install.sh" \
         "${OUT_DIR}/bin/screen-recorder-detect-audio-device" \
         "${OUT_DIR}/bin/screen-recorder-preflight" \
         "${OUT_DIR}/preflight/setup_debian_ubuntu.sh" \
         "${OUT_DIR}/preflight/setup_fedora_rhel.sh" \
         "${OUT_DIR}/preflight/setup_arch_endeavouros.sh" || true

mkdir -p "${DIST_DIR}"
rm -f "${ARCHIVE_PATH}" "${ARCHIVE_PATH}.sha256"

tar -C "${DIST_DIR}" -czf "${ARCHIVE_PATH}" "screen-recorder-${VERSION}"
sha256sum "${ARCHIVE_PATH}" > "${ARCHIVE_PATH}.sha256"

echo "Created: ${ARCHIVE_PATH}"
echo "Checksum: ${ARCHIVE_PATH}.sha256"
