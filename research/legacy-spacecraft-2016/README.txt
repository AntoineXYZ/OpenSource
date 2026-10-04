2016 spacecraft designer recovery
=================================

Current implementation
----------------------
The modern client can open the legacy designer from the launch-screen chooser
or with -legacy-spacecraft [path/to/design.ship]. The standalone path uses a
plain QApplication without a world session. The solid view uses a local OpenGL
widget and the bundled modern spacecraft atlas. In the normal client,
Legacy occupies the existing window; exit before choosing the other designer.
Launch-screen music stops during Legacy and resumes only if previously playing.

The 2D editor supports fresh/New designs, loading, inspection, cell painting
and erasing, equipment reconciliation after room edits, and header controls.
Rooms have fixed-size name/number labels at their first boundary vertex, using
the original label colors and black shadow; see PANEL-RECOVERY.txt.
The grid is the original finite 80x80-cell square with eight direction labels
and brighter ten-cell lines (CELL-SHAPES.txt). Fit includes its edges and labels.
The canvas has no visible scrollbars or top summary strip; middle drag pans.
The original icon panel docks on the right by default and toggles with F6.
Deck arrows cover -28..28 (57 decks); there is no deck dropdown. The footer
dropdown is minimum tech level. Room Number displays subtype+1 (1..256), not
the persistent room ID. Left click/drag paints; middle drag pans; wheel zooms;
F fits the view. There is no File/View menu bar; Escape temporarily exits the
designer through the normal discard-confirmation path. Unsupported original
tools remain disabled.

Edits are session-only. New/Open/Exit require Discard/Cancel for a dirty design;
failed loads preserve it. Original files remain read-only. Successful opens
remember LegacySpacecraft/LastOpenDirectory in the native user settings for
Hazeron Freehold / Shores of Hazeron, shared by both launch paths.

The original Render Mode button switches the floor plan to textured 3D.
Left drag orbits; middle/right/Shift-drag pans; wheel zooms; F or Home fits.
The separate cutaway checkbox clips at the selected deck's half-height;
deck arrows move the cut. Mode switching preserves the floor-plan view and
orbit camera. Painting and erasing return to the floor plan. Armor changes
the recovered wall profiles and windows; Streamline Hull is a physics setting.

The mesh uses the original quarter-grid occupancy rules, room/armor wall
profiles, interior wall materials, adjacent-deck floors/ceilings and transparent
window textures. This is a reconstruction from static evidence; full original
visual parity has not been validated. See SURFACE-RECOVERY-NOTES.txt for the
implemented surfaces and remaining geometry/detail limitations.

Saving, undo/redo, equipment placement, whole-room/deck tools, move/copy,
path checks, avatar navigation and world rendering remain unfinished.

Read by topic
-------------
FORMAT.txt                    File layout, field evidence, working-copy state.
CELL-SHAPES.txt                Grid masks, snapping, float precision and ties.
CONTROL-MAPPING.txt            Original mouse dispatch and room commands.
PAINTING-RULES.txt             Room identity, raster edits, quirks and guards.
EQUIPMENT-RECONCILIATION.txt    Placement rules and ordered detail side effects.
VEHICLE-BOUNDS.txt             Model footprints and missing-resource behavior.
PANEL-RECOVERY.txt             Exact icon layout, labels and header controls.
SURFACE-RECOVERY-NOTES.txt      Hull/interior implementation and remaining gaps.
RENDERING-MATERIAL-EVIDENCE.txt Original texture/material/surface predicates.
TESTING.txt                   Reference checks and manual native harnesses.

Evidence and tools
------------------
pseudocode/ keeps function bodies and necessary assembly, identified by module
and preferred-image address. DLLs share base 0x180000000; the EXE uses
0x140000000. Include the module when looking up an address. sources.json records
original binary identity and prior IDA provenance. Decompiled types, locals and
descriptive function names are reconstructions, not recovered original source.
Folded getters can have unrelated aliases. Read/write direction of exported
stream operators must be checked in their bodies, not inferred from the glyph.

cell-shapes.json and room-raster-fixtures.json are geometry fixtures;
cell-shape-constants.json retains original floating constants. detail-types.json,
vehicle-bounds.json and panel-icons.json hold recovered definitions and asset
provenance. sample-inspection.json records original file hashes and counts.
Keep these with their consumers; they are not disposable tool transcripts.

inspect_ship.py reads .ship files (--full includes complete records).
cell_shapes.py and room_raster.py are executable geometry references.
inspect_detail_types.py, inspect_vehicle_bounds.py, inspect_panel_icons.py and
inspect_panel_layout.py reproduce extraction from the original data/binaries.
The generated cell atlas and raw panel-layout dump need not be checked in.

Native source map (paths relative to repository root)
----------------------------------------------------
src/lib/data/designs/aulegacyship.h/.cpp         Records and atomic file reader.
src/lib/data/designs/aulegacyshipgeometry.h/.cpp Cell selection and raster port.
src/lib/data/designs/aulegacyshipedit.h/.cpp     Add/erase transactions.
src/lib/data/designs/aulegacyshipdetails.h/.cpp  Equipment reconciliation.
src/lib/designer/legacy/aulegacyshipviewer.h/.cpp Interface and session state.
src/lib/designer/legacy/aulegacyshipicons.h/.cpp Embedded original PNGs.
src/lib/designer/legacy/aulegacyshipmesh.h/.cpp Recovered CPU surface generator.
src/lib/designer/legacy/aulegacyshiprender.h/.cpp Textured OpenGL orbit view.
src/app/clients/scene/main.cpp                  Standalone launch branch.
src/lib/designer/core/audesignerframe.cpp       Chooser and embedded lifecycle.
.vscode/launch.json                            Already-built launch profiles.

Recovery status
---------------
The user verified original -spacecraft launch, all three original sample files,
and the native viewer, painting and fresh-design workflow. Python/static checks
support the recovered algorithms; native harnesses and exact original-client
before/after comparisons remain separate verification. See TESTING.txt.
The assistant must not initiate builds; the user builds and runs native checks.

This directory keeps current findings rather than milestone history. Duplicate
MCP transcripts, obsolete integration maps and generated presentation outputs
were removed after retaining unique evidence. Git retains the earlier reports.
