## 0.1.2

### Features and Improvements

- **Topology Tree and Streams**
  - Added streams to the topology tree.
  - Added track details tab where additional track topology information is displayed.
  - Sidebar and track details have been reorganized for easier navigation and analysis.
  - Track selection reflected in its sidebar node.
- **Stream and Memory Operation Tracks**: Introduced new tracks for streams and memory operations.
- **Trace Trimming**: Added feature for allowing traces to a be trimmed to a selected time range.
- **UI and Usability Enhancements**:
  - Added a confirmation dialog for critical actions.
  - Added context menu to tables for copying row data to clipboard.
  - Added new button icons.
  - Improved color themes, dark mode, and visual polish throughout the UI.
  - Use better looking built-in OS fonts.
  - Adaptively scale fonts based on DPI scale settings.

### Bug Fixes

- Fixed occasional crashes when fetching call stack traces.
- Fixed crash when opening files with missing extended track data.
- Resolved issues with table desynchronization when tracks are quickly unselected.
- Fixed disappearing track issue when using go to track feature on side bar.
- Fixed crash when opening incompatible files.

## 0.1.1

- Initial Pre-alpha build