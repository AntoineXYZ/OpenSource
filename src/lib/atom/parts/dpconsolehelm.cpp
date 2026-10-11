//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: dpconsolehelm.cpp,v 1.264 2022/04/14 16:43:11 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "dpconsolehelm.h"
//
#include "dacreature.h"
#include "dasc.h"
#include "daperson.h"
#include "auatomrenderinfo.h"
#include "dicontrolinput.h"
#include "dipanelcontrol.h"

namespace
{
bool LegacyShip(const DaAtom& atom)
{
	const DaSc* ship = dynamic_cast<const DaSc*>(&atom);
	return ship && ship->IsLegacyDebug();
}

void RemoveDeferredControls(DiControlInputArray& controls)
{
	controls.RemoveId(DpStationHelm::CI_FTLSelect);
	controls.RemoveId(DpStationHelm::CI_FTLEngage);
	controls.RemoveId(DpStationHelm::CI_FTLSettingUp);
	controls.RemoveId(DpStationHelm::CI_FTLSettingDown);
}
}

DpConsoleHelm::DpConsoleHelm()
{
}

DpConsoleHelm::~DpConsoleHelm()
{
}

DpConsoleHelm::DpConsoleHelm(const DpConsoleHelm& c) : DpStationHelm(c), DpConsole(c), m_LegacyControls(c.m_LegacyControls)
{
}

DpStation* DpConsoleHelm::Clone() const
{
	return new DpConsoleHelm(*this);
}

void DpConsoleHelm::CreateDeviceObjects(AuAtomRenderInfo& ari, const DaAtom& parent, bool inBuilding)
{
	m_LegacyControls = LegacyShip(parent);
	if(m_LegacyControls && !m_ScreenTex)
		m_ScreenTex = ari.re->CreateTextureFromBitmap(parent.Id().GetID(),
			QString("PCon%1").arg(parent.Id().GetID()), ":data/legacy_pc_Console.bmp");
	DpStationHelm::CreateDeviceObjects(ari, parent, inBuilding);
	DpConsole::CreateDeviceObjects(ari, parent, inBuilding);
}

// Called between rendering scene frames that contains this object.
void DpConsoleHelm::AnimateDeviceObjects(AuGLEffects& re, const DaVessel& ship, const AuElapsedTimer& last,
		const AuPoint& location, const DaCreature& sceneAtom)
{
	DpStationHelm::AnimateDeviceObjects(re, ship, last, location, sceneAtom);	// Primary display screen.
	DpConsole::AnimateDeviceObjects(re, ship, last, sceneAtom);					// Secondary display screen.
	if(LegacyShip(ship))
	{
		// The recovered button occupies the modern beacon/strobe rectangles.
		// Overlay it after native animation so those controls cannot replace it.
		AuGLTexture* texture = re.FindTexture(QString("PCon%1").arg(ship.Id().GetID()));
		if(texture)
		{
			static const QImage on(QStringLiteral(":data/pc_IntLightOn.bmp"));
			static const QImage dim(QStringLiteral(":data/pc_IntLightRed.bmp"));
			static const QImage off(QStringLiteral(":data/pc_IntLightOff.bmp"));
			const INTERIOR_LIGHT light = ship.InteriorLight(QString());
			QImage button = light == INTLIGHT_Bright ? on : light == INTLIGHT_Dim ? dim : off;
			texture->ReplaceSubImage(re, button, 164, 90);
			// Keep the recovered crew-door slot and hull-door indicator positions.
			static const QImage original(QStringLiteral(":data/legacy_pc_Console.bmp"));
			QImage crewSlot = original.copy(182, original.height() - 106, 16, 16);
			texture->ReplaceSubImage(re, crewSlot, 182, 90);
			static const QImage hullOpen(QStringLiteral(":data/pc_HullDoorsUnlockedO.png"));
			static const QImage hullClosed(QStringLiteral(":data/pc_HullDoorsUnlocked.png"));
			QImage hullButton = ship.AnyHullDoorOpen() ? hullOpen : hullClosed;
			texture->ReplaceSubImage(re, hullButton, 200, 90);
		}
	}
}

void DpConsoleHelm::EnumControlInputs(DiControlInputArray& controls) const
{
	DpStationHelm::EnumControlInputs(controls);
	if(m_LegacyControls)
	{
		RemoveDeferredControls(controls);
		controls.RemoveId(CI_BeaconsOnOff);
		controls.RemoveId(CI_StrobesOnOff);
		// Original helm control 13 remains reserved in the modern input enum.
		controls.append(DiControlInput(QObject::tr("Interior Lights On/Dim/Off"), CI_UNUSED0,
			Qt::Key_unknown, Qt::NoModifier, DiControlInput::MT_None, Qt::NoModifier,
			DiControlInput::JT_None, 0, 100, false));
	}
}

