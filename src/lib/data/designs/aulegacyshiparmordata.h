#ifndef AuLegacyShipArmorData_H
#define AuLegacyShipArmorData_H

#include <QtGlobal>

namespace AuLegacyShip { namespace LegacyArmor {
// Original GD 0x3a mode entries are Area, Contact, Projectile, Psionic;
// effect entries retain the original WE_None through WE_Generic order.
// Absorption bytes are divided by 200, blocking bytes are whole damage points.
struct Data
{
	quint16 module;
	quint8 modeAbsorb[4], modeBlock[4];
	quint8 effectAbsorb[17], effectBlock[17];
};

constexpr Data Table[] = {
	{237, {20,20,20,4}, {10,10,10,2},
		{0,0,0,2,0,0,0,0,0,0,2,0,2,2,0,0,0}, {0,0,0,1,0,0,0,0,0,0,1,0,1,0,0,0,0}},
	{240, {10,10,10,2}, {5,5,5,1},
		{0,0,2,0,0,2,2,2,0,0,0,0,2,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
	{241, {10,10,10,2}, {5,5,5,1},
		{0,2,0,2,0,0,0,0,0,2,2,0,0,2,2,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
	{242, {10,10,10,2}, {5,5,5,1},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
	{280, {30,30,30,6}, {15,15,15,4},
		{0,2,2,2,0,0,0,2,0,0,2,0,2,2,0,0,0}, {0,1,1,1,0,0,0,1,0,0,0,0,1,1,0,0,0}},
	{281, {60,60,60,8}, {30,30,30,5},
		{0,2,2,2,0,0,0,2,0,0,2,0,2,2,0,0,0}, {0,1,1,1,0,0,0,1,0,0,0,0,1,1,0,0,0}}
};

inline const Data* Get(quint16 module)
{
	for(const Data& value : Table) if(value.module == module) return &value;
	return nullptr;
}
}}

#endif // AuLegacyShipArmorData_H
