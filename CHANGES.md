## Optiq Beta 0.1.0

### Features and Improvements

** Timeline **
  - Improved tooltip for event tracks
    - Shows more informative stats when hovered over a combined event
    - Shows table of event names, counts, and durations
    - Shows total event count
  - Combined events now colored by underlying event that has longest combined duration
  - Sample track graph rendering improved
    - Area under line now filled by default
    - Sample under cursor is now highlighted
    - Tool tip now shows duration
  - Scale for sample tracks can be adjusted by clicking and editting the min/max values
  - New compact mode option that allows tracks with tall flame graphs to be "compressed"

** Histogram **
  - Now aligned with timeline
  - Updated dynamically to only include tracks that have visibility turned on

** Tables **
  - Added feature to export table results to a csv file
  
** UI Misc **
  - Use native OS file dialogs when openinng or saving files
  - Improved dark theme colors
  - About Dialog now has link to documentation

### Fixes

  - Fix: Delete temporary files when application exits
  - Fix: Sample tracks graph disappears when zoomed in too deeply
  - Fix: Large values not plotted correctly on sample tracks
  - Fix: Sample track tooltip value displayed wrong value
  - Fix: Don't allow grouping table data by Id
  - Fix: Ignore event with invalid time values

## Alpha 0.6.3

- Initial Alpha build
