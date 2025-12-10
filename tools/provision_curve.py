#!/usr/bin/env python3
"""UART helper for provisioning Curve25519 scalars/peers."""

from __future__ import annotations

import argparse
import binascii
import json
import re
import secrets
import sys
import time
import subprocess
import socket
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

try:
    import serial  # type: ignore
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "pyserial is required. Activate the Zephyr venv and run 'pip install pyserial'."
    ) from exc

CURVE_LEN = 32  # bytes
DEFAULT_SCALAR = "00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF"
DEFAULT_PEER = "A1A2A3A4A5A6A7A8B1B2B3B4B5B6B7B8C1C2C3C4C5C6C7C8D1D2D3D4D5D6D7D8"
DEFAULT_STORE_PATH = Path.home() / ".helium_provision" / "curve_keys.json"
READY_MARKERS = ("EVT,UART_CMD,READY", "EVT,APP,READY")
ACK_MARKERS = ("EVT,PROVISION,CURVE25519_UPDATED",)


@dataclass
class CurveMaterial:
    scalar: str
    peer: str | None


def _normalize_hex(value: str) -> str:
    """Remove whitespace so heredocs / wrapped strings still validate."""
    return "".join(value.split())


def _hex_key(value: str) -> str:
    value = _normalize_hex(value)
    if len(value) != CURVE_LEN * 2:
        raise argparse.ArgumentTypeError(
            f"expected {CURVE_LEN * 2} hex chars, got {len(value)}"
        )
    try:
        binascii.unhexlify(value)
    except binascii.Error as exc:  # pragma: no cover
        raise argparse.ArgumentTypeError(f"invalid hex: {exc}") from exc
    return value.lower()


def generate_scalar() -> str:
    scalar = bytearray(secrets.token_bytes(CURVE_LEN))
    scalar[0] &= 248
    scalar[31] &= 127
    scalar[31] |= 64
    return scalar.hex()


def generate_peer() -> str:
    return secrets.token_bytes(CURVE_LEN).hex()


def _store_material(material: CurveMaterial, store_path: Path) -> None:
    entry = {
        "scalar": material.scalar,
        "peer": material.peer,
        "generated_at": datetime.now(timezone.utc).isoformat(),
    }
    store_path.parent.mkdir(parents=True, exist_ok=True)
    data: list[dict[str, str | None]] = []
    if store_path.exists():
        try:
            data = json.loads(store_path.read_text())
            if not isinstance(data, list):
                raise ValueError("storage payload is not a list")
        except (json.JSONDecodeError, OSError, ValueError) as exc:
            print(
                f"warning: could not read existing store '{store_path}': {exc}.",
                file=sys.stderr,
            )
            data = []
    data.append(entry)
    store_path.write_text(json.dumps(data, indent=2))
    print(f"Stored generated values in {store_path}")


def _load_latest_material(store_path: Path) -> CurveMaterial:
    if not store_path.exists():
        raise SystemExit(f"error: no stored values at '{store_path}'.")
    try:
        data = json.loads(store_path.read_text())
    except (json.JSONDecodeError, OSError) as exc:
        raise SystemExit(f"error: failed to read store '{store_path}': {exc}") from exc
    if not isinstance(data, list) or not data:
        raise SystemExit(f"error: store '{store_path}' is empty.")
    entry = data[-1]
    if not isinstance(entry, dict):
        raise SystemExit(f"error: malformed entry in '{store_path}'.")
    scalar = entry.get("scalar")
    peer = entry.get("peer")
    if not isinstance(scalar, str):
        raise SystemExit("error: stored scalar is missing or invalid.")
    scalar = _hex_key(scalar)
    peer_value: str | None = None
    if isinstance(peer, str):
        peer_value = _hex_key(peer)
    print(f"Loaded stored values from {store_path}")
    return CurveMaterial(scalar=scalar, peer=peer_value)


