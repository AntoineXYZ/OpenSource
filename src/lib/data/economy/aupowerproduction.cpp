#include "aupowerproduction.h"

qint32 AuPowerProduction::ScaleForOrbit(qint32 quantity, INDUSTRYID industry, ORBITZONE zone)
{
	if(industry == IID_WindPowerPlant)
	{
		switch(zone)
		{
		case OZONE_Inferno: quantity /= 4; break;
		case OZONE_Inner: quantity /= 2; break;
		case OZONE_Outer: quantity *= 2; break;
		case OZONE_Frigid: quantity *= 4; break;
		default: break;
		}
	}
	else if(industry == IID_SolarPowerPlant)
	{
		switch(zone)
		{
		case OZONE_Inferno: quantity *= 4; break;
		case OZONE_Inner: quantity *= 2; break;
		case OZONE_Outer: quantity /= 2; break;
		case OZONE_Frigid: quantity /= 4; break;
		default: break;
		}
	}
	return quantity;
}
