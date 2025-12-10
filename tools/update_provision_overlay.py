#!/usr/bin/env python3
"""
Update the provisioning overlay with the latest scalar/peer pair from
~/.helium_provision/curve_keys.json (or a specific entry).

Usage:
    ./tools/update_provision_overlay.py \
        --overlay ../prj_provision.conf \
        [--store ~/.helium_provision/curve_keys.json] \
        [--entry 1]
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, List

DEFAULT_STORE = Path.home() / ".helium_provision" / "curve_keys.json"
DEFAULT_OVERLAY = Path(__file__).resolve().parents[1] / "prj_provision.conf"


def _load_entries(store_path: Path) -> List[Dict[str, Any]]:
    if not store_path.exists():
        raise SystemExit(f"error: store '{store_path}' does not exist.")
    try:
        data = json.loads(store_path.read_text())
    except (json.JSONDecodeError, OSError) as exc:
        raise SystemExit(f"error: failed to read '{store_path}': {exc}") from exc
    if not isinstance(data, list) or not data:
        raise SystemExit(f"error: store '{store_path}' is empty or malformed.")
    return data


def _select_entry(entries: List[Dict[str, Any]], entry_index: int | None) -> Dict[str, Any]:
    if entry_index is None:
        return entries[-1]
    if entry_index < 0 or entry_index >= len(entries):
        raise SystemExit(
            f"error: entry index {entry_index} out of range (0..{len(entries) - 1})."
        )
    return entries[entry_index]


def _sanitize_hex(value: str, label: str) -> str:
    if not isinstance(value, str):
        raise SystemExit(f"error: {label} is missing or invalid in the store entry.")
    sanitized = value.strip().lower()
    if len(sanitized) != 64 or not all(c in "0123456789abcdef" for c in sanitized):
        raise SystemExit(f"error: {label} must be 64 hex characters, got '{value}'.")
    return sanitized


def _apply_setting(content: str, key: str, value: str) -> str:
    line = f'{key}="{value}"'
    pattern = re.compile(rf"^{re.escape(key)}=.*$", re.MULTILINE)
    if pattern.search(content):
        return pattern.sub(line, content)
    # append at the end
    if not content.endswith("\n"):
        content += "\n"
    return content + line + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Update provisioning overlay with stored Curve25519 material."
    )
    parser.add_argument(
        "--store",
        type=Path,
        default=DEFAULT_STORE,
        help=f"Path to curve_keys.json (default: {DEFAULT_STORE})",
    )
    parser.add_argument(
        "--overlay",
        type=Path,
        default=DEFAULT_OVERLAY,
        help=f"Provisioning overlay to update (default: {DEFAULT_OVERLAY})",
    )
    parser.add_argument(
        "--entry",
        type=int,
        default=None,
        help="Zero-based index into the store (default: latest entry).",
    )
    args = parser.parse_args()

    entries = _load_entries(args.store)
    entry = _select_entry(entries, args.entry)
    scalar = _sanitize_hex(entry.get("scalar"), "scalar")
    peer = _sanitize_hex(entry.get("peer"), "peer") if entry.get("peer") else None

    try:
        overlay_text = args.overlay.read_text()
    except OSError as exc:
        raise SystemExit(f"error: cannot read overlay '{args.overlay}': {exc}") from exc

    overlay_text = _apply_setting(
        overlay_text, "CONFIG_APP_CURVE25519_STATIC_SECRET_HEX", scalar
    )
    if peer:
        overlay_text = _apply_setting(
            overlay_text, "CONFIG_APP_CURVE25519_STATIC_PEER_PUB_HEX", peer
        )

    try:
        args.overlay.write_text(overlay_text)
    except OSError as exc:
        raise SystemExit(f"error: cannot write overlay '{args.overlay}': {exc}") from exc

    idx_desc = args.entry if args.entry is not None else len(entries) - 1
    print(f"Updated {args.overlay} with entry #{idx_desc} (scalar/peer).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
