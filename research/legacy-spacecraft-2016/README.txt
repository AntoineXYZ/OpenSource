2016 spacecraft designer recovery
=================================

Current implementation
----------------------
The modern client can open the legacy designer from the launch-screen chooser
or with -legacy-spacecraft [path/to/design.ship]. Both entry paths initialize
the normal client frame, game globals, resources and shared AuApp sound service.
The direct-launch path enters the designer within that frame. The solid view
extends AuDesignerView with the recovered original spacecraft geometry and atlas.
Both floor plan and walkthrough use its native sidebar renderer, button settings,
tooltips and window actions, including Spacecraft Design and Settings. In the normal client,
Legacy occupies the existing window; exit before choosing the other designer.
Launch-screen music stops during Legacy and resumes only if previously playing.

The 2D editor supports fresh/New designs, loading, inspection, cell painting
and erasing, equipment placement/rotation/brushes, equipment reconciliation
after room edits, and header controls.
The single blue information pane recalculates after room/equipment edits, armor,
streamline and minimum-TL changes, and New/Open. It reports hull/system volumes, equipment and
vehicle mass, berths/crew roles, passenger class, propulsion, power, capacitor,
fuel/cargo capacity and endurance, sensors, grouped bays/transporters, FTL
requirements, hit points and design warnings. TL is displayed one-based.
Calculations reuse compatible modern geometry/physics helpers while retaining
the original per-detail mass rounding, cubic-foot units and technology boost.
Weapon bays share room numbers across decks; transporters include deck in
their key. See SHIP-STATS-EVIDENCE.txt and ship-stats-report-evidence.txt.
Manufacturing-material TL, costs, armor/shield absorption and ammunition damage
remain unavailable; the panel explicitly distinguishes calculated systems TL
from complete manufacturing requirements. Reports never rewrite saved header
metadata or alter the design's dirty state. Native validation remains manual.
The pane follows the original report section order, uppercase headings, thin
rules, and 10-pixel indentation. Modified appears in yellow above classification.
New-design tips share this pane until there is a hull to report. Edit feedback
is appended below the report and is also available as the pane's tooltip.
Report updates preserve scrolling; New/Open return to the top. There are no tabs.
Rooms have fixed-size name/number labels at their first boundary vertex, using
the original label colors and black shadow; see PANEL-RECOVERY.txt.
The grid is the original finite 80x80-cell square with eight direction labels
and brighter ten-cell lines (CELL-SHAPES.txt). Fit includes its edges and labels.
The canvas has no visible scrollbars or top summary strip; middle drag pans.
The original icon panel docks on the right by default and toggles with the
Spacecraft Design sidebar button or the configured Design Parts binding (default F6).
Its title bar uses the original design-disk icon, both docked and floating.
There is no bottom status bar; edit feedback appears in the tools panel.
Deck arrows cover -41..41; new spacecraft rooms can be painted on -28..28.
See BUILDING-VS-SPACECRAFT.txt for the distinct mode and placement limits.
There is no deck dropdown. The footer
dropdown is minimum tech level. Room Number displays subtype+1 (1..256), not
the persistent room ID. Left click/drag paints; middle drag pans; wheel zooms;
F fits the view. There is no File/View menu bar; Escape temporarily exits the
designer through the normal discard-confirmation path. Unsupported original
tools remain disabled.

Save/Ctrl+S writes the legacy .ship format; see FORMAT.txt for byte-parity rules.
New/Open/Exit require Save/Discard/Cancel for a dirty design; failed loads
preserve it. Opening alone never rewrites the source file. Successful opens
remember LegacySpacecraft/LastOpenDirectory in the native user settings for
Hazeron Freehold / Shores of Hazeron, shared by both launch paths.

The original Render Mode button switches the floor plan to a first-person
walkthrough. It uses the game's Avatar and Scene bindings; with defaults,
WASD moves, the captured mouse looks, Shift runs, Q toggles continuous
forward movement, and Space jumps or brakes in zero gravity. C/Z moves up/down
in zero gravity, gravity wells or on a ladder. Outside the ship, thrust builds
momentum; Shift increases acceleration, releasing input coasts with drag,
and Space brakes. E sits at a nearby ordinary console/captain chair or leaves
the occupied console; otherwise it engages/releases a nearby ladder.
Attachment centers the avatar and faces the rungs once; free look
continues while W/S climbs according to head pitch (S reverses W), or C/Z climbs
directly. Shift does not boost ladder speed; see WALKTHROUGH-LADDER-INPUT.txt.
E or Space releases
the ladder. Home finds a clear standing position on the selected deck unless rebound.
The configured avatar/hand input toggle (default Return/right-click) and
temporary hand mode (default Ctrl)
provide access to doors and the editor interface. Capture releases on focus
loss, dialogs and leaving the scene, with no drag required for looking.
The existing Gravity button controls studio gravity outside the ship; interiors retain
their own gravity. Mode switching preserves the character pose and the
independent floor-plan view. Painting and erasing return to the floor plan.
Render mode uses the shared procedural skybox renderer and original texture
pixels retained in the modern assets. Its repeatable preview environment is
not the random temporary world from a particular 2016 session. Skybox graphics
settings apply. The backdrop shares the in-game galactic star generator and
uses a deep-space environment without a nearby nebula or artificial dimming.
Sky preparation starts in floor-plan mode and yields between sampling batches
and cube faces. Render mode can open while it finishes; completed skies are
cached across mode switches.
See WALKTHROUGH-SKY-LADDERS.txt for evidence and manual checks.
The orbit camera and cutaway control are removed. Armor changes the recovered
wall profiles and windows; Streamline Hull is a physics setting.