def _interactive_generate(store_path: Path, should_store: bool) -> CurveMaterial:
    print("Interactive Curve25519 provisioning helper")
    input("Press Enter to generate a new scalar…")
    scalar = generate_scalar()
    print(f"Scalar (hex): {scalar}")
    input("Press Enter to generate a peer public key…")
    peer = generate_peer()
    print(f"Peer (hex): {peer}")
    material = CurveMaterial(scalar=scalar, peer=peer)
    if should_store:
        _store_material(material, store_path)
    else:
        print("Storage disabled; values will not be written to disk.")
    input("Press Enter to continue and provision the device (Ctrl-C to abort)…")
    return material


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Send 'prov curve' over UART")
    parser.add_argument("scalar", nargs="?", help="64-hex Curve25519 scalar")
    parser.add_argument("peer", nargs="?", help="Optional 64-hex peer public key")
    parser.add_argument("--scalar", dest="scalar_opt",
                        help="64-hex Curve25519 scalar (flag form)")
    parser.add_argument("--peer", dest="peer_opt",
                        help="Optional 64-hex peer public key (flag form)")
    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument(
        "--interactive",
        action="store_true",
        help="Interactively generate a random scalar/peer before sending.",
    )
    mode_group.add_argument(
        "--reuse-stored",
        action="store_true",
        help="Reuse the most recently stored scalar/peer pair.",
    )
    parser.set_defaults(wait_ready=True)
    parser.add_argument(
        "--demo",
        action="store_true",
        help=(
            "Send the built-in RFC 7748 test vectors. "
            "Use only for quick UART validation; real provisioning requires explicit material."
        ),
    )
    parser.add_argument(
        "--store-path",
        type=Path,
        default=DEFAULT_STORE_PATH,
        help="Where to cache generated scalar/peer values (default: %(default)s).",
    )
    parser.add_argument(
        "--no-store",
        action="store_true",
        help="Skip writing generated values to disk when using --interactive.",
    )
    parser.add_argument(
        "--device", default="/dev/ttyACM0", help="Serial port (default: %(default)s)"
    )
    parser.add_argument(
        "--command-file",
        type=Path,
        help="Send a raw command from this file instead of constructing 'prov curve …'.",
    )
    parser.add_argument(
        "--gdb-script",
        type=Path,
        help=(
            "Emit a GDB command file that copies the scalar/peer into the "
            "firmware staging buffers (gdb_curve_* symbols) and calls the "
            "persist_state helpers. Skips all UART activity."
        ),
    )
    parser.add_argument(
        "--gdb-run",
        action="store_true",
        help=(
            "Automatically start 'west debugserver', run arm-zephyr-eabi-gdb "
            "with the generated script, and tear everything down when done."
        ),
    )
    parser.add_argument(
        "--gdb-path",
        type=Path,
        default=Path.home() / "zephyr-sdk-0.17.4/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb",
        help="Path to arm-zephyr-eabi-gdb when using --gdb-run "
             "(default: %(default)s).",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path.home() / "zephyrproject/build/provision",
        help="Zephyr build directory passed to west/gdb when using --gdb-run "
             "(default: %(default)s).",
    )
    parser.add_argument(
        "--baud", type=int, default=115200, help="Baud rate (default: %(default)d)"
    )
    parser.add_argument(
        "--wait",
        type=float,
        default=10.0,
        help="Seconds to wait for device output (default: %(default)s). "
             "Set to 0 to skip reading.",
    )
    parser.add_argument(
        "--send-delay",
        type=float,
        default=7.0,
        help="Seconds to wait before transmitting (default: %(default)s). "
             "Helps give the MCU time to reboot/settle before provisioning.",
    )
    parser.add_argument(
        "--no-read",
        action="store_true",
        help="Write the command and exit without draining UART output. "
             "Useful if you want to read the response later via screen/minicom.",
    )
    parser.add_argument(
        "--wait-ready",
        dest="wait_ready",
        action="store_true",
        help="Wait for the device to print ready markers before provisioning (default).",
    )
    parser.add_argument(
        "--no-wait-ready",
        dest="wait_ready",
        action="store_false",
        help="Skip waiting for UART ready banners and use --send-delay instead.",
    )
    parser.add_argument(
        "--ready-timeout",
        type=float,
        default=15.0,
        help="Seconds to wait for UART ready markers when --wait-ready is enabled "
             "(default: %(default)s).",
    )
    return parser.parse_args()


def _resolve_arg(
    positional: str | None,
    optional: str | None,
    stdin_values: list[str],
    label: str,
) -> str | None:
    value = positional if positional is not None else optional

    if value == "-":
        if not stdin_values:
            raise SystemExit(f"error: expected {label} value from stdin")
        value = stdin_values.pop(0)

    if value is None:
        return None

    return _hex_key(value)


def _material_from_args(
    args: argparse.Namespace, stdin_values: list[str]
) -> CurveMaterial:
    if args.demo:
        print(
            "warning: --demo selected; sending RFC 7748 test vectors. "
            "Do not ship hardware provisioned with these values.",
            file=sys.stderr,
        )
        return CurveMaterial(scalar=DEFAULT_SCALAR, peer=DEFAULT_PEER)

    scalar = _resolve_arg(args.scalar, args.scalar_opt, stdin_values, "scalar")
    if not scalar:
        raise SystemExit(
            "error: no scalar provided. Use --scalar/--interactive/--reuse-stored "
            "or pass --demo to transmit the RFC 7748 test vector."
        )

    peer = _resolve_arg(args.peer, args.peer_opt, stdin_values, "peer")

    return CurveMaterial(scalar=scalar, peer=peer)


_ANSI_RE = re.compile(r"\x1B\[[0-?]*[ -/]*[@-~]")


