2016 spacecraft designer recovery
=================================

Current implementation
----------------------
The modern client can open the legacy designer from the launch-screen chooser
or with -legacy-spacecraft [path/to/design.ship]. The standalone path uses a
plain QApplication without a world session. The solid view uses a local OpenGL
widget and the recovered original spacecraft atlas. In the normal client,
Legacy occupies the existing window; exit before choosing the other designer.
Launch-screen music stops during Legacy and resumes only if previously playing.

The 2D editor supports fresh/New designs, loading, inspection, cell painting
and erasing, equipment placement/rotation/brushes, equipment reconciliation
after room edits, and header controls.
Rooms have fixed-size name/number labels at their first boundary vertex, using
the original label colors and black shadow; see PANEL-RECOVERY.txt.
The grid is the original finite 80x80-cell square with eight direction labels
and brighter ten-cell lines (CELL-SHAPES.txt). Fit includes its edges and labels.
The canvas has no visible scrollbars or top summary strip; middle drag pans.
The original icon panel docks on the right by default and toggles with F6.
Its title bar uses the original design-disk icon, both docked and floating.
There is no bottom status bar; edit feedback appears in the tools panel.
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

The original equipment palette includes stations, machinery, berths, doors,
hatches, openings, railings, all five stair shapes, turrets, a game table,
logical position markers and all six vehicle buttons. Placement uses the
recovered room restrictions, bridge exceptions, station limits, spacing and
vehicle footprints. Left/Right rotates the supported placement tools. Rounded
red outlines group related palette buttons without changing their slots.

Equipment renders with recovered procedural geometry or original 3DS assets.
The surviving model textures match the 2016 pixels. The original full-resolution
spacecraft atlas, machinery effect atlas and changed helm bitmap are embedded
for the legacy renderer. Parked vehicles remain stationary, and
doors/hatches are closed. Logical markers have no physical mesh in the original
stationary renderer. Live console readouts, firing, movement and contrails
need world state and remain outside this orbit preview.

Saving, undo/redo, whole-room/deck tools, move/copy,
path checks, avatar navigation and world rendering remain unfinished.

Read by topic
-------------
FORMAT.txt                    File layout, field evidence, working-copy state.
CELL-SHAPES.txt                Grid masks, snapping, float precision and ties.
CONTROL-MAPPING.txt            Original mouse dispatch and room commands.
PAINTING-RULES.txt             Room identity, raster edits, quirks and guards.
EQUIPMENT-RECONCILIATION.txt    Placement rules and ordered detail side effects.
EQUIPMENT-EDITING-EVIDENCE.txt  Placement/brush/erase commands and restrictions.
EQUIPMENT-PALETTE-EVIDENCE.txt  Equipment buttons, rotation and turret surfaces.
EQUIPMENT-RECOVERY-NOTES.txt    Procedural equipment geometry and effect mapping.
RESOURCE-EQUIPMENT-EVIDENCE.txt Models, original atlases and console textures.
STAIR-RENDERING-EVIDENCE.txt    All five stair shapes and their orientations.
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
inspect_equipment_assets.py compares model bytes and decoded texture pixels;
equipment-assets.json records the current equipment asset comparison.
The generated cell atlas and raw panel-layout dump need not be checked in.

Native source map (paths relative to repository root)
----------------------------------------------------
src/lib/data/designs/aulegacyship.h/.cpp         Records and atomic file reader.
src/lib/data/designs/aulegacyshipgeometry.h/.cpp Cell selection and raster port.
src/lib/data/designs/aulegacyshipedit.h/.cpp     Add/erase transactions.
src/lib/data/designs/aulegacyshipdetails.h/.cpp  Equipment editing/reconciliation.
src/lib/designer/legacy/aulegacyshipviewer.h/.cpp Interface and session state.
src/lib/designer/legacy/aulegacyshipicons.h/.cpp Embedded original PNGs.
src/lib/designer/legacy/aulegacyshipmesh.h/.cpp Recovered CPU surface generator.
src/lib/designer/legacy/aulegacyshiprender.h/.cpp Textured OpenGL orbit view.
src/lib/designer/legacy/aulegacyshipequipment.h/.cpp Procedural equipment.
src/lib/designer/legacy/aulegacyshipstairs.h/.cpp Original stair geometry.
src/lib/designer/legacy/aulegacyshipvehicles.h/.cpp Resource model importer.
src/lib/designer/legacy/aulegacyshipeffects.h     Embedded original effect atlas.
src/lib/designer/legacy/aulegacyshipatlas.h       Original full-resolution ship atlas.
src/lib/designer/legacy/aulegacyshiphelm.h        Original helm display bitmap.
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
