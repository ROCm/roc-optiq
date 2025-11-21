.. meta::
  :description: Component how-to
  :keywords: Component, ROCm, API, how-to 

******************************
Customize a ROCm-Optiq project
******************************

.. |book| image:: ../images/bookmarks.png
.. |pen| image:: ../images/pencil.png
.. |flow| image:: ../images/flow-change.png

You can customize the data views of opened trace files in ROCm-Optiq such as display settings, saved trace selections, added bookmarks / annotations, and more.

Apply a time range filter
=========================

Set a time range filter in the **Timeline View** to limit that data displayed to a specific period. 

To set a time range filter, double-click a range in the **Timeline View**.

.. image:: ../images/filter.png

Double-click in the **Timeline View** again to clear the selection.

.. image:: ../images/time-range-filter.gif

The active time range filter applies to Event and sample Counter details in the **Advanced Details** section.

Save trace selections
---------------------

When there's an active time range filter, the trace can be trimmed from the **Edit** > **Save Trace Selection** menu option:

.. image:: ../images/save-trace.png

This creates a new trace file containing only the events in the filter.

Add an annotation
=================

Annotations are customized notes you can add to any area of the **Timeline View**.

To add an annotation:

1. Click **+** from the **Annotations** section of the toolbar:

   .. image:: ../images/add-annotation.png

   Annotations can also be added by right-clicking on the **Timeline View** and clicking the **Add Annotation** context menu option.

2. The **Annotation** modal displays. Provide a title and your note, then click **Save** to create the annotation.

   .. image:: ../images/save-annotation.png

   Once saved, the annotation displays on the timeline:

   .. image:: ../images/annotation-example.png

.. tip::

  - The annotation can be edited or deleted by clicking |pen|.
  - A complete list of annotations can be viewed in the **Annotations** tab of the **Advanced Details** section.

Create and use bookmarks
========================

The current view on the timeline (scroll and zoom position) can be saved to a bookmark for quick navigation.

To create and use a bookmark:

1. Click **+** on the |book| drop-down menu from the main **Toolbar**.
2. Select the bookmark number to navigate to the selected bookmark.
   
Or:

1.	Press **Ctrl** + any key from **0** ‒ **9** to create a view bookmark. The bookmark saves to an index based on the number used and creates a shortcut.
2.	Press any key from **0** ‒ **9** to restore the view to a stored bookmark from that index number shortcut. For example, if you save a bookmark using **Ctrl** + **1**, pressing **1** restores that bookmark.

Delete bookmarks
----------------

To delete a bookmark, click **X** to delete a bookmark from the |book| drop-down menu. Click **Reset View** to return the **Timeline View** to its original pan and zoom settings.

Change display settings
=======================

You can change these global display settings for ROCm-Optiq from the **Settings** menu:

- The application theme display (Light or Dark mode) or the font scaling: either automatic based on the display DPI, or customized using the font size control.

  .. image:: ../images/settings.png

- The time unit settings displayed on the **Timeline View**.

  .. image:: ../images/units.png

Customize Timeline display options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Customize display options for each track by clicking the gear icon in the track's **Description** in the **Timeline View**:

.. image:: ../images/track-gear.png

-	For Event tracks, you can change the coloring method used to color the events.
-	For sample Counter tracks, you can toggle between line plot and box plot mode. Set threshold limits so you can highlight which points are in a danger zone.

Set the flow rendering display mode
===================================

Use the **Flow** buttons on the **Toolbar** to show and hide flow information on the **Timeline View**, or change the flow display mode from Render (fan) to Chain mode. 

|flow|

- Chain mode displays events in a linked sequence, emphasizing dependencies and execution order. This is useful when analyzing how operations are chained together across queues or streams.

  .. image:: ../images/chain-mode.png

- Render mode Shows events in a fan-out style, highlighting parallelism and branching. This helps visualize concurrency and how multiple operations originate from a single source.

  .. image:: ../images/fan-mode.png

.. note::

  Chain mode and Render mode are visualization modes for relations; they don't represent the actual kernel scheduling flow.

