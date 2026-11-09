# Contributing to AME – Adaptive Mix Exchange

Thanks for your interest in AME! Bug reports, ideas and pull requests are welcome.

## Reporting bugs
Please use the **Bug report** issue template and include:
- AME version (shown on the name plate in the plug-in), Windows version, DAW and DAW version
- What you did, what you expected and what happened
- Whether the problem also occurs with a single Line and no Exchange

## Suggesting features
Use the **Feature request** template. Describe the mixing problem you want to solve, not only the control you would like to have.

## Pull requests
1. Fork the repository and create a branch from `main`.
2. Build and run the smoke test (see [README](README.md#building-from-source)):
   ```bat
   cmake -B build -A x64 -DAME_BUILD_TESTS=ON
   cmake --build build --config Release
   build\AMESmokeTest_artefacts\Release\AMESmokeTest.exe
   ```
3. Keep pull requests focused; describe what changes and why.
4. Update the manual (`docs/manual/manual_en.md` and `handbuch_de.md`) and `CHANGELOG.md` when behaviour changes.

### Code guidelines
- C++20, JUCE 8. No allocations or locks in the audio thread.
- **Never change existing parameter IDs**, the plug-in ID or the state type – saved projects must keep loading. New parameters need neutral defaults.
- UI texts in both languages: `tr ("Deutsch", "English")`; parameter names shown in the DAW are English.
- New source files start with the license header:
  ```cpp
  // AME – Adaptive Mix Exchange · Copyright (C) 2026 Batze the bit builder
  // SPDX-License-Identifier: AGPL-3.0-only
  ```

## License
By contributing you agree that your contribution is licensed under the [GNU AGPLv3](LICENSE), like the rest of the project.