Walkthrough collision uses structural hull/interior surfaces and stairs, plus
the original machinery/game-table obstacle cells. Consoles, berths, surgery
units, transporter pads, chairs, vehicle previews and turret guns are passable.
The local standing-human capsule remains an approximation of the original
person path queries, as are ladder attachment and ship membership. The
default-human speeds, jump impulse, gravity values and door animation timing
are recovered. FOV follows g_Au.FieldOfViewY and the game's aspect-ratio limit;
mouse sensitivity, mouse-grab preference and hand cursor use game settings. The
short near plane is a local adaptation. A default male Gen3 human uses the
native DaPerson controller for input, stances, momentum, jumping and camera.
Legacy geometry supplies floor/gravity queries, collision and interaction attachment. The local body is hidden in every camera mode, matching
the 2016 DaPerson renderer. Shared Scene bindings and legacy Escape/Shift+Escape
switch top-down and first/third-person views. Console selection uses the original strict three-foot XY
range, avatar deck and detail order. Occupied consoles swivel with the player.
See WALKTHROUGH-FLIGHT-CONSOLES-BODY.txt for the recovered rules and checks.
Crouch (held) and prone/stand (toggle) use configured Avatar controls and
native pose camera heights/speeds. Taller stance changes require clearance.
Vehicle driving remains unimplemented. Help and transient messages are inside
the single blue information pane; the added inspection tool and Selection tab were removed.

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
for the legacy renderer. Doors/hatches use the original closed/half/open
textures at 100-ms intervals, with logical collision changing immediately.
Automatic small doors/hatches open on a blocked movement attempt. Ctrl+P opens
the nearest closed door/hatch; Ctrl+C closes the nearest open one. A click on
a nearby panel also toggles it. Manual operation has the recovered six-foot
range, deck rule and large-door stack behavior. Doors stay open until closed
in the designer. Character positions and door states are temporary and never
alter saved .ship bytes. Parked vehicles remain stationary. Logical markers
have no physical mesh in the original stationary renderer. Live console
readouts, firing, vehicle driving and contrails need world state.

The yellow crosshair reuses the modern client's HudItems artwork and drawing
layout. Looking at the ship acquires its name and selected subsystem's icon/HP
bar on a 200-ms lock cycle, with the original detail-change rule and distance
gate. Consoles use command/control health; machinery, pads and turrets use
their recovered system/group. The authoring ship is undamaged; the bar shows
remaining/maximum hits without numeric HP text. Sight eligibility follows the
original detail types; hand interaction and movement obstacles remain separate.
Intersections still use local mesh geometry rather than original sight proxies.
See WALKTHROUGH-SUBSYSTEM-TARGETS.txt for evidence and manual checks.

Door/hatch sound families and metal/studio-floor footstep loops use the game's
shared OpenAL context, sound archive loader and user sound settings. Machinery ambience follows
the recovered working-system conditions; the original designer initializes
power plant, sensors and drive off. See WALKTHROUGH-AUDIO.txt for the sound
mapping, original initial state and remaining console-control scope.

Undo/redo, whole-room/deck tools, move/copy, the path-checking tool and world
rendering remain unfinished.

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
WALKTHROUGH-RECOVERY.txt       Avatar, controls, camera and gravity evidence.
WALKTHROUGH-COLLISION.txt      Local controller and explicit approximations.
WALKTHROUGH-INPUT.txt          Captured mouse, hand mode and focus lifecycle.
WALKTHROUGH-AUDIO.txt          Original sound families, footsteps and system state.
WALKTHROUGH-FLIGHT-CONSOLES-BODY.txt Momentum, console seating and native Gen3 body.
DOOR-INTERACTION.txt           Animation, manual/automatic operation and stacks.
TESTING.txt                   Reference checks and manual native harnesses.
SHIP-STATS-EVIDENCE.txt       Room/equipment aggregation, crew and topology.
ship-stats-report-evidence.txt Original report sections, physics and limits.

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
src/lib/designer/legacy/aulegacyshiprender.h/.cpp First-person OpenGL view/input.
src/lib/designer/legacy/aulegacyshipwalk.h/.cpp   Local character collision/movement.
src/lib/designer/legacy/aulegacyshiphud.h/.cpp    Shared HUD artwork and target display.
src/lib/designer/legacy/aulegacyshipsound.h/.cpp  Walkthrough sources using shared game audio.
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
