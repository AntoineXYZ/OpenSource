#ifndef AuLegacyCityConstructionTips_H
#define AuLegacyCityConstructionTips_H

#include <QtGlobal>

// Exact AuBuildButton text from Shores of Hazeron.exe (2016).
// Manual export: AuTabBuild constructor 0x2b9890, lines 327142..328938;
// common AuBuildButton constructor 0x2b8fb0, lines 326719..326970.
// Empty descriptions are intentional: 54 of the 82 original buttons have
// only their title followed by the common placement tips. Retain %1 in
// descriptions/tips until substituting AuGlobal::MiddleDotSymbol().
namespace AuLegacyCityConstructionTips
{
struct ButtonText
{
	quint8 industry, process; // process 0 is the original ordinary-building argument.
	const char* name;
	const char* description;
	int sourceLine;
};

static const char TitleFormat[] = "<b>%1</b>";
static const char DescriptionFormat[] = "<b>%1</b><br>%2<br>";

// Original non-button controls. Line numbers refer to the same executable
// manual export. Keep these separate from notices about deferred gameplay.
// GradingStyle: %1 is MiddleDotSymbol().
// RoadWidth: %1 is MiddleDotSymbol(); %2 spacecraft slab minimum (52m);
// %3 vehicle slab minimum (40m); %4 road minimum (8m); %5 maximum (80m).
// ListBuilding replaces DesignInitial with DesignProcedural for industries
// 1, 2, 10, 114 and 129; otherwise DesignOrdinary. Their %1 is IndustryName.
// Source line 329246.
static const char ClearLand[] = "<html><b>Clear Land</b><br>Clear land of all developments. All labor must be applied before clearing roads that are  'Under Construction'.<br>Enemy developments can be cleared if they are under construction and they have no completed levels.";

// Source line 328955.
static const char GradingStyle[] = "<html><b>Grading Style</b><br>Selects the type of grading for the next road or graded area to be drawn.<br>%1 <i>Road grading</i> slopes with the terrain when the ends are at different altitudes.<br>%1 <i>Slab grading</i> forces the altitude of the second end to match the starting end, so the ground is level.<br>%1 Slab altitude is determined by the altitude of the starting point.<br>%1 Slabs are needed by factories to place newly manufactured vehicles and spacecraft. <b>Minimum widths apply.</b><br>%1 Slabs are needed by airports to park spacecraft. <b>Minimum widths apply.</b><br>%1 Slabs are refuel and repair locations for vehicles and spacecraft.<br>%1 Slabs are landing pads for spacecraft, enabling them to buy and sell cargo.<br>%1 <i>Arrows</i> or the <i>mouse wheel</i> switch grading while drawing.</html>";

// Source line 329024.
static const char RoadWidth[] = "<html><b>Road Width</b><br>The width of the next road, slab, grade, irrigation or defoliant, in meters.<br>%1 Fractions are ok.<br>%1 Minimum width of a road is %4m.<br>%1 Minimum width of a vehicle spawn slab is %3m.<br>%1 Minimum width of a spacecraft spawn slab is %2m.<br>%1 Maximum width of a road or slab is %5m.</html>";

// Source line 329621.
static const char DesignInitial[] = "<html>Choose the building design to use for new construction. This choice has no effect when adding to an existing building.<br>The list contains building designs published in your empire plus building designs you have on disk.</html>";

// Source line 332343.
static const char DesignProcedural[] = "%1 does not construct a building. No building design can be chosen.";

// Source line 332494.
static const char DesignOrdinary[] = "<html>Choose the %1 design to use for new construction. This choice has no effect when adding to an existing %1.<br>The list contains %1 designs published in your empire plus %1 designs you have on disk.</html>";

// Source line 329571.
static const char MaterialsHeading[] = "Commodities that are needed to construct the building.\nSome buildings do not need any materials to construct.\nThe ToolTip on the picture below shows the amount needed.";

// Source line 329541.
static const char ProductsHeading[] = "Commodities that can be manufactured by the building.\nSome buildings do not manufacture anything.";

// Source line 329523.
static const char Terrain[] = "Terrain placement required for construction of the building.";

// Source line 329464.
static const char Jobs[] = "Number of jobs created by each building. When a building adds a\njob for each level, that includes the first level.";

// Source line 329500.
static const char Homes[] = "Number of homes created per completed building level.";

// Source line 329605.
static const char MakeProduct[] = "Enables the new building to begin making a product.";


static const ButtonText Buttons[] =
{
	{40, 0, "Bakery",
		"", 327142},
	{35, 0, "Dairy",
		"", 327157},
	{41, 0, "Brewery",
		"", 327171},
	{42, 0, "Distillery",
		"", 327186},
	{30, 0, "Farm",
		"", 327201},
	{43, 0, "Grist Mill",
		"", 327215},
	{45, 0, "Meat Processing Plant",
		"", 327230},
	{32, 0, "Orchard",
		"", 327246},
	{46, 0, "Sugar Mill",
		"", 327259},
	{34, 0, "Wharf",
		"Enables commercial fishing.<br>Connects a city with all other cities on the planet that have a wharf for import/export of commodities and migration of citizens.", 327274},
	{48, 0, "Winery",
		"", 327286},
	{22, 0, "Apartment",
		"", 327356},
	{21, 0, "Condominium",
		"", 327368},
	{20, 0, "House",
		"", 327381},
	{60, 0, "Coal Power Plant",
		"", 327435},
	{61, 0, "Gasoline Power Plant",
		"", 327450},
	{62, 0, "Hydroelectric Power Plant",
		"", 327465},
	{63, 0, "Hydrogen Power Plant",
		"", 327480},
	{64, 0, "Natural Gas Power Plant",
		"", 327496},
	{65, 0, "Nuclear Power Plant",
		"", 327509},
	{66, 0, "Solar Power Plant",
		"", 327522},
	{67, 0, "Wind Power Plant",
		"", 327534},
	{76, 0, "Explosives Factory",
		"", 327599},
	{31, 0, "Logging Camp",
		"", 327614},
	{44, 0, "Lumber Mill",
		"", 327629},
	{50, 0, "Mine",
		"", 327644},
	{83, 0, "Refinery",
		"", 327659},
	{84, 0, "Rocket Motor Factory",
		"", 327674},
	{86, 0, "Smelter",
		"", 327690},
	{47, 0, "Textile Mill",
		"", 327703},
	{89, 0, "Treatment Plant",
		"", 327716},
	{51, 0, "Well",
		"", 327728},
	{71, 0, "Armorer",
		"", 327796},
	{72, 0, "Carpenter",
		"", 327811},
	{73, 0, "Clothing Factory",
		"", 327826},
	{74, 0, "Computer Factory",
		"", 327841},
	{78, 0, "Jeweler",
		"", 327856},
	{75, 0, "Electronics Factory",
		"", 327871},
	{79, 0, "Machine Shop",
		"", 327885},
	{80, 0, "Munitions Factory",
		"", 327899},
	{81, 0, "Pharmaceutical Lab",
		"", 327911},
	{82, 0, "Plastics Factory",
		"", 327922},
	{77, 0, "Weapon Smith",
		"", 327935},
	{70, 0, "Aircraft Factory",
		"", 328006},
	{110, 0, "Airport Repair Shop",
		"Provides repair services to spacecraft.", 328020},
	{111, 0, "Airport Terminal",
		"Connects a city by air and space with all other cities in the solar system for import/export of commodities and migration of citizens.", 328036},
	{3, 0, "Design Studio",
		"Spacecraft designs and building designs are developed, published and purchased at a design studio.<br>Design studio sells blank disks and paper.", 328052},
	{85, 0, "Ship Yard",
		"", 328065},
	{87, 0, "Spacecraft Factory",
		"Spacecraft Factory manufactures spacecraft from designs created at a design studio. Take a design disk to the spacecraft factory to manufacture a design.", 328081},
	{126, 0, "Hangar",
		"Hangar stores spacecraft. Each player may store one spacecraft at every hanger. Any number of players may store spacecraft at the same hanger. All stored spacecraft are lost if the hanger is bulldozed or destroyed by combat. Spacecraft cannot be stored or retrieved from a hanger in a city that is enemy toward your empire.", 328094},
	{88, 0, "Vehicle Factory",
		"", 328105},
	{127, 0, "Observatory",
		"Provides information about the stars and planets.", 328118},
	{1, 0, "Town Square",
		"Establishes a city.<br>%1 Creates the primary flag of the city.<br>%1 Establishes a garrison troop contingent for the city of 5 troops. A city cannot garrison more than 1/7 of its population.<br>%1 Conscripts troops from the population to fill the garrison.<br>%1 Supplies unassigned troops to empire spacecraft and vehicles when they board troops.", 328192},
	{123, 0, "Capitol",
		"Enables control of all manufacturing and construction processes in a city from one location.<br>Enables a city to be declared the capital of an empire.<br>Use Governance window to declare a city to be a capital, after the Capitol building is finished.", 328217},
	{10, 0, "Military Flag",
		"Adds a military flag to the city.<br>%1 Adds a flag that must be captured before the town square flag can be captured.<br>%1 Increases the garrison troop contingent for the city by 5 troops. A city cannot garrison more than 1/7 of its population.<br>%1 Conscripts troops from the population to fill the garrison.<br>%1 Supplies unassigned troops to empire spacecraft and vehicles when they board troops.<br>%1 Supplies up to 2 troops to operate military sites.", 328240},
	{128, 0, "Military Guard Tower",
		"This military site attacks enemy troops, enemy vehicles, enemy spacecraft, and wild animals that get within range.<br>%1 Attack is omnidirectional, not limited by the geometry or facing of the building.<br>%1 Troop needed to operate. Takes one troop from a military flag.", 328273},
	{11, 0, "Military Weapon System",
		"This military site attacks enemy spacecraft that get within range.<br>%1 Attack is omnidirectional, not limited by the geometry or facing of the building.<br>%1 Troop needed to operate. Takes one troop from a military flag.", 328303},
	{12, 0, "Military Shield Generator",
		"This military site defends buildings against damage and blocks enemy transporter beams.<br>%1 Defense is omnidirectional, not limited by the geometry or facing of the building.<br>%1 Troop needed to operate. Takes one troop from a military flag.", 328334},
	{112, 0, "Arena",
		"", 328403},
	{100, 0, "Bank",
		"Stores taxes collected locally. Provides a place for citizens to collect rewards owed them by the empire. Citizens can also take out loans and make loan payments at a bank.", 328418},
	{101, 0, "Broker",
		"Imports all commodities. Each level increases the amount imported.", 328432},
	{102, 0, "Casino",
		"", 328445},
	{120, 0, "Church",
		"", 328458},
	{122, 0, "Fire Station",
		"", 328471},
	{103, 0, "Grocery",
		"Provides a place for citizens to buy commodities. Imports the commodities that it sells. Each level increases the amount imported.", 328487},
	{121, 0, "Hospital",
		"", 328500},
	{114, 0, "Park",
		"", 328513},
	{124, 0, "Police Station",
		"", 328526},
	{104, 0, "Retail Store",
		"Provides a place for citizens to buy commodities. Imports the commodities that it sells. Each level increases the amount imported.", 328542},
	{125, 0, "University",
		"", 328557},
	{105, 0, "Cantina",
		"Provides a place for citizens to buy commodities. Imports the commodities that it sells. Each level increases the amount imported.", 328570},
	{4, 0, "Warehouse",
		"Increases city storage capacity for commodities.", 328581},
	{113, 0, "Zoo",
		"", 328593},
	{2, 5, "Grade Area",
		"Graded areas change the shape of existing terrain.<br>%1 The presense of foliage is not affected.<br>%1 Grading does not block construction of roads or buildings.<br>%1 Grading does not interconnect cities like roads.<br>%1 Grading requires labor but no materials.", 328680},
	{2, 80, "Defoliate Area",
		"Defoliated areas remove trees, shrubs and ground cover that are present.<br>%1 Roads and buildings clear their site appropriately. It is not necessary to clear plants for them.<br>%1 Defoliation does not block construction of roads or buildings.<br>%1 Defoliation does not interconnect cities like roads.<br>%1 Defoliation requires labor but no materials.", 328713},
	{2, 81, "Clear Area",
		"Cleared areas remove trees and shrubs that are present. Any ground cover that is present is mowed.<br>%1 Roads and buildings clear their site appropriately. It is not necessary to clear plants for them.<br>%1 Clearing does not block construction of roads or buildings.<br>%1 Clearing does not interconnect cities like roads.<br>%1 Clearing requires labor but no materials.", 328747},
	{2, 74, "Irrigate Area",
		"Irrigated areas grow local trees, shrubs and ground cover. Ground cover is mowed.<br>%1 Irrigation does not consume water from city supplies.<br>%1 Irrigation does not block construction of roads or buildings.<br>%1 Irrigation does not interconnect cities like roads.<br>%1 Irrigation requires labor but no materials.", 328781},
	{2, 2, "Dirt Road",
		"Road expands a city and interconnects its buildings.<br>%1 Unpaved roads are easiest to construct, requiring the least labor and no materials.", 328815},
	{2, 3, "Asphalt Road",
		"Road expands a city and interconnects its buildings.<br>%1 Paved roads are lighted with street lights. %1 Street lights consume power when it is available; they are dim with no power.", 328848},
	{2, 4, "Concrete Road",
		"Road expands a city and interconnects its buildings.<br>%1 Paved roads are lighted with street lights. %1 Street lights consume power when it is available; they are dim with no power.", 328881},
	{2, 7, "Pressurized Road",
		"Road expands a city and interconnects its buildings.<br>%1 Paved roads are lighted with street lights. %1 Street lights consume power when it is available; they are dim with no power. %1 Pressurized roads are enclosed to provide a habitable atmosphere inside.", 328913},
	{129, 79, "Biodome",
		"Biodome covers an area with a dome containing a habitable atmosphere. Local plants grow inside biodomes.<br>Biodomes are designed by clicking the center location and then clicking a location at the radius. The radius location determines dome size and column location.", 328938},
};

struct PlacementText
{
	quint8 process; // 0 is the helper's default case.
	const char* text;
	int sourceLine;
};

static const PlacementText Placements[] =
{
	{2, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the road.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 <i>Arrows</i> or the <i>mouse wheel</i> switch grading style while drawing.<br>%1 Hold <i>Ctrl</i> to change an existing road to dirt construction.", 326817},
	{3, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the road.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 <i>Arrows</i> or the <i>mouse wheel</i> switch grading style while drawing.<br>%1 Hold <i>Ctrl</i> to change an existing road to asphalt construction.<br>", 326830},
	{4, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the road.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 <i>Arrows</i> or the <i>mouse wheel</i> switch grading style while drawing.<br>%1 Hold <i>Ctrl</i> to change an existing road to concrete construction.", 326843},
	{5, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the graded area.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 <i>Arrows</i> or the <i>mouse wheel</i> switch grading style while drawing.", 326856},
	{0, "<br><i>Placement Tips</i><br>%1 Radial line indicates front facing; it usually faces toward the road, toward the sea for coastal buildings.<br>%1 Outermost end of radial line determines placement altitude, where it touches the terrain.<br>%1 <i>+ (plus)</i>, <i>- (minus)</i>, or <i>mouse wheel</i> changes the number of levels to build.<br>%1 When cursor is red, <i>Space</i> or click aligns to the obstructing road or building.<br>%1 <i>Left/Right Arrows</i> rotates the building.<br>%1 Rotation angle step is set in <b>Constraint Angle</b> control.<br>%1 Hold <i>Shift</i> to rotate in 1 degree steps.<br>%1 <i>Up/Down Arrows</i> changes construction material.<br>%1 <i>* (asterisk)</i> switches <b>Make Product</b> on off.<br>%1 <i>Ctrl+Left/Right Arrows</i> changes manufacturing process.", 326869},
	{7, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the road.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 <i>Arrows</i> or the <i>mouse wheel</i> switch grading style while drawing.<br>%1 Hold <i>Ctrl</i> to change an existing road to pressurized construction with an enclosure.", 326882},
	{74, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the irrigated area.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 Does not change terrain grading.", 326895},
	{79, "<br><i>Placement Tips</i><br>%1 Biodome columns must not block any roads.<br>%1 Biodome edge must not cross any building sites.<br>%1 Biodome may not contain an <b>Airport Terminal</b>.<br>%1 <i>Left/Right Arrows</i> toggle column base between 1) altitude of location clicked or 2) altitude for 10m minimum column heights.<br>%1 Hold <i>Shift</i> while drawing the radius point, to constrain the angle of rotation.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.", 326908},
	{80, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the defoliated area.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 Does not change terrain grading.", 326921},
	{81, "<br><i>Placement Tips</i><br>%1 Hold <i>Shift</i> while drawing the second end, to constrain the angle of the cleared area.<br>%1 Constraint angle is set in <b>Constraint Angle</b> control.<br>%1 Does not change terrain grading.", 326934},
};

inline const ButtonText* FindButton(quint8 industry, quint8 process)
{
	for(const auto& button : Buttons)
		if(button.industry == industry && (!button.process || button.process == process)) return &button;
	return nullptr;
}

// Pass the original layout entry's process: 0 for ordinary buildings,
// a specific road process or 79 for a biodome.
inline const char* PlacementTips(quint8 process)
{
	const char* ordinary = "";
	for(const auto& placement : Placements)
	{
		if(placement.process == process) return placement.text;
		if(!placement.process) ordinary = placement.text;
	}
	return ordinary;
}
}

#endif
