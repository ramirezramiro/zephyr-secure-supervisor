# Provisioning Helpers

This directory contains the host-side tooling that keeps the provisioning workflow reproducible. Both scripts are invoked from the repo root (inside your Zephyr workspace virtualenv) and mirror the commands documented in the main `README.md` / `docs/crypto_backends.md`.

## `provision_curve.py`

Send the `prov curve <scalar> [peer]` command over `/dev/ttyACM0` with the desired material. Key options:

| Flag | Purpose |
|------|---------|
| `--interactive` | Generate/clamp a fresh scalar+peer pair, print it, and cache it under `~/.helium_provision/curve_keys.json`. |
| `--reuse-stored` | Re-send the most recent entry from the cache. |
| `--no-read` | Skip draining UART output (useful if you plan to view logs later in `screen`). |
| `--no-wait-ready` / `--wait-ready` | Control whether the helper waits for `EVT,UART_CMD,READY` before sending; default is to wait. |
| `--send-delay` | Fixed delay before sending when `--no-wait-ready` is used. |
| `--command-file` | Stream a pre-built command instead of constructing `prov curve …`. |
| `--gdb-script`, `--gdb-run` | Generate (and optionally execute) a GDB script that writes the scalar/peer via debug symbols instead of UART. |

Example (interactive provisioning + auto-persist overlay update):

```bash
python3 tools/provision_curve.py --interactive --device /dev/ttyACM0 \
  --no-read --no-wait-ready --send-delay 7
./tools/update_provision_overlay.py --overlay prj_provision.conf
```

Outputs:
- Prints the scalar/peer pair (when generating). Each run appends an entry to `~/.helium_provision/curve_keys.json` with an ISO timestamp.
- Streams the UART response unless `--no-read` is supplied; look for `app: Provision auto-persist secret=ok peer=ok` to confirm success.

## `update_provision_overlay.py`

Copies a cached scalar/peer pair from `~/.helium_provision/curve_keys.json` into `prj_provision.conf` so the provisioning overlay can auto-persist it.

| Flag | Purpose |
|------|---------|
| `--store PATH` | Use an alternate cache file (default: `~/.helium_provision/curve_keys.json`). |
| `--overlay FILE` | Target overlay (default: `prj_provision.conf`). |
| `--entry N` | Zero-based index into the cache; default is the latest entry. |

Example (latest entry):

```bash
./tools/update_provision_overlay.py --overlay prj_provision.conf
```

Output:
- Overwrites the `CONFIG_APP_CURVE25519_STATIC_*` strings in the overlay and prints which cache entry was used (e.g., `Updated prj_provision.conf with entry #44`).

See the root `README.md` for the full provisioning workflow and release artefacts for sample UART logs.
