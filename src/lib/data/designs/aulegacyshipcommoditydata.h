#ifndef AuLegacyShipCommodityData_H
#define AuLegacyShipCommodityData_H

#include <QtGlobal>

// Recovered 2016 GD version 0x3a. SHA256: dac1d7b576a8405d55a85f9eb2723d757d0cce0ba7dc49aeb88f7be8e34f9106
// Base costs follow DgData::CalcCommodityBaseCosts; armor and weapons retain
// the original serialized fields, independent of the modern commodity database.
namespace AuLegacyShip { namespace LegacyCommodity {
struct Data
{
	const char* name;
	const char* bitmap;
	float baseCost;
	quint8 category;
	quint8 absorbKinetic, absorbEnergy, blockKinetic, blockEnergy;
	quint8 absorbContact, blockContact;
	quint16 damage;
	quint8 damageMod;
	quint16 radius;
};

constexpr Data Table[] = {
	{"", "", 0.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 0
	{"Flag", ":data/c_Flag.png", 13.8666658f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 1
	{"Computer", ":data/c_Computer.png", 568.400024f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 2
	{"Electricity", ":data/c_Electricity.png", 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 3
	{"Hammer", ":data/c_Hammer.png", 10.5f, 0, 0, 0, 0, 0, 0, 0, 11, 4, 0}, // 4
	{"Air", ":data/c_Air.png", 12.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 5
	{"Helicopter", ":data/c_Helicopter.png", 400.06665f, 0, 0, 0, 0, 0, 6, 4, 0, 0, 0}, // 6
	{"Space Rocket", ":data/c_SpaceRocket.png", 3444.53345f, 0, 0, 0, 0, 0, 12, 6, 0, 0, 0}, // 7
	{"Space Fighter F1", ":data/c_SpaceFighter.png", 1357.93335f, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0}, // 8
	{"Space Transport", ":data/c_CommercialAircraft.png", 2040.80005f, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0}, // 9
	{"Pistol Ammunition", ":data/c_PistolAmmo.png", 34.2999992f, 4, 0, 0, 0, 0, 0, 0, 31, 9, 0}, // 10
	{"Heavy Arms Ammunition Pi", ":data/c_HeavyArmsAmmo.png", 201.5f, 2, 0, 0, 0, 0, 0, 0, 71, 29, 10}, // 11
	{"Bomb Bl Black Cat", ":data/c_Bomb.png", 11048.0f, 0, 0, 0, 0, 0, 0, 0, 751, 249, 100}, // 12
	{"Plastic Explosive", ":data/c_PlasticExplosive.png", 108.0f, 0, 0, 0, 0, 0, 0, 0, 151, 49, 20}, // 13
	{"Animal Carcass", ":data/c_Animal.png", 35.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 14
	{"Beans", ":data/c_Beans.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 15
	{"Beer", ":data/c_Beer.png", 9.80000019f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 16
	{"Leather Body Armor", ":data/c_LeatherBodyArmor.png", 609.0f, 0, 0, 0, 0, 0, 4, 2, 0, 0, 0}, // 17
	{"Bread", ":data/c_Bread.png", 10.2250004f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 18
	{"Candy", ":data/c_Candy.png", 9.22222233f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 19
	{"Cheese", ":data/c_Cheese.png", 35.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 20
	{"Fabric Clothing", ":data/c_LinenClothes.png", 31.2333317f, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, // 21
	{"Coal", ":data/c_Coal.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 22
	{"Oil", ":data/c_CrudeOil.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 23
	{"Crystals", ":data/c_Crystals.png", 30.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 24
	{"Dynamite", ":data/c_Dynamite.png", 26.625f, 0, 0, 0, 0, 0, 0, 0, 151, 49, 20}, // 25
	{"Eggs", ":data/c_Eggs.png", 35.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 26
	{"Electronic Part", ":data/c_ElectronicPart.png", 54.7999992f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 27
	{"Farm Machinery", ":data/c_FarmMachinery.png", 1818.40002f, 0, 0, 0, 0, 0, 24, 12, 0, 0, 0}, // 28
	{"Fertilizer", ":data/c_Fertilizer.png", 35.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 29
	{"Plant Fiber", ":data/c_Fiber.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 30
	{"Pistol", ":data/c_Pistol.png", 200.0f, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 31
	{"Rifle", ":data/c_Rifle.png", 300.0f, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 32
	{"Fish", ":data/c_Fish.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 33
	{"Flour", ":data/c_Flour.png", 8.25f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 34
	{"Fruit", ":data/c_Fruit.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 35
	{"Gasoline", ":data/c_Gasoline.png", 24.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 36
	{"Gems", ":data/c_Gems.png", 100.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 37
	{"Grain", ":data/c_Grain.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 38
	{"Grapes", ":data/c_Grapes.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 39
	{"Gun Powder", ":data/c_GunPowder.png", 96.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 40
	{"Woven Helmet", ":data/c_WovenHelmet.png", 77.4666595f, 0, 0, 0, 0, 0, 4, 2, 0, 0, 0}, // 41
	{"Herbs", ":data/c_Herbs.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 42
	{"Hops", ":data/c_Hops.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 43
	{"Hydrogen", ":data/c_Hydrogen.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 44
	{"Ice", ":data/c_Ice.png", 5.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 45
	{"Crystal Necklace", ":data/c_CrystalJewelry.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 46
	{"Leather", ":data/c_Leather.png", 47.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 47
	{"Lumber", ":data/c_Lumber.png", 8.5f, 0, 0, 0, 0, 0, 20, 10, 0, 0, 0}, // 48
	{"Animal Meat", ":data/c_AnimalMeat.png", 40.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 49
	{"Mechanical Part", ":data/c_MechanicalPart.png", 36.5999985f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 50
	{"Metal", ":data/c_Metal.png", 20.0f, 0, 0, 0, 0, 0, 50, 25, 0, 0, 0}, // 51
	{"Milk", ":data/c_Milk.png", 35.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 52
	{"Minerals", ":data/c_Minerals.png", 20.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 53
	{"Mine Machinery", ":data/c_MineMachinery.png", 1935.59998f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 54
	{"Small Missile Bl Zipper", ":data/c_MissileSm.png", 1959.40002f, 7, 0, 0, 0, 0, 0, 0, 126, 24, 15}, // 55
	{"Large Missile Bl Thumper", ":data/c_MissileLg.png", 16223.2002f, 3, 0, 0, 0, 0, 0, 0, 701, 199, 90}, // 56
	{"Natural Gas", ":data/c_NaturalGas.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 57
	{"Nuts", ":data/c_Nuts.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 58
	{"Ore", ":data/c_Ore.png", 15.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 59
	{"Pastry", ":data/c_Pastry.png", 13.794445f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 60
	{"Petrochemicals", ":data/c_Petrochemicals.png", 49.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 61
	{"Pharmaceuticals", ":data/c_Pharmaceuticals.png", 30.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 62
	{"Plastic", ":data/c_Plastic.png", 26.6000004f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 63
	{"Large Rocket Motor", ":data/c_RocketMotorLg.png", 2860.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 64
	{"Metal Power Armor", ":data/c_PowerArmor.png", 5740.57324f, 0, 0, 0, 0, 0, 20, 10, 0, 0, 0}, // 65
	{"Radioactives", ":data/c_Radioactives.png", 40.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 66
	{"Small Rocket Bl Partypopper", ":data/c_RocketSm.png", 1423.0f, 7, 0, 0, 0, 0, 0, 0, 151, 49, 20}, // 67
	{"Large Rocket Bl Thud", ":data/c_RocketLg.png", 14658.0f, 3, 0, 0, 0, 0, 0, 0, 751, 249, 100}, // 68
	{"Small Rocket Motor", ":data/c_RocketMotorSm.png", 840.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 69
	{"Commercial Ship", ":data/c_CommercialShip.png", 2313.43335f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 70
	{"Naval Ship", ":data/c_NavalShip.png", 3111.5332f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 71
	{"Environment Suit Helmet", ":data/c_VacSuitHelmet.png", 417.666656f, 0, 0, 0, 0, 0, 4, 1, 0, 0, 0}, // 72
	{"Medical Kit", ":data/c_MedKit.png", 64.7333298f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 73
	{"Spices", ":data/c_Spices.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 74
	{"Rifle Ammunition", ":data/c_RifleAmmo.png", 25.583334f, 5, 0, 0, 0, 0, 0, 0, 46, 14, 0}, // 75
	{"Stone", ":data/c_Stone.png", 10.0f, 0, 0, 0, 0, 0, 40, 20, 0, 0, 0}, // 76
	{"Submarine", ":data/c_Submarine.png", 1207.1333f, 0, 0, 0, 0, 0, 20, 10, 0, 0, 0}, // 77
	{"Sugar", ":data/c_Sugar.png", 23.666666f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 78
	{"Textiles", ":data/c_Textiles.png", 22.7333317f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 79
	{"Environment Suit", ":data/c_VacSuit.png", 1926.53345f, 0, 0, 0, 0, 0, 6, 3, 0, 0, 0}, // 80
	{"Vegetable", ":data/c_Vegetable.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 81
	{"SUV", ":data/c_Suv.png", 663.266663f, 0, 0, 0, 0, 0, 14, 8, 0, 0, 0}, // 82
	{"Water", ":data/c_Water.png", 18.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 83
	{"Whiskey", ":data/c_Whiskey.png", 74.6666641f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 84
	{"Wine", ":data/c_Wine.png", 72.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 85
	{"Log", ":data/c_Wood.png", 12.0f, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0}, // 86
	{"Atmosphere Density", ":data/c_Atmosphere.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 87
	{"Water in the Environment", ":data/c_Hydrographics.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 88
	{"Vegetation Density", ":data/c_Vegetation.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 89
	{"Metal Body Armor", ":data/c_MetalBodyArmor.png", 400.0f, 0, 0, 0, 0, 0, 16, 10, 0, 0, 0}, // 90
	{"Plastic Body Armor", ":data/c_PlasticBodyArmor.png", 416.0f, 0, 0, 0, 0, 0, 12, 7, 0, 0, 0}, // 91
	{"Woven Body Armor", ":data/c_WovenBodyArmor.png", 277.333313f, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0}, // 92
	{"Nitrate Explosive", ":data/c_NitrateExplosive.png", 158.0f, 0, 0, 0, 0, 0, 0, 0, 151, 49, 20}, // 93
	{"Sunlight", ":data/c_Sunlight.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 94
	{"Leather Clothing", ":data/c_LeatherClothes.png", 33.5f, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0}, // 95
	{"Leather Helmet", ":data/c_LeatherHelmet.png", 136.0f, 0, 0, 0, 0, 0, 2, 1, 0, 0, 0}, // 96
	{"Plastic Helmet", ":data/c_PlasticHelmet.png", 105.199997f, 0, 0, 0, 0, 0, 6, 3, 0, 0, 0}, // 97
	{"Metal Bracelet", ":data/c_MetalJewelry.png", 5.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 98
	{"Gem Ring", ":data/c_GemJewelry.png", 150.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 99
	{"Fish Meat", ":data/c_FishMeat.png", 28.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 100
	{"Vodka", ":data/c_Vodka.png", 70.6666641f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 101
	{"Heavy Weapon", ":data/c_HeavyWeapon.png", 161.600006f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 102
	{"Tank", ":data/c_Tank.png", 5634.6001f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 103
	{"APC", ":data/c_APC.png", 3055.0f, 0, 0, 0, 0, 0, 60, 30, 0, 0, 0}, // 104
	{"Metal Helmet", ":data/c_MetalHelmet.png", 102.0f, 0, 0, 0, 0, 0, 8, 5, 0, 0, 0}, // 105
	{"Metal Power Armor Helmet", ":data/c_PowerArmorHelmet.png", 1657.8667f, 0, 0, 0, 0, 0, 10, 5, 0, 0, 0}, // 106
	{"Crate", ":data/c_Crate.png", 15.8000002f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 107
	{"Lantern", ":data/c_Lantern.png", 29.7999992f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 108
	{"Knapsack", ":data/c_Knapsack.png", 27.7333317f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 109
	{"Backpack", ":data/c_Backpack.png", 55.4666634f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 110
	{"Lumenite", ":data/c_Lumenite.png", 120.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 111
	{"Blank Disk", ":data/c_BlankDisk.png", 1.58000004f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 112
	{"Ship Design Disk", ":data/c_DesignDisk.png", 1.58000004f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 113
	{"Shovel", ":data/c_Shovel.png", 18.0f, 0, 0, 0, 0, 0, 0, 0, 11, 4, 0}, // 114
	{"Gin", ":data/c_Gin.png", 97.6666641f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 115
	{"Knife", ":data/c_Knife.png", 64.0f, 0, 0, 0, 0, 0, 0, 0, 16, 4, 0}, // 116
	{"Shotgun", ":data/c_Shotgun.png", 250.0f, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 117
	{"Shotgun Ammunition", ":data/c_ShotgunAmmo.png", 18.1875f, 6, 0, 0, 0, 0, 0, 0, 21, 9, 0}, // 118
	{"Antidote", ":data/c_Antidote.png", 40.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 119
	{"Antitoxin", ":data/c_Antitoxin.png", 114.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 120
	{"Rum", ":data/c_Rum.png", 95.3333282f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 121
	{"Tequila", ":data/c_Tequila.png", 126.666664f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 122
	{"Fitting Wrench", ":data/c_FittingWrench.png", 13.8666658f, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0}, // 123
	{"Glue Gun", ":data/c_GlueGun.png", 19.8666668f, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0}, // 124
	{"Screw Driver", ":data/c_ScrewDriver.png", 8.31999969f, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0}, // 125
	{"Sewing Needle", ":data/c_SewingNeedle.png", 6.0f, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0}, // 126
	{"Soldering Iron", ":data/c_SolderIron.png", 29.7999992f, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0}, // 127
	{"Tongs", ":data/c_Tongs.png", 41.5999985f, 0, 0, 0, 0, 0, 0, 0, 9, 1, 0}, // 128
	{"Wrench", ":data/c_Wrench.png", 10.3999996f, 0, 0, 0, 0, 0, 0, 0, 9, 1, 0}, // 129
	{"Hay", ":data/c_Hay.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 130
	{"Torch", ":data/c_Torch.png", 12.1999998f, 0, 0, 0, 0, 0, 0, 0, 7, 1, 0}, // 131
	{"Sailboat", ":data/c_SailBoat.png", 464.399994f, 0, 0, 0, 0, 0, 4, 3, 0, 0, 0}, // 132
	{"Motor Bike", ":data/c_MotorBike.png", 273.799988f, 0, 0, 0, 0, 0, 10, 5, 0, 0, 0}, // 133
	{"Eludium", ":data/c_Eludium.png", 110.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 134
	{"Pliers", ":data/c_Pliers.png", 20.7999992f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 135
	{"Torque Wrench", ":data/c_TorqueWrench.png", 45.0f, 0, 0, 0, 0, 0, 0, 0, 9, 1, 0}, // 136
	{"Vulcanite", ":data/c_Vulcanite.png", 535.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 137
	{"Ioplasma", ":data/c_Ioplasma.png", 200.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 138
	{"Magmex", ":data/c_Magmex.png", 80.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 139
	{"Phlogiston", ":data/c_Phlogiston.png", 200.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 140
	{"Bolite", ":data/c_Bolite.png", 200.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 141
	{"Cryozine", ":data/c_Cryozine.png", 200.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 142
	{"Myrathane", ":data/c_Myrathane.png", 200.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 143
	{"Polytaride", ":data/c_Polytaride.png", 200.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 144
	{"Vulcium", ":data/c_Vulcium.png", 2000.0f, 0, 0, 0, 0, 0, 70, 35, 0, 0, 0}, // 145
	{"Magmium", ":data/c_Magmium.png", 200.0f, 0, 0, 0, 0, 0, 60, 30, 0, 0, 0}, // 146
	{"Lava in the Environment", ":data/c_Magma.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 147
	{"Supercooled Liquid in the Environment", ":data/c_SupercooledLiquid.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 148
	{"Inferno Atmosphere Density", ":data/c_InfernoAtmosphere.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 149
	{"Frigid Atmosphere Density", ":data/c_FrigidAtmosphere.png", 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 150
	{"Magmium Knife", ":data/c_MagmiumKnife.png", 504.799988f, 0, 0, 0, 0, 0, 0, 0, 21, 4, 0}, // 151
	{"Vulcium Knife", ":data/c_VulciumKnife.png", 5835.6001f, 0, 0, 0, 0, 0, 0, 0, 26, 4, 0}, // 152
	{"Magmium Power Armor", ":data/c_MagmiumPowerArmor.png", 17425.4531f, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0}, // 153
	{"Magmium Power Armor Helmet", ":data/c_MagmiumPowerArmorHelmet.png", 4946.2666f, 0, 0, 0, 0, 0, 14, 7, 0, 0, 0}, // 154
	{"Vulcium Power Armor", ":data/c_VulciumPowerArmor.png", 157692.766f, 0, 0, 0, 0, 0, 60, 30, 0, 0, 0}, // 155
	{"Vulcium Power Armor Helmet", ":data/c_VulciumPowerArmorHelmet.png", 43139.8672f, 0, 0, 0, 0, 0, 20, 10, 0, 0, 0}, // 156
	{"Cronodollars", ":data/c_Money.png", 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 157
	{"Aircraft Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 158
	{"Military Guard Tower Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 159
	{"Airport Repair Shop Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 160
	{"Airport Terminal Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 161
	{"Apartment Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 162
	{"Arena Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 163
	{"Armorer Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 164
	{"Bakery Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 165
	{"Bank Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 166
	{"Brewery Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 167
	{"Broker Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 168
	{"Capitol Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 169
	{"Carpenter Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 170
	{"Casino Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 171
	{"Church Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 172
	{"Clothing Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 173
	{"Coal Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 174
	{"Military Shield Generator Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 175
	{"Computer Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 176
	{"Design Studio Technology", ":data/c_TechLevel.png", 2000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 177
	{"Distillery Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 178
	{"Electronics Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 179
	{"Explosives Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 180
	{"Farm Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 181
	{"Fire Station Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 182
	{"Gasoline Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 183
	{"Grist Mill Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 184
	{"Grocery Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 185
	{"Military Weapon System Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 186
	{"Hospital Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 187
	{"Condominium Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 188
	{"House Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 189
	{"Hydroelectric Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 190
	{"Hydrogen Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 191
	{"Jeweler Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 192
	{"Logging Camp Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 193
	{"Lumber Mill Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 194
	{"Machine Shop Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 195
	{"Meat Processing Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 196
	{"Mine Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 197
	{"Munitions Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 198
	{"Natural Gas Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 199
	{"Nuclear Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 200
	{"Orchard Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 201
	{"Park Technology", ":data/c_TechLevel.png", 2000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 202
	{"Pharmaceutical Lab Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 203
	{"Plastics Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 204
	{"Police Station Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 205
	{"Refinery Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 206
	{"Retail Store Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 207
	{"Rocket Motor Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 208
	{"University Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 209
	{"Ship Yard Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 210
	{"Smelter Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 211
	{"Solar Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 212
	{"Spacecraft Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 213
	{"Sugar Mill Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 214
	{"Cantina Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 215
	{"Textile Mill Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 216
	{"Treatment Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 217
	{"Vehicle Factory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 218
	{"Warehouse Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 219
	{"Weapon Smith Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 220
	{"Well Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 221
	{"Wharf Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 222
	{"Wind Power Plant Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 223
	{"Winery Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 224
	{"Zoo Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 225
	{"Grav Coupling", ":data/c_GravCoupling.png", 269.600006f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 226
	{"Cryo Heat Sink", ":data/c_CryoHeatSink.png", 573.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 227
	{"Io Tube", ":data/c_IoTube.png", 382.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 228
	{"Clamp", ":data/c_Clamp.png", 39.5999985f, 0, 0, 0, 0, 0, 0, 0, 9, 1, 0}, // 229
	{"Oscilloscope", ":data/c_OScope.png", 163.399994f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 230
	{"Space Fighter F2", ":data/c_SpaceFighter2.png", 2373.8667f, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0}, // 231
	{"Mission Disk", ":data/c_MissionDisk.png", 1.58000004f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 232
	{"Bone", ":data/c_Bone.png", 40.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 233
	{"Command And Control Module", ":data/c_CommandAndControlModule.png", 7878.71973f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 234
	{"Gravity Drive Module", ":data/c_DriveModule.png", 106156.359f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 235
	{"Rocket Drive Module", ":data/c_DriveModule.png", 616451.438f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 236
	{"Metal Hull Module", ":data/c_HullModule.png", 165674.562f, 0, 0, 0, 0, 0, 20, 10, 0, 0, 0}, // 237
	{"Life Support Module", ":data/c_LifeSupportModule.png", 6111.1001f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 238
	{"Power Plant Module", ":data/c_PowerPlantModule.png", 36974.5664f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 239
	{"Energy Shield Module", ":data/c_ShieldModule.png", 114487.039f, 0, 0, 2, 0, 0, 10, 5, 0, 0, 0}, // 240
	{"Kinetic Shield Module", ":data/c_ShieldModule.png", 395415.562f, 0, 2, 0, 0, 0, 10, 5, 0, 0, 0}, // 241
	{"Cloaking Shield Module", ":data/c_ShieldModule.png", 2043546.62f, 0, 0, 0, 0, 0, 10, 5, 0, 0, 0}, // 242
	{"Surgery Medical Module", ":data/c_MedicalModule.png", 4470.08057f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 243
	{"Genetics Medical Module", ":data/c_MedicalModule.png", 96456.1406f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 244
	{"Cybernetics Medical Module", ":data/c_MedicalModule.png", 55297.3086f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 245
	{"Personnel Transporter Module", ":data/c_TransporterModule.png", 9669.90723f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 246
	{"Vehicle Transporter Module", ":data/c_TransporterModule.png", 258590.938f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 247
	{"Energy Weapon Turret Module", ":data/c_TurretModule.png", 10225.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 248
	{"Beam Laser Turret Module", ":data/c_TurretModule.png", 16523.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 249
	{"Weapon Launcher Turret Module", ":data/c_TurretModule.png", 3886.90015f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 250
	{"Missile Launcher Turret Module", ":data/c_TurretModule.png", 4444.90039f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 251
	{"Energy Weapon Bay Module", ":data/c_BayModule.png", 178965.391f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 252
	{"Pulse Laser Bay Module", ":data/c_BayModule.png", 97151.3828f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 253
	{"Weapon Launcher Bay Module", ":data/c_BayModule.png", 19138.7695f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 254
	{"Missile Launcher Bay Module", ":data/c_BayModule.png", 25266.7695f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 255
	{"Atmosphere Condenser Bay Module", ":data/c_BayModule.png", 34199.0469f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 256
	{"Prospector Bay Module", ":data/c_BayModule.png", 25266.7695f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 257
	{"Tractor Beam Bay Module", ":data/c_BayModule.png", 136243.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 258
	{"Wormhole Drive FTL Module", ":data/c_FTLModule.png", 53405.7695f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 259
	{"Warp Drive FTL Module", ":data/c_FTLModule.png", 464099.844f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 260
	{"Ballistic Weapon Bay Module", ":data/c_BayModule.png", 37065.2695f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 261
	{"Ballistic Weapon Turret Module", ":data/c_TurretModule.png", 3886.90015f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 262
	{"Sensor Module", ":data/c_SensorModule.png", 25705.0723f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 263
	{"Magmium Heat Sink", ":data/c_MagmiumHeatSink.png", 1193.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 264
	{"Vulcium Heat Sink", ":data/c_VulciumHeatSink.png", 6823.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 265
	{"Myrasplicer", ":data/c_Myrasplicer.png", 1022.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 266
	{"Microscope", ":data/c_Microscope.png", 235.400009f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 267
	{"Gigacell", ":data/c_Gigacell.png", 378.23999f, 1, 0, 0, 0, 0, 0, 0, 61, 19, 0}, // 268
	{"Tesloid Dissipator", ":data/c_TesloidDissipator.png", 2826.62012f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 269
	{"Nanopath Calibrator", ":data/c_NanopathCalibrator.png", 1021.62f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 270
	{"Antiflux Particles", ":data/c_AntifluxParticles.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 271
	{"Borexino Precipitate", ":data/c_BorexinoPrecipitate.png", 2000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 272
	{"Type O Preons", ":data/c_TypeOPreons.png", 3000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 273
	{"Type B Preons", ":data/c_TypeBPreons.png", 4000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 274
	{"Type A Preons", ":data/c_TypeAPreons.png", 5000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 275
	{"Type F Preons", ":data/c_TypeFPreons.png", 6000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 276
	{"Type G Preons", ":data/c_TypeGPreons.png", 7000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 277
	{"Type K Preons", ":data/c_TypeKPreons.png", 8000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 278
	{"Type M Preons", ":data/c_TypeMPreons.png", 9000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 279
	{"Magmium Hull Module", ":data/c_HullModule.png", 1066074.62f, 0, 0, 0, 0, 0, 30, 15, 0, 0, 0}, // 280
	{"Vulcium Hull Module", ":data/c_HullModule.png", 10066574.0f, 0, 0, 0, 0, 0, 60, 30, 0, 0, 0}, // 281
	{"Paper", ":data/c_Paper.png", 2.1500001f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 282
	{"Particle Collector Bay Module", ":data/c_BayModule.png", 1010701.38f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 283
	{"DNA Sample Kit", ":data/c_DNASampleKit.png", 3.57333302f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 284
	{"DNA Sample", ":data/c_DNASample.png", 3.57333302f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 285
	{"Observatory Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 286
	{"Building Design Disk", ":data/c_BuildingDesignDisk.png", 1.58000004f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 287
	{"Building Design Drawing", ":data/c_BuildingDesignDrawing.png", 1.95000005f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 288
	{"Siphon Bay Module", ":data/c_BayModule.png", 25266.7695f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 289
	{"Officer Log", ":data/c_OfficerLog.png", 1.58000004f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 290
	{"Wildcatter Bay Module", ":data/c_BayModule.png", 27499.8398f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 291
	{"\u041a\u0430\u043b\u0430\u0301\u0448\u043d\u0438\u043a\u043e\u0432", ":data/c_AK47.png", 300.0f, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 292
	{"Utility Pouch", ":data/c_UtilityPouch.png", 26.7333317f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 293
	{"Grav Pack", ":data/c_GravPack.png", 422.853363f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 294
	{"Silage", ":data/c_Hay.png", 13.75f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 295
	{"Laser Rifle", ":data/c_LaserRifle.png", 2301.56006f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 296
	{"Small Rocket BlBu Firecracker", ":data/c_RocketSm.png", 1498.5f, 7, 0, 0, 0, 0, 0, 0, 151, 49, 20}, // 297
	{"Large Rocket BlBu Molotov", ":data/c_RocketLg.png", 14733.5f, 3, 0, 0, 0, 0, 0, 0, 751, 249, 100}, // 298
	{"Small Missile BlBu Barnburner", ":data/c_MissileSm.png", 2034.90002f, 7, 0, 0, 0, 0, 0, 0, 126, 24, 15}, // 299
	{"Large Missile BlBu Fireball", ":data/c_MissileLg.png", 16298.7002f, 3, 0, 0, 0, 0, 0, 0, 701, 199, 90}, // 300
	{"Small Rocket SlPi Meathook", ":data/c_RocketSm.png", 1494.0f, 7, 0, 0, 0, 0, 0, 0, 151, 49, 20}, // 301
	{"Large Rocket BuDi Scudpox", ":data/c_RocketLg.png", 3799.5f, 3, 0, 0, 0, 0, 0, 0, 751, 249, 100}, // 302
	{"Small Missile CrFr Snowball", ":data/c_MissileSm.png", 2686.31982f, 7, 0, 0, 0, 0, 0, 0, 126, 24, 15}, // 303
	{"Large Missile SlPo Sideviper", ":data/c_MissileLg.png", 16335.2002f, 3, 0, 0, 0, 0, 0, 0, 701, 199, 90}, // 304
	{"Laser Pistol", ":data/c_LaserPistol.png", 1399.5f, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 305
	{"Heavy Arms Ammunition PiRa", ":data/c_HeavyArmsAmmo.png", 252.5f, 2, 0, 0, 0, 0, 0, 0, 71, 29, 0}, // 306
	{"Heavy Arms Ammunition PiBl", ":data/c_HeavyArmsAmmo.png", 320.5f, 2, 0, 0, 0, 0, 0, 0, 71, 29, 0}, // 307
	{"Heavy Arms Ammunition PiAc", ":data/c_HeavyArmsAmmo.png", 261.5f, 2, 0, 0, 0, 0, 0, 0, 71, 29, 0}, // 308
	{"Stasis Beam Lens", ":data/c_EnergyWeapon.png", 10856.0f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 309
	{"Neutron Beam Lens", ":data/c_EnergyWeapon.png", 4016.35986f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 310
	{"Electron Beam Lens", ":data/c_EnergyWeapon.png", 3723.11987f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 311
	{"Erg Ray Lens", ":data/c_EnergyWeapon.png", 5982.58008f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 312
	{"Freeze Ray Lens", ":data/c_EnergyWeapon.png", 4763.59961f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 313
	{"Compressor Beam Lens", ":data/c_EnergyWeapon.png", 74735.3594f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 314
	{"Disruptor Beam Lens", ":data/c_EnergyWeapon.png", 9586.28027f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 315
	{"Heat Ray Lens", ":data/c_EnergyWeapon.png", 5062.84033f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 316
	{"Proton Beam Lens", ":data/c_EnergyWeapon.png", 56679.9219f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 317
	{"Disintegrator Ray Lens", ":data/c_EnergyWeapon.png", 54498.3984f, 1, 0, 0, 0, 0, 0, 0, 151, 99, 0}, // 318
	{"Weapon Deployment Bay Module", ":data/c_BayModule.png", 23315.2695f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 319
	{"Bomb BlRa Los Alamos", ":data/c_Bomb.png", 26230.0f, 0, 0, 0, 0, 0, 0, 0, 751, 249, 100}, // 320
	{"Bomb BlPi Porcupine", ":data/c_Bomb.png", 24230.0f, 0, 0, 0, 0, 0, 0, 0, 751, 249, 100}, // 321
	{"Bomb ElSt Tesla", ":data/c_Bomb.png", 24480.0f, 0, 0, 0, 0, 0, 0, 0, 751, 249, 200}, // 322
	{"Bomb SuPo Muffler", ":data/c_Bomb.png", 26680.0f, 0, 0, 0, 0, 0, 0, 0, 651, 249, 100}, // 323
	{"Bomb SlCr Meat Slicer", ":data/c_Bomb.png", 24230.0f, 0, 0, 0, 0, 0, 0, 0, 351, 249, 100}, // 324
	{"Seed", ":data/c_Seed.png", 24.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 325
	{"Small Flower Pot", ":data/c_FlowerPotSmall.png", 6.71999979f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 326
	{"Large Flower Pot", ":data/c_FlowerPotLarge.png", 12.6400003f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 327
	{"Genesis Device", ":data/c_GenesisDevice.png", 145992.359f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 328
	{"Picture Frame 60x34cm", ":data/c_PictureFrame.png", 4.69999981f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 329
	{"Picture Frame 22x28cm", ":data/c_PictureFrame.png", 3.75999975f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 330
	{"Picture Frame 120x68cm", ":data/c_PictureFrame.png", 6.26666641f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 331
	{"Head", ":data/c_Skull.png", 46.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 332
	{"Candle", ":data/c_Candle.png", 6.16666651f, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}, // 333
	{"Type O Preon Ring", ":data/c_PreonRingO.png", 14101.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 334
	{"Type B Preon Ring", ":data/c_PreonRingB.png", 17501.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 335
	{"Type A Preon Ring", ":data/c_PreonRingA.png", 20901.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 336
	{"Type F Preon Ring", ":data/c_PreonRingF.png", 24301.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 337
	{"Type G Preon Ring", ":data/c_PreonRingG.png", 27701.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 338
	{"Type K Preon Ring", ":data/c_PreonRingK.png", 31101.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 339
	{"Type M Preon Ring", ":data/c_PreonRingM.png", 34501.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 340
	{"ParcX Box", ":data/c_ParcX.png", 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 341
	{"Fire", ":data/c_StoneFirePit.png", 14.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 342
	{"Tepee", ":data/c_Tepee.png", 111.199997f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 343
	{"Pressure Tent", ":data/c_PressureTent.png", 745.799988f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 344
	{"Beacon", ":data/c_Beacon.png", 69.6999969f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 345
	{"Drum", ":data/c_Drum.png", 96.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 346
	{"Ice Cream", ":data/c_IceCream.png", 42.3333321f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 347
	{"Dairy Technology", ":data/c_TechLevel.png", 1000.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 348
	{"Burning Barrel", ":data/c_BurningBarrel.png", 26.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // 349
};

inline const Data* Get(quint16 id)
{
	return id < sizeof(Table) / sizeof(Table[0]) ? &Table[id] : nullptr;
}

} }
#endif // AuLegacyShipCommodityData_H


