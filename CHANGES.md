## 0.1.3

### Features and Improvements

- **Event Table UX**
  - Table can now be scrolled horizontally.
  - Right-clicking on a column header opens a context menu to show/hide columns.
- **Event Table Data**
  - Different types of tracks can now be displayed at the same time in the event table.
  - Event table shows the union of columns when tracks have events with different properties.
- **Event Multi-select**
  - Multiple events can be selected on the timeline.
  - Event details for each selected event can be expaned or collapsed in details pane.
- **Notification/toast system for in-app feedback**
  - Show notifications for copy-to-clipboard and save-trimmed progress feedback.
- **View bookmarking**
  - Save/restore timeline view. (Use keys Ctrl+0..9 to save, 0..9 to restore).
- **Application Icon added**
- **Display Settings**
  - Font size can now be manually adjusted by the user.
  - User settings now persits across session via application settings file.
- **Misc UI Enchancments**
  - Show directional cursor when hovering over the move track grip handle.
  - Only show one event info tool tip when hovering over densely packed events.

### Fixes

- Fixed queries related to memory copy operations.
- Fixed tracks not loading in certain traces.
- Overall stability improvements.
- Loading indicator now visible when table data is being fetched.

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

### Fixes

- Fixed occasional crashes when fetching call stack traces.
- Fixed crash when opening files with missing extended track data.
- Resolved issues with table desynchronization when tracks are quickly unselected.
- Fixed disappearing track issue when using go to track feature on side bar.
- Fixed crash when opening incompatible files.

## 0.1.1

- Initial Pre-alpha build