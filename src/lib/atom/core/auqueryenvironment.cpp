//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auqueryenvironment.cpp,v 1.10 2022/02/19 17:40:51 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "auqueryenvironment.h"

void AuQueryEnvironment::SetDesignScene(bool gravity, bool spacecraft, ROOMTYPE roomType, bool gravityWell, bool ladder,
	double fallDistance, const AuPoint& up, ON_GROUND ground)
{
	Init();
	m_AtmType = ATMTYPE_Standard;
	m_Environment = EB_Design | EB_Breathable | EB_Atmosphere;
	if(gravity) m_Environment |= EB_Gravity;
	if(spacecraft) m_Environment |= EB_Spacecraft | EB_Pathable;
	if(ladder) m_Environment |= EB_InLadderWell;
	if(gravityWell) m_Environment |= EB_GravityWell;
	m_FallDist = fallDistance;
	m_GravityNormal = up;
	m_terNormal = up;
	m_OnGround = ground;
	m_fAirDensity = spacecraft ? .5f : 0.f;
	m_CapRoomType = roomType;
}
//

quint64 AuQueryEnvironment::MemoryUsage() const
{
	quint64 mem = 0;
	mem += sizeof(m_AtmType);
	mem += sizeof(m_BdCityId);
	mem += sizeof(m_BdId);
	mem += sizeof(m_CapRoomId);
	mem += StringMemoryUsage(m_CapRoomName);
	mem += sizeof(m_CapRoomType);
	mem += sizeof(m_CityEmpire);
	mem += sizeof(m_colAtmosphere);
	mem += sizeof(m_EnclosureId);
	mem += sizeof(m_Environment);
	mem += sizeof(m_FallDist);
	mem += sizeof(m_fAltitudeAGL);
	mem += sizeof(m_fAltitudeMSL);
	mem += sizeof(m_fCeilingDist);
	mem += sizeof(m_fCGAltitude);
	mem += sizeof(m_fAirDensity);
	mem += sizeof(m_fAtmDensity);
	mem += sizeof(m_fSeaDepth);
	mem += sizeof(m_GlobeLL);
	mem += sizeof(m_GravityNormal);
	mem += sizeof(m_InIndustry);
	mem += sizeof(m_InIndustryLL);
	mem += sizeof(m_OnGround);
	mem += sizeof(m_OrbitZone);
	mem += sizeof(m_Galaxy);
	mem += sizeof(m_terNormal);
	mem += sizeof(m_WaterDepth);
	mem += sizeof(m_WorldAtomType);
	return mem;
}
