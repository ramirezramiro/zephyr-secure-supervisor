# simple_aes.c

`src/simple_aes.c` implements the tiny AES block primitive that powers zephyr-secure-supervisor encryption. The code is deliberately minimal so it fits the NUCLEO-L053R8’s 8 KB SRAM budget while still passing the MISRA-style audits. The STM32L0 line does ship with an on-die AES accelerator, but we keep this lightweight software path so the same code works on boards without crypto hardware and so MISRA-driven reviews can trace every block operation.

## Responsibilities
- Provide AES-128 key expansion and block encrypt routines with no dynamic allocation.
- Offer a clean boundary for `app_crypto.c` so CTR mode logic and IV tracking live outside this file.
- Avoid direct `memcpy` usage by consuming the inline guards from `safe_memory.h`.

## Interactions
- Only `src/app_crypto.c` includes this file; everything else calls higher-level helpers (`app_crypto_encrypt`, `app_crypto_init`).
- Unit coverage arrives via `tests/unit/misra_stage1`, which exercises full persistence + crypto round trips on hardware.

## Extension Tips
Need to explore PQC or other ciphers? Add a sibling implementation in `src/` (e.g., `simple_kyber.c`) and point `app_crypto.c` at it—the rest of the system remains unchanged.
