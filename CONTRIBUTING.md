# Contributing to Blockworld

Blockworld is a small C++26 farming and voxel-building game for macOS. Bug fixes,
clearer controls, farming improvements, and performance work are welcome.

Follow the build instructions in [README.md](README.md), then run:

```sh
cmake --build --preset dev
ctest --preset dev
```

For changes to rendering or input, also play the game and describe what you
checked. The CPU previews and unit tests do not verify a native Metal window.
Use `--no-save` or `--world-dir /path/to/test-world` when experimenting with
world generation or save changes.

Keep existing worlds compatible. Block and item IDs are stable, and save-format
changes need migration and malformed-save coverage. Do not commit saved worlds,
build outputs, or personal machine settings.

For bug reports, include your macOS version, processor, game version, steps to
reproduce, and relevant logs or screenshots. For pull requests, explain the
problem, resulting behavior, and validation. Keep changes focused.

Contributions are licensed under the project's [MIT license](LICENSE).
