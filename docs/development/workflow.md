# Development Workflow

## SigmaStudio changes

A SigmaStudio export is one versioned unit. Graph changes may relocate parameters even
when the affected block was not edited.

After changing the graph or generated files:

1. Compile the complete SigmaStudio design.
2. Replace the complete matching export under `src/SigmaStudioFiles/`.
3. Review the generated diff for missing or partial output.
4. Perform a pristine firmware build.
5. Flash, reset, and verify audio behavior and control logs.

Use `west build --pristine` or `tools/buildprog/buildprog.py --pristine`.

## Generated-file rules

- Use symbols from the current generated headers; do not copy numeric addresses.
- Do not manually edit generated files.
- Keep application aliases descriptive, but do not treat them as address-stable.
- Ensure program, parameter, register, and metadata files come from the same export.
- A successful RAM read/write proves access, not that the running DSP assigns the address
  to the intended block.

Incremental builds are suitable for C-only changes that do not affect generated files,
devicetree, Kconfig, the board, sysbuild domains, or build configuration. When unsure, use
a pristine build to prevent stale application objects from targeting a new DSP layout.
