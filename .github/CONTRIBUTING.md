<head>
  <meta charset="UTF-8">
  <meta name="description" content="Contributing to ROCm Optiq">
  <meta name="keywords" content="ROCm, contributing, ROCm Optiq">
</head>

# Contributing to ROCm Optiq #

We welcome contributions to ROCm Optiq.  Please follow these details to help ensure your contributions will be successfully accepted.

## Issue Discussion ##

Please use the GitHub Issues tab to notify us of issues.

* Use your best judgement for issue creation. If your issue is already listed, upvote the issue and
  comment or post to provide additional details, such as how you reproduced this issue.
* If you're not sure if your issue is the same, err on the side of caution and file your issue.
  You can add a comment to include the issue number (and link) for the similar issue. If we evaluate
  your issue as being the same as the existing issue, we'll close the duplicate.
* If your issue doesn't exist, use the issue template to file a new issue.
  * When filing an issue, be sure to provide as much information as possible, including script output so
    we can collect information about your configuration. This helps reduce the time required to
    reproduce your issue.
  * Check your issue regularly, as we may require additional information to successfully reproduce the
    issue.
* You may also open an issue to ask questions to the maintainers about whether a proposed change
  meets the acceptance criteria, or to discuss an idea pertaining to the library.

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

#### Internal build system
* `build.cmd` / `pkgbuild.cmd` - Build scripts for Windows
* `Installer/` - Windows installer configuration files

## Coding Style ##

Please refer to [coding style document](../CODING.md) for coding style guidelines.

## Pull Request Guidelines ##

By creating a pull request, you agree to the statements made in the [code license](#code-license) section. Your pull request should target the default branch. Our current default branch is the **main** branch, which serves as our integration branch.

Pull requests should:

- ensure code builds successfully.
- do not break existing test cases.

### Process ###

All pull requests will be reviewed by the appropriate code owners.  Please ensure code is formatted using this [formmatting style](../.clang-format) file.

## Code License ##

All code contributed to this project will be licensed under the license identified in the [License.md](../LICENSE.md). Your contribution will be accepted under the same license.

