#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

WORKSPACE_DIR="${ZEPHYR_WORKSPACE:-$(pwd)}"

info() {
    printf '==> %s\n' "$*"
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

# Sanity check: ensure west can find the workspace from the current directory
if ! command -v west >/dev/null 2>&1; then
    fail "west not found in PATH"
fi

pushd "${WORKSPACE_DIR}" >/dev/null || fail "Unable to enter workspace ${WORKSPACE_DIR}"

info "Running native_sim tests: tests/persist_state"
west build -b native_sim "${APP_DIR}/tests/persist_state" -p auto \
    --build-dir build/tests/persist_state
west build -t run --build-dir build/tests/persist_state

info "Running native_sim tests: tests/supervisor"
west build -b native_sim "${APP_DIR}/tests/supervisor" -p auto \
    --build-dir build/tests/supervisor
west build -t run --build-dir build/tests/supervisor

info "Building production firmware (nucleo_l053r8)"
west build -b nucleo_l053r8 "${APP_DIR}" -p auto --build-dir build/release

info "Building provisioning firmware overlay (prj_provision.conf)"
west build -b nucleo_l053r8 "${APP_DIR}" -p auto \
    --build-dir build/provision \
    -DOVERLAY_CONFIG=prj_provision.conf

info "All builds/tests completed successfully."

popd >/dev/null
