# Release Checklist

Use this list before tagging or sharing a new zephyr-secure-supervisor drop.

1. **Sync + clean build**
   - `west build -b nucleo_l053r8 -p always .`
   - Confirm there are no local hacks in `prj.conf` or overlays.
2. **Run regression suites**
   - `tests/persist_state` and `tests/supervisor` on native_sim.
   - `tests/unit/misra_stage1` on the NUCLEO-L053R8 and archive the UART log.
3. **Capture artifacts**
   - Memory usage snippet (`ninja size` output) and sample UART log for the README.
   - Update `docs/testing.md` or `tests/README.md` if commands change.
4. **Verify docs + licensing**
   - Ensure `README.md`, `docs/*.md`, `LICENSE`, and `NOTICE` reflect the release name and scope.
5. **Clean workspace**
   - Remove `build/` or other generated directories you don’t intend to commit.
6. **Tag + publish**
   - Create release notes summarizing changes.
   - Push repo to GitHub under the `zephyr-secure-supervisor` name.