def _strip_ansi(value: str) -> str:
    return _ANSI_RE.sub("", value)


def _drain_until_ready(
    ser: serial.Serial, timeout: float, markers: tuple[str, ...]
) -> bool:
    """Read UART output until any marker is seen or timeout expires."""
    deadline = time.monotonic() + timeout
    window = ""
    while True:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            text = chunk.decode(errors="ignore")
            sys.stdout.write(text)
            sys.stdout.flush()
            window = (window + _strip_ansi(text))[-512:]
            if any(marker in window for marker in markers):
                return True
        else:
            if time.monotonic() >= deadline:
                return False
            time.sleep(0.05)


def _drain_response(
    ser: serial.Serial, wait_time: float, ack_markers: tuple[str, ...]
) -> bool:
    """Drain UART output after sending the provisioning command."""
    print("=== Device response ===")
    deadline = time.monotonic() + wait_time
    window = ""
    ack_seen = False
    while True:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            text = chunk.decode(errors="ignore")
            sys.stdout.write(text)
            sys.stdout.flush()
            window = (window + _strip_ansi(text))[-1024:]
            if any(marker in window for marker in ack_markers):
                ack_seen = True
            deadline = max(deadline, time.monotonic() + 0.2)
        else:
            if time.monotonic() >= deadline:
                break
            time.sleep(0.05)
    print("=== End ===")
    return ack_seen


def _send_curve_command(
    material: CurveMaterial,
    device: str,
    baud: int,
    wait_time: float,
    send_delay: float,
    no_read: bool,
    wait_ready: bool,
    ready_timeout: float,
    command_file: Path | None,
) -> None:
    if command_file is not None:
        payload: bytes = command_file.read_bytes()
        payload = payload.rstrip(b"\r\n") + b"\r\n"
    else:
        text = f"prov curve {material.scalar}"
        if material.peer:
            text += f" {material.peer}"
        payload = (text + "\r\n").encode("ascii")

    print(f"Opening {device} @ {baud} baud…", file=sys.stderr)
    with serial.Serial(device, baud, timeout=0.5) as ser:
        if wait_ready:
            print(
                f"Waiting up to {ready_timeout:.1f} s for UART ready markers…",
                file=sys.stderr,
            )
            ready = _drain_until_ready(ser, ready_timeout, READY_MARKERS)
            if ready:
                print("Device reported ready; provisioning…", file=sys.stderr)
            else:
                print(
                    "warning: ready markers not seen before timeout; continuing anyway.",
                    file=sys.stderr,
                )
        else:
            if not no_read:
                ser.reset_input_buffer()
            if command_file is None and send_delay > 0:
                print(
                    f"Waiting {send_delay:.1f} s before provisioning…",
                    file=sys.stderr,
                )
                time.sleep(send_delay)
        max_attempts = 2
        attempts = 0
        prompt_allowed = sys.stdin.isatty()
        ack_seen = False

        while attempts < max_attempts and not ack_seen:
            attempts += 1
            if attempts > 1:
                print(
                    f"Retrying provisioning attempt {attempts} of {max_attempts}…",
                    file=sys.stderr,
                )
            ser.write(payload)
            ser.flush()

            if no_read or wait_time <= 0:
                print(
                    "Command sent. Re-open your UART monitor to view the response."
                )
                return

            ack_seen = _drain_response(ser, wait_time, ACK_MARKERS)
            if ack_seen:
                break

            if attempts < max_attempts:
                if prompt_allowed:
                    answer = input(
                        "Provisioning acknowledgement not observed. "
                        "Resend the same scalar? [Y/n]: "
                    ).strip().lower()
                    if answer.startswith("n"):
                        break
                else:
                    print(
                        "warning: provisioning acknowledgement missing; "
                        "skipping retry (non-interactive session).",
                        file=sys.stderr,
                    )
                    break

        if not ack_seen:
            print(
                "warning: scalar not reaching hardware; "
                "check the provisioning build, UART cabling, and try again.",
                file=sys.stderr,
    )


def _python_gdb_block(symbol: str, label: str, hex_value: str) -> list[str]:
    return [
        "  python",
        f'import binascii, gdb',
        f'data = binascii.unhexlify("{hex_value}")',
        f'addr = int(gdb.parse_and_eval("(unsigned long)&{symbol}[0]"))',
        "gdb.selected_inferior().write_memory(addr, data)",
        f'gdb.write("Loaded {label} ({len(hex_value) // 2} bytes) into {symbol}\\n")',
        "end",
    ]


