## 0.1.6

### Features and Improvements

- **Projects**
  - Projects can now be saved and opened (.rpv files).
  - A project will save: 
    - The track layout (order, sizes, coloring options, etc).
    - Bookmark information.
    - User annotations.

- **Annotations**
  - Annotations can be placed on the timeline.
  - Support for editing / moving / deleting annotation notes. 
  - Annotation list view added to bottom pane.

- **Toolbar**
  - Toolbar control added.
  - Annotation controls (show, hide, add) added to toolbar.
  - Flow visibility controls added to toolbar.
  - Bookmark controls added to toolbars.

- **Files**
  - Recently opened files list added to file menu for convenience.
  - Drag and drop file support added. Multiple traces can be dropped and opened at the same time.

- **Settings**
  - Improvement to layout and functionality of settings dialog.
  - Time formatting section added.
  - Time format setting applied to items in timeline view.

- **Event/Sample Tables**
  - Fixed "group by" query behaviour.
  - Improved query interface.
  - Added right click menu option to jump to highlighted event in timeline
  - Improved responsiveness. (Allow outdated data requests to be cancelled).

- **Timeline**
  - Improved event coloring options.
  - Events now colored by name by default.
  - Added Menu option to unselect all track or events.
  - Added indicator to track description area that shows if track should be expanded to vertically to show hidden stacked events.

- **Misc UI**
  - Show progress messages on loading popup.
  - Added banner indicating build is for internal users only.

### Fixes

- Rocpd (.db) file processing improvements:
  - Sample counter tracks now loaded.
  - Flow events now rendered.
  - Thread tracks separated into thread and sampled thread tracks.
- UI:
  - Fixed flow rendering issues.
  - Improved event details layout.
- Event Table view:
  - Fixed "group by" query behaviour.
- Fixed topology mis-categorization issues.
- Fixed sidebar control id collisions.
- Fixed crash on exit.
- Fixed zoom related crash.
- Fixed track data not being requested occasionally when scrolling vertically.

### Known Issues

- Stream tracks may overlap events incorrectly.
- Time formatting not applied to all fields that display time data.

## 0.1.4

### Features and Improvements

- **Event Details**
  - Event names added to event flow table.
  - Option added to show and hide flow arrows. Press F3 to show flow control panel.

### Fixes

- Fixed issues opening .db files without levels table.
- Fixed issue where some tracks would not initially load.
- Fixed hidden (internal) columns appearing group-by combo box.
- Fixed table aggregation not working for .rpd files.
- Fixed flow arrow direction flag not being used.

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