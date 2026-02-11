## Optiq Beta 0.2.0

### Features and Improvements

**Summary View**
  - New summary view that displays information about top ten kernels.
  - Duration breakdown plotted on charts.
  - Details presented as a table.

**Mini-map**
  - New mini-map feature shows wholistic view of tracks and events
  - Timeline can be navigated using mini-map.
  - Shows location of current view.

**Timeline**
  - Improved navigation and selection controls.
  - *New* Hold Ctrl+left mouse and drag to perform a time range filter selection.
  - *New* Press left mouse to select / deselect an event.  Press Ctrl+left mouse to add to selection.
  - Context menu option to create a time range filter from a selected event or events.
  - Improved annotation interface. (Annotations start as minimized marker icons).
  - Event name aligns to always be visible on screen if possible.

**Event Details View**
  - Event arguments now displayed.
  - Empty sections not displayed instead of wasting space with headers and no data message.
  - Improved how Callstack section is displayed.

**UI Misc**
  - Improved track names and descriptions.
  - Event Histogram can normalize to all tracks or only visible tracks.
  - When displaying Event Id show database Id instead of internal App Id.
  - Command line parameter support added.  
    - "-f filename" to launch and open file.
    - "-v" to show version number.
  - Fullscreen mode (toggle with F11 key).
  - Use standard OS locations for settings and log files.

**Multi-node**
  - Multinode data now supported.
  - New multi-database yaml file format supported.
  - Improved data processing logic.

**Misc**
  - Project now builds on Mac OS. 

### Fixes

Fix: RPM package generation.
Fix: Default font path detection, and fallback font size generation.
Fix: Log spam in release builds.
Fix: Crash when same event was repeatedly selected/deselected.
Fix: Misc application crashes.
Fix: Build on linux systems that have libtbb-dev installed.


## Optiq Beta 0.1.0

### Features and Improvements

**Timeline**
  - Improved tooltip for event tracks
    - Shows more informative stats when hovered over a combined event
    - Shows table of event names, counts, and durations
    - Shows total event count
  - Combined events now colored by the underlying event that has longest combined duration
  - Sample track graph rendering improved
    - Area under line now filled by default
    - Sample under cursor is now highlighted
    - Tool tip now shows duration
  - Scale for sample tracks can be adjusted by clicking and editing the min/max values
  - New compact mode option that allows tracks with tall flame graphs to be "compressed"

**Histogram**
  - Now aligned with timeline
  - Updated dynamically to only include tracks that have visibility turned on

**Tables**
  - Added feature to export table results to a csv file
  
**UI Misc**
  - Use native OS file dialogs when opening or saving files
  - Improved dark theme colors
  - About Dialog now has link to documentation

### Fixes

  - Fix: Delete temporary files when application exits
  - Fix: Sample tracks graph disappears when zoomed in too deeply
  - Fix: Large values not plotted correctly on sample tracks
  - Fix: Sample track tooltip value displayed wrong value
  - Fix: Don't allow grouping table data by Id
  - Fix: Ignore event with invalid time values
  - Fix: Extension not appended when saving on Linux
  - Fix: Counter tracks not trimmed when saving trimmed trace
  - Fix: No data when scrolling at highest zoom
  - Fix: Samples overlap on track
  - Fix: Crash when re-ordering tracks and view track details

## Alpha 0.6.3

- Initial Alpha build