def _emit_gdb_script(material: CurveMaterial, script_path: Path, show_example: bool) -> None:
    if not material.scalar:
        raise SystemExit("error: GDB mode requires a scalar value.")

    script_path.parent.mkdir(parents=True, exist_ok=True)

    lines: list[str] = ["set pagination off", "", "define provision_curve"]
    lines.append("  monitor reset halt")
    lines.extend(_python_gdb_block("gdb_curve_secret_buf", "scalar", material.scalar))
    lines.append(
        "  set $rc = persist_state_curve25519_set_secret(gdb_curve_secret_buf)"
    )
    lines.append(
        '  printf "persist_state_curve25519_set_secret() -> %ld\\n", $rc'
    )

    if material.peer:
        lines.extend(_python_gdb_block("gdb_curve_peer_buf", "peer", material.peer))
        lines.append(
            "  set $rc = persist_state_curve25519_set_peer(gdb_curve_peer_buf)"
        )
        lines.append(
            '  printf "persist_state_curve25519_set_peer() -> %ld\\n", $rc'
        )
    else:
        lines.append('  printf "warning: no peer provided; skipping peer write\\n"')

    lines.append("  monitor reset run")
    lines.append("end")
    lines.append("")
    lines.append("provision_curve")
    lines.append("quit")

    script_path.write_text("\n".join(lines))
    print(f"Wrote GDB provisioning script to {script_path}")
    if show_example:
        print("Example session:")
        print("  # Terminal A")
        print("  west debugserver -r openocd --build-dir <build-dir>")
        print("  # Terminal B")
        print(
            f"  arm-zephyr-eabi-gdb -q -x {script_path} "
            "<build-dir>/zephyr/zephyr.elf"
        )


def _wait_for_port(host: str, port: int, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(0.5)
            try:
                sock.connect((host, port))
            except OSError:
                time.sleep(0.2)
                continue
            return True
    return False


def _run_gdb_flow(script_path: Path, build_dir: Path, gdb_path: Path) -> None:
    if not script_path.exists():
        raise SystemExit(f"error: GDB script '{script_path}' does not exist.")
    elf_path = build_dir / "zephyr" / "zephyr.elf"
    if not elf_path.exists():
        raise SystemExit(f"error: missing ELF at '{elf_path}'. Build the firmware first.")
    if not gdb_path.exists():
        raise SystemExit(f"error: arm-zephyr-eabi-gdb not found at '{gdb_path}'.")

    debug_cmd = [
        "west",
        "debugserver",
        "-r",
        "openocd",
        "--build-dir",
        str(build_dir),
    ]
    print(f"Starting debugserver: {' '.join(debug_cmd)}")
    debug_proc = subprocess.Popen(debug_cmd)
    try:
        if not _wait_for_port("127.0.0.1", 3333, timeout=15.0):
            raise SystemExit(
                "error: timed out waiting for OpenOCD to listen on port 3333."
            )
        gdb_cmd = [
            str(gdb_path),
            "-q",
            "-ex",
            "target remote :3333",
            "-x",
            str(script_path),
            str(elf_path),
        ]
        print(f"Running GDB: {' '.join(gdb_cmd)}")
        result = subprocess.run(gdb_cmd, check=False)
        if result.returncode != 0:
            raise SystemExit(f"error: GDB exited with status {result.returncode}.")
    finally:
        if debug_proc.poll() is None:
            debug_proc.terminate()
            try:
                debug_proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                debug_proc.kill()
def main() -> int:
    args = parse_args()

    stdin_values: list[str] = []
    if not sys.stdin.isatty():
        stdin_values = sys.stdin.read().split()

    if isinstance(args.store_path, str):
        store_path = Path(args.store_path)
    else:
        store_path = args.store_path

    if args.command_file is not None:
        if args.gdb_script is not None or args.gdb_run:
            raise SystemExit(
                "error: --command-file cannot be combined with GDB modes."
            )

    if args.command_file is not None:
        material = CurveMaterial(scalar="", peer=None)
    elif args.interactive:
        material = _interactive_generate(store_path, should_store=not args.no_store)
    elif args.reuse_stored:
        material = _load_latest_material(store_path)
    else:
        material = _material_from_args(args, stdin_values)

    if args.gdb_script or args.gdb_run:
        script_path: Path
        if args.gdb_script:
            script_path = Path(args.gdb_script)
        else:
            tmp = tempfile.NamedTemporaryFile(prefix="provision_curve_", suffix=".gdb", delete=False)
            tmp.close()
            script_path = Path(tmp.name)
        _emit_gdb_script(material, script_path, show_example=not args.gdb_run)
        if args.gdb_run:
            _run_gdb_flow(
                script_path=script_path,
                build_dir=Path(args.build_dir),
                gdb_path=Path(args.gdb_path),
            )
        return 0

    _send_curve_command(
        material,
        device=args.device,
        baud=args.baud,
        wait_time=args.wait,
        send_delay=args.send_delay,
        no_read=args.no_read,
        wait_ready=args.wait_ready,
        ready_timeout=args.ready_timeout,
        command_file=args.command_file,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