void DpConsoleHelm::ProcessControlInput(const DiControlInputArray& controls, DaPerson* person, DaVessel& ship,
	CONTROL_CONTEXT context, const AuPoint& location)
{
	if(!LegacyShip(ship))
	{
		DpStationHelm::ProcessControlInput(controls, person, ship, context, location);
		return;
	}
	if(!ship.IsNormal()) return;
	DiControlInputArray active = controls;
	RemoveDeferredControls(active);
	active.RemoveId(CI_BeaconsOnOff);
	active.RemoveId(CI_StrobesOnOff);
	if(active.HasId(CI_UNUSED0) && !ship.IsNew())
	{
		if(context != CONTROL_Client)
		{
			const INTERIOR_LIGHT light = ship.InteriorLight(QString());
			ship.InteriorLightToggle(light == INTLIGHT_Bright ? INTLIGHT_Dim
				: light == INTLIGHT_Dim ? INTLIGHT_Dark : INTLIGHT_Bright);
			ship.SetModified();
		}
	}
	active.RemoveId(CI_UNUSED0);
	DpStationHelm::ProcessControlInput(active, person, ship, context, location);
}

void DpConsoleHelm::Refresh(const DpStation* station, const DaAtom& parent)
{
	m_LegacyControls = LegacyShip(parent);
	DpStationHelm::Refresh(station, parent);
}

void DpConsoleHelm::Rx(QDataStream& in, DaCapsule& capsule)
{
	m_LegacyControls = LegacyShip(capsule);
	DpStationHelm::Rx(in, capsule);
}

void DpConsoleHelm::SampleControlPanelInput(qint16 panelId, const AuPoint& panelLoc,
	DiControlInputArray& controls, bool continuous)
{
	DpStationHelm::SampleControlPanelInput(panelId, panelLoc, controls, continuous);
	if(!m_LegacyControls || panelId) return;
	controls.RemoveId(CI_BeaconsOnOff);
	controls.RemoveId(CI_StrobesOnOff);
	controls.RemoveId(CI_CloseHullDoors);
	RemoveDeferredControls(controls);
	double width, height;
	PanelDimensions(width, height);
	if(width <= 0 || height <= 0) return;
	const QPoint pixel(int(panelLoc.x * 256 / width), int(panelLoc.y * 128 / height));
	const DiPanelControl legacy[] = {
		DiPanelControl(CI_UNUSED0, QRect(164, 90, 16, 16)),
		DiPanelControl(CI_CloseHullDoors, QRect(200, 83, 16, 7))
	};
	controls.SampleControls(panelId, pixel, continuous, legacy, sizeof(legacy) / sizeof(legacy[0]));
}

bool DpConsoleHelm::Tick(const AuElapsedTimer& last, const AuElapsedTimer& now, DaPerson* person)
{
	m_LegacyControls = person->Parent() && LegacyShip(*person->Parent());
	return DpStationHelm::Tick(last, now, person);
}

void DpConsoleHelm::Render(const AuMatrix4x4& projection, const AuMatrix4x4& farProjection, const AuMatrix4x4& tModel,
		const DaPerson& sceneAtom, const DaCreature* seatedCreature, AuAtomRenderInfo& ari)
{
	AuMatrix4x4 tPanel(tModel);
	if(seatedCreature)
		tPanel.Translate(0,0,-seatedCreature->GroundHeightAboveCG()/seatedCreature->GrowthScale());
	DpStationHelm::Render(projection, farProjection, tPanel, sceneAtom, seatedCreature, ari);
	DpConsole::RenderConsole(projection, tPanel, ari, m_ScreenTex);
}

void DpConsoleHelm::RenderMini(const AuMatrix4x4& projection, const AuMatrix4x4& tModel, const DaCreature* seatedCreature,
		AuAtomRenderInfo& ari)
{
	AuMatrix4x4 tPanel(tModel);
	if(seatedCreature)
		tPanel.Translate(0,0,-seatedCreature->GroundHeightAboveCG()/seatedCreature->GrowthScale());
	DpConsole::RenderConsole(projection, tPanel, ari, m_ScreenTex);
}

void DpConsoleHelm::PanelDimensions(double& width, double& height) const
{
	DpConsole::PanelDimensions(0, width, height);
}

void DpConsoleHelm::EnumControlPanels(AuQuadArray &aControlPanels, const AuMatrix4x4 &tModel, QVector<qint16>& idPanels,
		const DaCreature& seatedCreature) const
{
	AuMatrix4x4 tPanel(tModel);
	tPanel.Translate(0,0,-seatedCreature.GroundHeightAboveCG()/seatedCreature.GrowthScale());
	DpConsole::EnumControlPanels(aControlPanels, tPanel, idPanels);
}
