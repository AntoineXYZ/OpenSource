#include "catalog.h"
#include "../build/internal.h"
#include "autexturelib.h"

#include <algorithm>

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode {

namespace {

template <typename T>
const T* FindByKey(const QList<T>& list, const QString& key)
{
	auto it = std::find_if(list.cbegin(), list.cend(), [&key](const T& item) { return item.key == key; });
	return it != list.cend() ? &*it : nullptr;
}

}

const QList<RoomKind>& RoomKinds()
{
	static const QList<RoomKind> kinds = {
		{"hall", "Hall", true, "Skirting, a pilaster at every cell and a cornice."},
		{"stateroom", "Stateroom", true, "Skirting, a chair rail and a cornice."},
		{"crewcabin", "Crew Cabin", true, "Skirting and the theme's wall finish: log courses, stone blocks or metal ribs."},
		{"officercabin", "Officer Cabin", true, "Panelled lower walls, a chair rail and a stepped cornice."},
		{"lounge", "Lounge", true, "Panelled lower walls, a chair rail, a stepped cornice and ceiling beams."},
		{"galley", "Galley", true, "Tiled lower walls and a cornice."},
		{"hold", "Hold", true, "A tall kick plate, a post at every cell and ceiling beams."},
		{"barracks", "Barracks", true, "The theme's wall finish with two rails along it."},
		{"brig", "Brig", true, "Bars along every wall."},
		{"sickbay", "Sick Bay", true, "A bumper rail, skirting and a plain cornice."},
		{"engineroom", "Engine Room", true, "The wall finish, pipe runs, posts and ceiling beams."},
		{"vehiclebay", "Vehicle Bay", true, "A heavy kick plate, heavy pillars and ceiling beams."},
		{"balcony", "Balcony", true, "Open to the air: no walls or roof where it meets the outside, a railing along its edge "
			"instead. Walls and doors where it meets a room."},
		{"platform", "Platform", true, "Like a balcony, standing on legs down to the ground or to what is below. On the ground "
			"deck, a terrace with no railing."},
		{"catwalk", "Catwalk", true, "A narrow walkway with bar railings, open to the air: paint it a cell wide."},
		{"turbolift", "Turbo Lift", true, "A lift shaft: paint the same rectangle of whole cells on every deck it runs through. "
			"A door into it on a deck makes a stop there (one door a deck); the car rides between its stops."},
		{"gravitywell", "Gravity Well", false, "A column with no gravity for moving between decks. Not available yet."},
		{"opentobelow", "Open to Deck Below", true, "No floor: the room below rises up through this deck, its walls and wall "
			"design carried on up and no ceiling or floor left between. Paint it over whole cells of one room."},
		{"roofgable", "Roof, Gable", false, "Not a room here: roofs are painted over a deck's top (roof lines)."},
		{"roofhip", "Roof, Hip", false, "Not a room here: roofs are painted over a deck's top (roof lines)."},
		{"roofround", "Roof, Round", false, "Not a room here: roofs are painted over a deck's top (roof lines)."},
	};
	return kinds;
}

const RoomKind* FindRoomKind(const QString& key)
{
	return FindByKey(RoomKinds(), key);
}

QString DefaultRoomName(const QString& type, int tag)
{
	const RoomKind* kind = FindRoomKind(type);
	QString name = kind ? kind->name : type;
	return tag > 1 ? QString("%1 %2").arg(name).arg(tag) : name;
}

const QList<ThemeKind>& ThemeKinds()
{
	static const QList<ThemeKind> kinds = {
		{"wood", "Wood", "A log building: log courses, timber posts and beams, panelled walls, a stone foundation outside."},
		{"stone", "Stone", "A stone building: ashlar blocks, pilasters with bases and capitals, keystones over the doors, "
			"corner quoins and a cornice outside."},
		{"metal", "Metal", "A metal building: ribbed walls, plates, kick plates, I-beams, bulkhead door frames."},
		{"plain", "Plain", "No trims: flat walls, as before the themes."},
	};
	return kinds;
}

const QList<DoorStyle>& DoorStyles()
{
	static const QList<DoorStyle> styles = {
		{"swing-in-left", "Swings in, hinged left", "Swings into the door's room, hinged on the left as seen from it.", true, false},
		{"swing-in-right", "Swings in, hinged right", "Swings into the door's room, hinged on the right as seen from it.", true,
			false},
		{"swing-out-left", "Swings out, hinged left", "Swings away from the door's room (outward for a hull door), hinged on "
			"the left.", true, false},
		{"swing-out-right", "Swings out, hinged right", "Swings away from the door's room, hinged on the right.", true, false},
		{"double-swing-in", "Pair, swings in", "Two leaves hinged at either side, meeting in the middle, swinging into the door's "
			"room.", true, true},
		{"double-swing-out", "Pair, swings out", "Two leaves hinged at either side, swinging away from the door's room (outward "
			"for a hull door).", true, true},
		{"slide-left", "Slides left", "Slides into the wall on the left: out of sight when open. Needs plain wall beside it.",
			true, false},
		{"slide-right", "Slides right", "Slides into the wall on the right.", true, false},
		{"split-v", "Splits and slides apart", "Splits down the middle; the halves slide apart into the walls on either side.",
			true, true},
		{"slide-up", "Slides up", "Rolls up into the wall above the doorway, like a shutter.", true, true},
		{"slide-down", "Slides down", "Sinks away into the floor.", true, true},
		{"split-h", "Splits up and down", "The top half rises into the wall above, the bottom half sinks into the floor. On the "
			"ground deck only.", true, false},
		{"portcullis", "Portcullis", "A barred gate that rises out of the doorway. For outside doors with nothing built above.",
			true, false},
		{"vanish", "Vanishes", "Gone at once when open, with no frames in between.", true, true},
	};
	return styles;
}

const DoorStyle* FindDoorStyle(const QString& key)
{
	return FindByKey(DoorStyles(), key);
}

QString DefaultDoorStyle(const QString& theme, bool hullDoor, bool wide)
{
	if(theme == "plain")
		return "vanish";
	if(wide)
		return theme == "metal" ? (hullDoor ? "slide-up" : "split-v") : "double-swing-in";
	if(theme == "metal")
		return "split-v";
	if(theme == "stone" && hullDoor)
		return "portcullis";
	return "swing-in-left";
}

const QList<OpeningStyle>& OpeningStyles()
{
	static const QList<OpeningStyle> styles = {
		{"plain", "Plain", "A square opening as high as a door."},
		{"arch", "Arched", "A round arch springing at door height (a flatter arch when it is wide)."},
		{"tall", "Tall", "A square opening nearly to the ceiling."},
		{"full", "Full", "No wall at all along that side of the cell (or half diagonal): from floor to ceiling and corner to "
			"corner."},
	};
	return styles;
}

const OpeningStyle* FindOpeningStyle(const QString& key)
{
	return FindByKey(OpeningStyles(), key);
}

const QList<FixtureType>& FixtureTypes()
{
	static const QList<FixtureType> types = {
		{"berth", "Berth", {"crew", "captain", "officer", "troop", "passenger"},
			{"Crew", "Captain", "Officer", "Troop", "Passenger"}, "A bed, in a room cell: the game counts each as a bed."},
		{"parking", "Parking spot", {"ground", "water", "rocket", "space", "spacecraft"},
			{"Ground vehicle (6 x 12 m)", "Water vehicle (12 x 24 m)", "Space rocket (10 x 10 m)", "Space vehicle (12 x 17 m)",
			"Spacecraft (outside, on the ground)"},
			"Where a vehicle driven into the room parks: the whole spot must be in the room. A spacecraft's is outside."},
		{"light", "Light", {"nav", "beacon", "strobe"}, {"Navigation light", "Rotating beacon", "Strobe"},
			"A light on the building: on an outside wall, or on a flat roof."},
		{"lightpole", "Light pole", {"beacon", "nav", "strobe"}, {"Rotating beacon", "Navigation light", "Strobe"},
			"A pole on the ground outside with a light on top."},
		{"flagpole", "Flag pole", {"flag"}, {"Empire flag"}, "A pole on the ground outside flying the empire's flag."},
		{"gametable", "Game table", {"table"}, {"Game table"},
			"A card table the game draws, with four seats round it: it needs a 4 m square of floor in one room."},
		{"turret", "Turret", {"cannon", "twin", "laser", "missile"},
			{"Heavy gun (ballistic)", "Twin heavy gun (ballistic)", "Beam projector (energy)", "Missile launcher (launcher)"},
			"A battery against spacecraft, two cells by two (its cell and those east and south of it), on the roof of rooms with "
			"open sky over them or on the ground outside, its gun raised to the sky the way it faces. Someone at its turret gunner "
			"station aims and fires it; with nobody there, the building's crew fire it at enemies in the system. Six at most."},
	};
	return types;
}

const FixtureType* FindFixtureType(const QString& key)
{
	return FindByKey(FixtureTypes(), key);
}

const QStringList& LightColors()
{
	static const QStringList colors = {"red", "green", "white", "amber", "blue"};
	return colors;
}

const QList<StationKind>& StationKinds()
{
	static const QList<StationKind> kinds = {
		{"command", "Command", "The captain's station: whoever is at it commands the building.", {}, {}},
		{"firecontrol", "Fire Control", "Aims and fires the building's weapons.", {"energy", "ballistic", "launcher"},
			{"Energy weapon bay", "Ballistic weapon bay", "Weapon launcher bay"}},
		{"medic", "Medic", "Treats a patient beside it (in the cell next to it, unless placed).", {"surgery", "genetics",
			"cybernetics"}, {"Surgery", "Genetics", "Cybernetics"}},
		{"navigator", "Navigator", "Plots courses.", {}, {}},
		{"sensor", "Sensor", "Works the building's sensors.", {"sensor"}, {"Sensor module"}},
		{"shield", "Shield", "Works the building's shield.", {"energy", "kinetic", "cloaking"},
			{"Energy shield", "Kinetic shield", "Cloaking shield"}},
		{"transporter", "Transporter", "Sends people from its pads (the cell next to it, unless placed) and brings them back.",
			{"personnel", "vehicle"}, {"Personnel transporter", "Vehicle transporter"}},
		{"gunner", "Turret gunner", "Aims and fires a turret, looking through its gun sight: the next turret with no gunner, "
			"unless one is picked. Put it in the fire control room.", {}, {}},
	};
	return kinds;
}

const StationKind* FindStationKind(const QString& key)
{
	return FindByKey(StationKinds(), key);
}

const QList<RailingStyle>& RailingStyles()
{
	static const QList<RailingStyle> styles = {
		{"balusters", "Balusters", "A handrail on square balusters between newel posts."},
		{"balustrade", "Balustrade", "Stout balusters on a plinth under a heavy rail, with pedestals at the ends."},
		{"bars", "Iron bars", "Slender iron bars under a flat rail."},
		{"glass", "Glass", "Glass panels between slim posts, with a rail along the top."},
		{"half-wall", "Half wall", "A solid wall to waist height, with a cap."},
	};
	return styles;
}

const RailingStyle* FindRailingStyle(const QString& key)
{
	return FindByKey(RailingStyles(), key);
}

QString DefaultRailingStyle(const QString& theme)
{
	return theme == "stone" ? "balustrade" : theme == "metal" ? "bars" : theme == "plain" ? "glass" : "balusters";
}

const QList<WindowShape>& WindowShapes()
{
	static const QList<WindowShape> shapes = {
		{"slit", "Slit", "An arrow slit: 0.2 m wide, a metre tall.", 0.2, 1.0, 2.0},
		{"small", "Small", "A small window, 0.6 m square, at head height.", 0.6, 1.1, 1.7},
		{"standard", "Standard", "An ordinary window, a metre wide.", 1.0, 0.9, 2.0},
		{"tall", "Tall", "Tall and narrow, from knee height nearly to the ceiling.", 0.9, 0.4, 2.3},
		{"wide", "Wide", "A picture window, 2 m wide (less in a small cell).", 2.0, 0.9, 2.0},
		{"ribbon", "Ribbon", "A long strip high up, as wide as the cell allows.", 0, 1.5, 2.2},
		{"glass-wall", "Glass wall", "Nearly floor to ceiling, as wide as the cell allows.", 0, 0.15, 2.45},
	};
	return shapes;
}

const WindowShape* FindWindowShape(const QString& key)
{
	return FindByKey(WindowShapes(), key);
}

QStringList WindowTechNames()
{
	return {"Primitive: open, with shutters", "Medieval: leaded panes", "Industrial: frame and glazing bars",
		"Modern: slim frame, one pane", "Future: frameless, tinted, glowing edge"};
}

int DefaultWindowTech(const QString& theme)
{
	return theme == "stone" ? 2 : theme == "wood" ? 3 : 4;
}

const QList<HatchStyle>& HatchStyles()
{
	static const QList<HatchStyle> styles = {
		{"flap-up", "Trapdoor, opens up", "Hinged at one edge, it swings up into the room above and stands open."},
		{"flap-down", "Trapdoor, drops down", "Hinged at one edge, it drops open into the room below and hangs there."},
		{"double-flap", "Pair of flaps", "Two halves hinged at either side swing up, like cellar doors."},
		{"slide", "Slides into the floor", "Slides sideways into the floor between the decks: out of sight when open."},
		{"vanish", "Vanishes", "Gone at once when open, with no frames in between."},
	};
	return styles;
}

const QList<HatchStyle>& LadderStyles()
{
	static const QList<HatchStyle> styles = {
		{"rungs", "Rails and rungs", "A plain ladder, its rails reaching up through the hatch to hold on to."},
		{"rope", "Rope ladder", "Rope sides and wooden rungs, hanging from the edge of the hatch."},
		{"caged", "Caged ladder", "A ladder with a safety cage of hoops around the climber."},
		{"pole", "Pole and pegs", "A single pole with foot pegs to either side."},
	};
	return styles;
}

const HatchStyle* FindHatchStyle(const QString& key)
{
	return FindByKey(HatchStyles(), key);
}

const HatchStyle* FindLadderStyle(const QString& key)
{
	for(const HatchStyle& s : LadderStyles())
		if(s.key == key)
			return &s;
	return nullptr;
}

QString DefaultHatchStyle(const QString& theme)
{
	return theme == "metal" ? "slide" : theme == "plain" ? "vanish" : "flap-up";
}

QString DefaultLadderStyle(const QString& theme)
{
	return theme == "metal" ? "caged" : "rungs";
}

const QList<StairStyle>& StairStyles()
{
	static const QList<StairStyle> styles = {
		{"timber", "Timber", "Closed treads and risers between stringers, with balustered handrails."},
		{"stone", "Stone", "Solid stone steps built up from the floor, between low stone walls."},
		{"steel", "Steel", "Open plate treads on steel stringers, with pipe rails."},
		{"floating", "Floating", "Thick slabs on a single spine, between glass balustrades."},
		{"ramp", "Ramp", "A smooth ramp with kerbs and handrails, no steps."},
	};
	return styles;
}

const StairStyle* FindStairStyle(const QString& key)
{
	return FindByKey(StairStyles(), key);
}

QString DefaultStairStyle(const QString& theme)
{
	return theme == "stone" ? "stone" : theme == "metal" ? "steel" : theme == "plain" ? "floating" : "timber";
}

QStringList StairKindKeys()
{
	return {"straight", "turn-left", "turn-right", "u-left", "u-right"};
}

QStringList StairKindNames()
{
	return {"Straight", "Turning left", "Turning right", "U-turn left", "U-turn right"};
}

const QList<RoofStyle>& RoofStyles()
{
	static const QList<RoofStyle> styles = {
		{"gable", "Gable", "Two slopes meeting at a ridge, with gable-end walls closing the ends. The ridge runs the way you "
			"choose, across the whole roof.", true, false, false, 35},
		{"hip", "Hip", "Slopes on every side, meeting in hips and a ridge; where the roof turns a corner, a valley.", false, false,
			false, 30},
		{"shed", "Shed", "One slope, down to the side you choose, with a tall wall closing the high side.", false, true, false, 15},
		{"round", "Barrel", "A curved barrel vault over each run of cells, its axis the way you choose.", true, false, false, 0},
		{"gambrel", "Gambrel", "A barn roof: steep lower slopes and shallow upper ones, with gable ends.", true, false, false, 60},
		{"mansard", "Mansard", "Steep lower slopes on every side under a low hipped top.", false, false, false, 65},
		{"dome", "Dome", "A dome on a low drum over each cell.", false, false, true, 0},
		{"spire", "Spire", "A steep pyramid spire with a finial over each cell.", false, false, true, 70},
		{"flat", "Flat with parapet", "The flat top, behind a low parapet wall with a coping.", false, false, false, 0},
	};
	return styles;
}

const RoofStyle* FindRoofStyle(const QString& key)
{
	return FindByKey(RoofStyles(), key);
}

QString DefaultRoofStyle(const QString& theme)
{
	return theme == "stone" ? "hip" : theme == "metal" ? "round" : theme == "plain" ? "flat" : "gable";
}

const QList<DetailStyle>& DormerStyles()
{
	static const QList<DetailStyle> styles = {
		{"gable", "Gable", "A little gable over the window, its ridge running back into the roof."},
		{"shed", "Shed", "A roof sloping gently back into the roof over the window."},
		{"box", "Box", "A flat-topped box round the window."},
	};
	return styles;
}

const DetailStyle* FindDormerStyle(const QString& key)
{
	return FindByKey(DormerStyles(), key);
}

const QList<DetailStyle>& SkylightStyles()
{
	static const QList<DetailStyle> styles = {
		{"pane", "Pane", "Glass across the hole in a low curb, following the roof's slope (or level, on a flat top)."},
		{"lantern", "Lantern", "A glass pyramid on a low glazed base, standing on a flat top."},
	};
	return styles;
}

const DetailStyle* FindSkylightStyle(const QString& key)
{
	return FindByKey(SkylightStyles(), key);
}

const QList<TextureKind>& TextureKinds()
{
	static const QList<TextureKind> kinds = {
		{"material", "The building's material", -1, 2.5},
		{"wood-floor", "Wood floor tiles", TEXLIB_WoodFloorTile, 2},
		{"wood-planks", "Planks and beams", TEXLIB_WoodPlanksBeams, 2},
		{"wood-rustic", "Rustic wood tiles", TEXLIB_WoodTileRustic, 2},
		{"wood-siding", "Wood siding", TEXLIB_WoodSiding, 2},
		{"wood-siding-painted", "Painted wood siding", TEXLIB_WoodSidingPainted, 2},
		{"logs", "Logs", TEXLIB_Logs, 2},
		{"logs-rough", "Rough logs", TEXLIB_LogsRough, 2},
		{"plaster", "Plaster", TEXLIB_StuccoWallPlaster, 2},
		{"plaster-paint", "Painted plaster", TEXLIB_StuccoPaintPlaster, 2},
		{"drywall", "Drywall", TEXLIB_DryWall, 2},
		{"knockdown", "Knockdown plaster", TEXLIB_WallKnockdown, 2},
		{"popcorn", "Popcorn ceiling", TEXLIB_PopcornCeilingLight, 2},
		{"wood-bare", "Bare wood", TEXLIB_WoodBare, 2},
		{"wood-studs", "Wall studs", TEXLIB_WoodWallStuds, 2.4},
		{"osb", "Chipboard", TEXLIB_OSB, 2},
		{"shake-shingles", "Shake shingles", TEXLIB_ShakeShingles, 2},
		{"asphalt-shingles", "Asphalt shingles", TEXLIB_ShinglesAsphalt, 2},
		{"terracotta", "Terracotta tiles", TEXLIB_TerraCottaRoof, 2},
		{"stone-tiles", "Stone floor tiles", TEXLIB_StoneFloorTileSquares, 2},
		{"cobblestone", "Cobblestone", TEXLIB_CobblestoneLight, 2},
		{"marble-checks", "Chequered marble", TEXLIB_MarbleTileWhiteBlackSquares, 2},
		{"marble", "Marble", TEXLIB_Marble, 2},
		{"tan-marble", "Tan marble", TEXLIB_TanMarble, 2},
		{"alabaster", "Alabaster tiles", TEXLIB_AlabasterTile1, 2},
		{"alabaster-large", "Large alabaster tiles", TEXLIB_AlabasterTile2, 2},
		{"stone-wall", "Stone wall", TEXLIB_StoneWall, 2.5},
		{"brick", "Brick", TEXLIB_BrickWall, 2},
		{"brick-light", "Light brick", TEXLIB_BrickLight, 2},
		{"concrete", "Concrete", TEXLIB_Concrete, 2},
		{"prestressed", "Prestressed concrete", TEXLIB_ConcretePrestressed, 3},
		{"tarmac", "Tarmac", TEXLIB_Tarmac, 3},
		{"metal-floor", "Metal floor", TEXLIB_FloorMetal, 2},
		{"metal-tread", "Tread plate", TEXLIB_MetalTread, 1.5},
		{"plate-squares", "Metal plate squares", TEXLIB_MetalPlateSquares, 2},
		{"plate-box", "Metal plate", TEXLIB_MetalPlateBox, 2.5},
		{"plate-x", "Cross-braced plate", TEXLIB_MetalPlateX, 2.5},
		{"metal-wall", "Metal wall", TEXLIB_WallMetal, 2.5},
		{"pipes", "Pipes", TEXLIB_WallPipes, 2.5},
		{"blue-panel", "Blue and white panels", TEXLIB_BlueWhiteWallPanel, 2.5},
		{"red-panel", "Red and white panels", TEXLIB_RedWhiteWallPanel, 2.5},
		{"cargo-wall", "Cargo hold wall", TEXLIB_WallCargo, 2.5},
		{"metal-ceiling", "Metal ceiling", TEXLIB_CeilingMetal, 2},
		{"silver-ceiling", "Silver ceiling", TEXLIB_MetalCeilingSilver, 2},
		{"brushed", "Brushed metal", TEXLIB_MetalBrushed, 2},
		{"vent", "Vents", TEXLIB_VentWhite, 1.5},
	};
	return kinds;
}

const TextureKind* FindTextureKind(const QString& key)
{
	return FindByKey(TextureKinds(), key);
}

const QStringList& ThemePalette(const QString& theme)
{
	static const QMap<QString, QStringList> palettes = {
		{"wood", {"wood-floor", "material", "wood-planks", "wood-rustic", "wood-siding", "wood-siding-painted", "logs", "logs-rough",
			"plaster", "drywall", "wood-bare", "shake-shingles", "asphalt-shingles", "osb", "wood-studs"}},
		{"stone", {"stone-tiles", "material", "cobblestone", "marble-checks", "stone-wall", "brick", "brick-light", "plaster",
			"marble", "tan-marble", "plaster-paint", "terracotta", "concrete", "alabaster", "wood-planks"}},
		{"metal", {"metal-floor", "material", "metal-tread", "plate-squares", "metal-wall", "pipes", "blue-panel", "red-panel",
			"cargo-wall", "metal-ceiling", "silver-ceiling", "plate-box", "plate-x", "brushed", "vent"}},
		{"plain", {"wood-floor", "material", "concrete", "prestressed", "drywall", "knockdown", "plaster-paint", "plaster",
			"popcorn", "brick-light", "alabaster-large", "asphalt-shingles", "tarmac", "wood-siding-painted", "metal-tread"}},
	};
	return palettes.contains(theme) ? palettes.find(theme).value() : palettes.find("wood").value();
}

QString ThemeTexture(const QString& theme, const QString& surface)
{
	static const QMap<QString, QMap<QString, QString>> usual = {
		{"wood", {{"floor", "wood-floor"}, {"walls", "plaster"}, {"ceiling", "wood-bare"}, {"outside", "material"},
			{"roofing", "shake-shingles"}, {"top", "asphalt-shingles"}}},
		{"stone", {{"floor", "stone-tiles"}, {"walls", "plaster"}, {"ceiling", "plaster-paint"}, {"outside", "material"},
			{"roofing", "terracotta"}, {"top", "concrete"}}},
		{"metal", {{"floor", "metal-floor"}, {"walls", "metal-wall"}, {"ceiling", "metal-ceiling"}, {"outside", "material"},
			{"roofing", "plate-box"}, {"top", "plate-x"}}},
		{"plain", {{"floor", "wood-floor"}, {"walls", "drywall"}, {"ceiling", "popcorn"}, {"outside", "material"},
			{"roofing", "asphalt-shingles"}, {"top", "concrete"}}},
	};
	return usual.value(usual.contains(theme) ? theme : "wood").value(surface);
}

QStringList RoomTypeTextures(const QString& type, const QString& surface)
{
	static const QMap<QString, QStringList> rather = {
		{"hall/floor", {"stone-tiles", "marble-checks", "wood-floor", "metal-floor"}},
		{"lounge/floor", {"wood-floor", "marble", "plate-squares"}},
		{"lounge/walls", {"wood-siding-painted", "brick-light", "blue-panel", "plaster"}},
		{"officercabin/walls", {"wood-siding-painted", "marble", "blue-panel", "plaster-paint"}},
		{"galley/floor", {"stone-tiles", "marble-checks", "alabaster", "alabaster-large", "plate-squares"}},
		{"hold/floor", {"wood-planks", "cobblestone", "metal-tread", "concrete"}},
		{"hold/walls", {"wood-studs", "brick", "cargo-wall", "prestressed"}},
		{"hold/ceiling", {"wood-planks", "plaster-paint", "metal-ceiling", "concrete"}},
		{"vehiclebay/floor", {"cobblestone", "tarmac", "metal-tread", "concrete"}},
		{"vehiclebay/walls", {"osb", "brick", "cargo-wall", "prestressed"}},
		{"engineroom/floor", {"wood-planks", "cobblestone", "metal-tread", "concrete"}},
		{"engineroom/walls", {"wood-studs", "brick", "pipes", "brick-light"}},
		{"barracks/walls", {"logs", "stone-wall", "metal-wall", "knockdown"}},
		{"brig/floor", {"cobblestone", "plate-squares", "concrete"}},
		{"brig/walls", {"logs-rough", "stone-wall", "plate-box", "prestressed"}},
		{"sickbay/floor", {"alabaster", "alabaster-large", "marble-checks", "plate-squares"}},
		{"sickbay/walls", {"plaster-paint", "blue-panel", "drywall"}},
		{"balcony/floor", {"wood-planks", "cobblestone", "metal-tread", "concrete"}},
		{"platform/floor", {"wood-planks", "cobblestone", "metal-tread", "concrete"}},
		{"catwalk/floor", {"wood-planks", "cobblestone", "metal-tread", "concrete"}},
		{"turbolift/floor", {"wood-planks", "stone-tiles", "metal-tread", "concrete"}},
	};
	return rather.value(type + "/" + surface);
}

QStringList RoomDesignKinds()
{
	return {"hall", "stateroom", "crewcabin", "officercabin", "lounge", "galley", "hold", "barracks", "brig", "sickbay",
		"engineroom", "vehiclebay"};
}

const ThemeKind* FindThemeKind(const QString& key)
{
	return FindByKey(ThemeKinds(), key);
}

int MaterialTexlib(const QString& theme)
{
	return theme == "stone" ? TEXLIB_StoneWall : theme == "metal" ? TEXLIB_MetalPlateBox : theme == "plain" ? TEXLIB_Concrete
		: TEXLIB_WoodSiding;
}

}
