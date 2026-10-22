# Contributing to roc-optiq

We are enthusiastic about contributions to our code and documentation. Please
feel free to file issues where documentation or functionality is lacking or,
even better, volunteer to contribute to help close these gaps!

______________________________________________________________________

> **Security vulnerabilities** — do not open a public GitHub Issue. See [SECURITY.md](SECURITY.md) for the private reporting process.

______________________________________________________________________

## Developer policies

These policies apply to all forms of activity and engagement in this project.

> [!IMPORTANT]
> AMD employees must also follow the ROCm open source software
> contributing policies at http://u.amd.com/rocm-oss-policies.

### Governance

This project is covered by the
[ROCm Project Governance](https://github.com/ROCm/ROCm/blob/develop/GOVERNANCE.md),
which also defines the code of conduct.

### Licensing

Code contributions to this project are covered under the terms of the
[LICENSE.md](LICENSE.md) file.

### Communication channels

Issue tracking and code contributions are managed in GitHub.
We use an open-source toolchain so that workflows can be easily replicated in any fork.

______________________________________________________________________

## Development workflows

### Issue tracking

Before filing a new issue, search through
[existing issues](https://github.com/ROCm/roc-optiq/issues) to avoid duplicates.

General guidelines:

- If your issue is already listed, upvote it and add a comment with reproduction details.
- When in doubt, file a new issue — we'll mark duplicates accordingly.
- Provide as much information as possible: ROCm Optiq version, opened trace file/profiling
  data, screen captures, and system information. This significantly reduces triage time.
- Check your issue regularly — we may ask follow-up questions.

## Acceptance Criteria ##

The aim of ROCm Optiq is to visualize data captured by ROCm profiling tools. 

Contributors wanting to submit new implementations, improvements, or bug fixes should follow the below mentioned guidelines.

## Code Structure ##

ROCm Optiq follows a modular architecture with clear separation of concerns:

### Source Code (`src/`)

* **`src/app/`** - Application entry point and main window initialization
  
* **`src/core/`** - Core library with fundamental data structures and utilities
  * Public API headers in `inc/`

* **`src/model/`** - Data model layer responsible for trace file parsing and Database access layer (SQLite-based trace file parsing)
  * Public API in `inc/`

* **`src/controller/`** - Business logic layer, implements the controller pattern
  * Handles trace loading, timeline management, event processing, and data transformations
  * Provides the bridge between the model and view layers
  * Public API in `inc/`

* **`src/view/`** - User interface layer (ImGui-based)
  * Implements timeline visualization, event tables, analysis views, and other UI components

### Third-Party Dependencies (`thirdparty/`)

External libraries including ImGui, GLFW, SQLite, spdlog, JSON parsers, and others.

### Build System and Packaging

* `CMakeLists.txt` - Root CMake configuration
* `CMakePresets.json` - CMake preset configurations for different platforms and build types
* `.github/workflows/` - CI/CD workflows for building packages on Ubuntu, Oracle Linux, Rocky Linux, and Windows
* `wix/` - WiX v4 MSI sources, built by the `PACKAGE_WIX` CMake target

### Code style

See [CODING.md](CODING.md).

### New feature or test development

Discussion about new features and test areas is welcome via:

- Filing a [GitHub issue](https://github.com/ROCm/roc-optiq/issues)
- Creating a discussion [GitHub discussions](https://github.com/ROCm/roc-optiq/discussions)

### Pull Requests

All contributions should be submitted through a Pull Request (PR).
Your PR should target the default branch.  Our current default branch is the **main** branch, which serves as our integration branch.

Before submitting a PR:

- Agree to the statements made in the [Licensing](#Licensing) section
- Ensure the change addresses an existing issue or documented requirement.
- Run all applicable tests and validate the results.
- Update documentation as needed.
- Ensure all required GitHub Actions checks pass.
- Resolve review feedback before requesting approval.

PRs require:

- Approval from the appropriate CODEOWNERS.
- Successful completion of required status checks.
- Compliance with repository security and quality requirements.

### Branching

- Do not commit directly to protected branches.
- Create a feature branch for all changes.
- Keep changes focused and atomic.
- Rebase or update branches regularly to minimize merge conflicts.

### Commit Messages

- Use clear and descriptive commit messages.

### Testing

Contributors are expected to:

- Execute relevant unit and integration tests.
- Verify that existing functionality is not impacted.
- Include test coverage for new functionality when applicable.
- Document any known limitations or test exclusions.

### Documentation

Changes that impact functionality, configuration, APIs, or user workflows should include corresponding documentation updates.

### Security Requirements

Contributors must not:

- Commit secrets, tokens, passwords, or credentials.
- Introduce vulnerable dependencies without justification.
- Bypass security controls or required security reviews.

All contributions may be subject to automated security scanning.
