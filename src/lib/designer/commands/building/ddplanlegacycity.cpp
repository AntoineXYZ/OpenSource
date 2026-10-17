#include "ddplanlegacycity.h"
#include "auatomrenderinfo.h"
#include "audesignerview.h"
#include "augleffects.h"
#include "auglobal.h"
#include "aulegacybd.h"
#include "aulegacybuildinggeometry.h"
#include "aulegacybiodome.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityconstruction.h"
#include "aulegacycitystate.h"
#include "daperson.h"
#include "dalegacycity.h"
#include "daatomlist.h"
#include "daworldll.h"
#include <QWheelEvent>
#include <QGuiApplication>
#include <QCursor>
#include <QPainter>
#include <QPixmap>
#include <cmath>

namespace
{
void AddLine(QList<AuPointAlpha>& vertices, const AuPoint& a, const AuPoint& b)
{
	vertices.append(AuPointAlpha(a.x, a.y, a.z)); vertices.append(AuPointAlpha(b.x, b.y, b.z));
}

double ClearSiteRadius(const AuLegacyCity::Development& dev)
{
	if(dev.industryId == 129) return dev.biodomeRadiusMeters * FeetPerMeter();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	const auto* recipe = !catalogue ? nullptr : dev.constructionProcessId
		? catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId))
		: catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(dev.industryId));
	return recipe ? recipe->buildRadius * FeetPerMeter() : 0;
}

struct ClearTarget
{
	quint32 city = 0;
	AuLegacyCity::Development development;
	AuMatrix4x4 placement;
	bool allowed = false;
};

bool FindClearTarget(const DaWorldLL& world, const AuPoint& point, ClearTarget& hit)
{
	AuGlobeLLF ll; if(!world.LLOfLoc(ll, point)) return false;
	DaAtomList atoms; world.ListNormalSubatoms(atoms);
	for(const auto* atom : atoms)
		if(const auto* city = dynamic_cast<const DaLegacyCity*>(atom))
		{
			if(city->IsDead()) continue;
			const auto state = city->Snapshot();
			// Original lookup prefers buildings, then roads, then enclosing biodomes.
			for(int kind = 0; kind < 3; ++kind) for(const auto& dev : state.developments)
			{
				if((dev.industryId == 129 ? 2 : dev.industryId == 2 ? 1 : 0) != kind) continue;
				const double radius = ClearSiteRadius(dev);
				const double halfLength = dev.roadLengthMeters * FeetPerMeter() / 2;
				const double halfWidth = dev.roadWidthMeters * FeetPerMeter() / 2;
				const AuPoint center = world.LocOfLL(dev.location, dev.altitudeMSL);
				if(center.Distance2(point) > std::pow(2 * (kind == 1 ? qMax(halfLength, halfWidth) : radius) + 100, 2)) continue;
				AuMatrix4x4 placement;
				world.BuildingConstructionTransform(center, dev.facingRadians, nullptr, AuPoint(), placement);
				const AuPoint local = placement.UnProject(world.LocOfLL(ll, dev.altitudeMSL));
				bool contains = kind == 2 ? center.Distance(world.LocOfLL(ll, dev.altitudeMSL)) <= radius + AuLegacyBd::Biodome::ContainmentMargin
					: std::abs(local.x) <= radius && std::abs(local.y) <= radius;
				if(kind == 1)
				{
					AuGlobeLLF start, end;
					world.LLOfLoc(start, placement.Project(AuPoint(0, -halfLength, 0)));
					world.LLOfLoc(end, placement.Project(AuPoint(0, halfLength, 0)));
					float fraction;
					const auto closest = ll.CoordOnSegment(start, end, &fraction, world.LLShape());
					const double grade = dev.roadStartAltitudeMSL + (dev.roadEndAltitudeMSL - dev.roadStartAltitudeMSL) * qBound(0.f, fraction, 1.f);
					contains = fraction >= 0 && fraction <= 1
						&& world.LocOfLL(ll, grade).Distance2(world.LocOfLL(closest, grade)) <= halfWidth * halfWidth;
				}
				if(!contains) continue;
				hit.city = city->Id().GetID(); hit.development = dev; hit.placement = placement;
				hit.allowed = dev.industryId != 1 || state.developments.size() == 1;
				return true;
			}
		}
	return false;
}

void ClearOutline(const DaWorldLL& world, const ClearTarget& hit,
	QList<AuPointAlpha>& vertices, QList<AuPointAlpha>& ground)
{
	const auto& dev = hit.development;
	const auto onGround = [&world](AuPoint point) {
		AuGlobeLL ll; return world.LLOfLoc(ll, point) ? world.GetTerrainLocation(ll) : point;
	};
	if(dev.industryId == 2)
	{
		// DpGlobeRoad::RenderCursorHit: edges sampled every fifteen feet.
		const double halfLength = dev.roadLengthMeters * FeetPerMeter() / 2;
		const double halfWidth = dev.roadWidthMeters * FeetPerMeter() / 2;
		AuGlobeLLF startLL, endLL;
		world.LLOfLoc(startLL, hit.placement.Project(AuPoint(0, -halfLength, 0)));
		world.LLOfLoc(endLL, hit.placement.Project(AuPoint(0, halfLength, 0)));
		const AuPoint start = world.LocOfLL(startLL, dev.roadStartAltitudeMSL);
		const AuPoint end = world.LocOfLL(endLL, dev.roadEndAltitudeMSL), direction = end - start;
		const double length = direction.Length(); if(length <= 0) return;
		const bool landscape = dev.constructionProcessId == 5 || dev.constructionProcessId == 74
			|| dev.constructionProcessId == 80 || dev.constructionProcessId == 81;
		AuPoint previousLeft, previousRight, previousCenter, firstLeft, firstRight;
		for(double along = 0;; along = qMin(length, along + 15))
		{
			AuPoint center = start + direction * (along / length);
			AuPoint right = direction.Cross(-world.GravityDir(center));
			if(right.IsZero()) right = hit.placement.ProjectVector(AuPoint(1, 0, 0));
			right.Unit(halfWidth);
			world.ElevateLocMSL(center, dev.roadStartAltitudeMSL + (dev.roadEndAltitudeMSL - dev.roadStartAltitudeMSL) * along / length);
			const AuPoint leftEdge = center - right, rightEdge = center + right;
			if(!along) { firstLeft = leftEdge; firstRight = rightEdge; }
			else
			{
				AddLine(vertices, previousLeft, leftEdge); AddLine(vertices, previousRight, rightEdge);
				if(!landscape && !dev.roadSlab) AddLine(vertices, previousCenter, center);
			}
			if(!landscape && !dev.roadSlab) AddLine(vertices, leftEdge, rightEdge);
			previousLeft = leftEdge; previousRight = rightEdge; previousCenter = center;
			if(along >= length) break;
		}
		if(dev.roadSlab)
		{
			AddLine(vertices, firstLeft, firstRight); AddLine(vertices, previousLeft, previousRight);
			if(!landscape) { AddLine(vertices, firstLeft, previousRight); AddLine(vertices, firstRight, previousLeft); }
		}
		return;
	}
	const double radius = ClearSiteRadius(dev);
	const int count = dev.industryId == 129 ? AuLegacyCityCatalogue::BiodomeSegmentCount(radius) : 32;
	AuPoint first, previous, firstTop, previousTop;
	for(int i = 0; i < count; ++i)
	{
		const float angle = float(i) * PIf * 2 / count;
		float x = std::cos(angle), y = std::sin(angle);
		if(dev.industryId != 129) { const float scale = qMax(std::abs(x), std::abs(y)); x /= scale; y /= scale; }
		const AuPoint point = hit.placement.Project(AuPoint(radius * x, radius * y, 0));
		const AuPoint bottom = onGround(point);
		if(!i) first = bottom; else AddLine(dev.industryId == 129 ? ground : vertices, previous, bottom);
		previous = bottom;
		if(dev.industryId == 129)
		{
			AuPoint top = point; world.ElevateLocMSL(top, dev.altitudeMSL + AuLegacyBd::Biodome::BeamHeight);
			AddLine(vertices, bottom, top);
			if(!i) firstTop = top; else AddLine(vertices, previousTop, top);
			previousTop = top;
		}
	}
	if(count) AddLine(dev.industryId == 129 ? ground : vertices, previous, first);
	if(dev.industryId == 129 && count) AddLine(vertices, previousTop, firstTop);
}

AuLegacyBd::Geometry::Mesh Preview(const AuLegacyCityProtocol::Request& request)
{
	quint32 designId = request.designId;
	if(!designId)
		for(quint32 id = 1; id <= 74; ++id)
		{
			const auto design = AuLegacyBd::Catalog::BuiltIn().Find(id);
			if(design && design->header.industryId == request.industryId) { designId = id; break; }
		}
	const auto design = AuLegacyBd::Catalog::BuiltIn().Find(designId);
	if(design)
	{
		AuLegacyBd::Geometry::Options options; options.materialCommodity = request.material;
		options.minimumDeck = design->MinimumDeck(); options.maximumDeck = design->MaximumDeck();
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			if(const auto* recipe = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(request.processId)))
				if(recipe->stackLimit > 0 && recipe->stackLimit < 32)
					options.maximumDeck = qMin(options.maximumDeck, qMax(0, int(request.levels) - 1));
		options.interior = false;
		return AuLegacyBd::Geometry::Build(*design, options);
	}
	AuLegacyCity::Development dev; dev.industryId = request.industryId; dev.materialId = request.material;
	dev.constructionProcessId = request.processId; dev.completedLevels = 1; dev.constructed = true;
	dev.roadLengthMeters = request.roadLengthMeters; dev.roadWidthMeters = request.roadWidthMeters;
	dev.biodomeRadiusMeters = request.biodomeRadiusMeters;
	return AuLegacyBd::Geometry::BuildProcedural(dev);
}
}

DdPlanLegacyCity::DdPlanLegacyCity(AuDesignerView* view, AuLegacyCityConstruction* controls,
	const AuLegacyCityProtocol::Request& request) : DdCommand(view), m_Interface(controls), m_Request(request)
{
	m_Angle = request.targetX;
	m_Request.targetX = m_Request.targetY = m_Request.targetZ = 0;
}

DdPlanLegacyCity::~DdPlanLegacyCity()
{
	if(m_Interface) m_Interface->PlanningStopped();
}

bool DdPlanLegacyCity::UsesControls(const AuLegacyCityConstruction* controls) const
{
	return m_Interface.data() == controls;
}

void DdPlanLegacyCity::UpdateSelection(const AuLegacyCityProtocol::Request& request)
{
	const bool sameProcess = request.industryId == m_Request.industryId && request.processId == m_Request.processId
		&& request.action == m_Request.action;
	const float heading = m_Request.headingDegrees;
	m_Request = request; m_Request.headingDegrees = heading;
	ChangeLevels(0);
	m_Angle = request.targetX;
	m_Request.targetX = m_Request.targetY = m_Request.targetZ = 0;
	if(!sameProcess) m_HasStart = false;
	m_PreviewValid = false;
}

void DdPlanLegacyCity::SetCursor(const AuMatrix4x4& camera, DaPerson* person, const QPoint& pixel)
{
	const QPixmap icon = m_Interface ? m_Interface->PlacementIcon(m_Request) : QPixmap();
	if(!person || !g_Au.IsLegacyGeneration() || icon.isNull())
	{ DdCommand::SetCursor(camera, person, pixel); return; }
	// 2016: DdPlanGlobeRoad::SetCursor (0x140327ba0) uses the native
	// road bitmap, doubled to 32 pixels for slabs, with a centered hotspot.
	if(m_Request.action == AuLegacyCityProtocol::Build && m_Request.industryId == 2)
	{
		m_pWnd->setCursor(QCursor(m_Request.roadSlab ? icon.scaledToWidth(32) : icon)); return;
	}
	// Building 0x140321130, biodome 0x14031bd50 and clear 0x14031d920.
	const bool building = m_Request.action == AuLegacyCityProtocol::Build && m_Request.industryId != 129;
	QPixmap image(building ? 64 : 32, 32); image.fill(Qt::transparent);
	QPainter painter(&image); painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.drawPixmap(QRectF(0, 0, 32, 32), icon, QRectF());
	if(building && m_Request.levels)
	{
		painter.setFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));
		const QString levels = QString::number(m_Request.levels);
		painter.setPen(Qt::black); painter.drawText(QRect(1, 1, 32, 32), Qt::AlignLeft | Qt::AlignTop, levels);
		painter.setPen(QColor(25, 229, 25)); painter.drawText(QRect(0, 0, 31, 31), Qt::AlignLeft | Qt::AlignTop, levels);
	}
	painter.end(); m_pWnd->setCursor(QCursor(image));
}

void DdPlanLegacyCity::ChangeMaterial(int direction)
{
	if(m_Request.industryId == 2) return;
	const quint16 materials[] = {86, 48, 76, 51, 146, 145};
	for(int i = 0; i < 6; ++i)
		if(materials[i] == m_Request.material)
		{
			m_Request.material = materials[(i + direction + 6) % 6]; m_PreviewValid = false;
			ChangeLevels(0);
			if(m_Interface) m_Interface->SetCursorMaterial(m_Request.material); return;
		}
}

void DdPlanLegacyCity::ChangeRotation(int direction, bool fine)
{
	if(m_Request.industryId == 2)
	{
		m_Request.roadSlab = !m_Request.roadSlab;
		if(m_Interface) m_Interface->SetCursorRoadSlab(m_Request.roadSlab); return;
	}
	double degrees = m_Request.headingDegrees + direction * (fine ? 1 : (m_Angle > 0 ? m_Angle : 45));
	while(degrees > 180) degrees -= 360;
	while(degrees < -180) degrees += 360;
	m_Request.headingDegrees = float(degrees);
}

void DdPlanLegacyCity::ChangeLevels(int direction)
{
	if(m_Request.action != AuLegacyCityProtocol::Build || m_Request.industryId == 2 || m_Request.industryId == 129) return;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn(); if(!catalogue) return;
	const int limit = catalogue->ConstructionLevelLimit(AuLegacyCity::ConstructionProcessId(m_Request.processId),
		AuLegacyCity::CommodityId(m_Request.material));
	const int maximum = qMax(1, limit);
	int levels = int(m_Request.levels) + direction;
	// Original DdPlanGlobeBuilding wraps when stepping past either end.
	// A settings change only clamps the existing selection to its new limit.
	if(direction) { if(levels < 1) levels = maximum; else if(levels > maximum) levels = 1; }
	else levels = qBound(1, levels, maximum);
	m_Request.levels = quint8(levels);
	m_PreviewValid = false;
	if(m_Interface) m_Interface->SetCursorLevels(m_Request.levels);
}

bool DdPlanLegacyCity::OnKeyDown(QKeyEvent* event)
{
	switch(event->key())
	{
	case Qt::Key_Left: ChangeRotation(1, event->modifiers() & Qt::ShiftModifier); return true;
	case Qt::Key_Right: ChangeRotation(-1, event->modifiers() & Qt::ShiftModifier); return true;
	case Qt::Key_Up: if(m_Request.industryId == 2) ChangeRotation(1, false); else ChangeMaterial(1); return true;
	case Qt::Key_Down: if(m_Request.industryId == 2) ChangeRotation(-1, false); else ChangeMaterial(-1); return true;
	case Qt::Key_Plus: case Qt::Key_Equal: ChangeLevels(1); return true;
	case Qt::Key_Minus: ChangeLevels(-1); return true;
	case Qt::Key_Space: m_HasStart = false; return true;
	default: return DdCommand::OnKeyDown(event);
	}
}

void DdPlanLegacyCity::OnMouseWheel(QWheelEvent* event)
{
	const int delta = event->angleDelta().y(); if(!delta) return;
	if(m_Request.industryId == 2) ChangeRotation(delta > 0 ? 1 : -1, false);
	else ChangeLevels(delta > 0 ? 1 : -1);
}

void DdPlanLegacyCity::ConstrainRoad(const DaWorldLL& world, AuPoint& point, bool constrain) const
{
	if(!m_HasStart) return;
	if(constrain && m_Angle > 0)
	{
		AuMatrix4x4 basis; world.BuildingConstructionTransform(m_Start, 0, nullptr, AuPoint(), basis);
		AuPoint delta = basis.Inverse().ProjectVector(point - m_Start);
		const double length = std::hypot(delta.x, delta.y);
		const double angle = std::round(std::atan2(delta.y, delta.x) * RAD2DEG / m_Angle) * m_Angle * DEG2RAD;
		point = basis.Project(AuPoint(std::cos(angle) * length, std::sin(angle) * length, 0));
	}
	AuGlobeLLF location;
	if(world.LLOfLoc(location, point))
		point = world.LocOfLL(location, m_Request.roadSlab ? world.AltitudeMSL(m_Start) : world.SurfaceAltitudeMSL(location));
}

void DdPlanLegacyCity::OnButtonDown(QMouseEvent* event)
{
	if(event->button() != Qt::LeftButton || !m_Interface || !g_Au.IsLegacyGeneration()) return;
	DaPerson* person = nullptr; m_pWnd->LockSceneAtom(&person);
	AuPoint point; DaWorldLL* world = person ? m_pWnd->FindTerrainLocationAtPixel(person, event->pos(), point) : nullptr;
	AuLegacyCityProtocol::Request request = m_Request;
	bool submit = false;
	if(world)
	{
		if(request.action == AuLegacyCityProtocol::ClearLand)
		{
			ClearTarget hit;
			if(!FindClearTarget(*world, point, hit) || !hit.allowed) { m_pWnd->UnlockSceneAtom(); return; }
			request.cityId = hit.city; request.developmentId = hit.development.id;
		}
		if((request.industryId == 2 || request.industryId == 129) && request.action == AuLegacyCityProtocol::Build && !m_HasStart)
		{ m_Start = point; m_StartWorld = world->Id().GetID(); m_HasStart = true; }
		else
		{
			if((request.industryId == 2 || request.industryId == 129) && m_HasStart)
			{
				if(m_StartWorld != world->Id().GetID()) { m_HasStart = false; m_pWnd->UnlockSceneAtom(); return; }
				if(request.industryId == 2) ConstrainRoad(*world, point, event->modifiers() & Qt::ShiftModifier);
				else
				{
					AuGlobeLLF radiusLocation;
					if(world->LLOfLoc(radiusLocation, point)) point = world->LocOfLL(radiusLocation, world->AltitudeMSL(m_Start));
				}
				AuPoint middle = request.industryId == 129 ? m_Start : (m_Start + point) / 2.;
				AuGlobeLLF center;
				if(world->LLOfLoc(center, middle))
					middle = world->LocOfLL(center, world->SurfaceAltitudeMSL(center));
				AuMatrix4x4 basis; world->BuildingConstructionTransform(middle, 0, nullptr, AuPoint(), basis);
				const AuPoint delta = basis.Inverse().ProjectVector(point - m_Start);
				request.headingDegrees = float(std::atan2(-delta.x, delta.y) * RAD2DEG);
				const float sizeMeters = float((point - m_Start).Length() / FeetPerMeter());
				if(request.industryId == 129) request.biodomeRadiusMeters = sizeMeters;
				else request.roadLengthMeters = sizeMeters;
				point = middle; m_HasStart = false;
			}
			AuGlobeLLF location;
			if(world->LLOfLoc(location, point))
			{
				if(request.action == AuLegacyCityProtocol::Build && request.industryId != 2 && request.industryId != 129)
					request.headingDegrees = float(std::remainder(request.headingDegrees - world->BuildingConstructionNorthAngle(point, nullptr) * RAD2DEG, 360.));
				request.worldId = world->Id().GetID(); request.latitudeRadians = location.Latitude();
				request.longitudeRadians = location.Longitude(); request.explicitLocation = true; submit = true;
			}
		}
	}
	m_pWnd->UnlockSceneAtom();
	if(submit && m_Interface) m_Interface->Submit(request);
}

void DdPlanLegacyCity::DeleteDeviceObjects(AuGLEffects& effects)
{
	Q_UNUSED(effects); if(m_PreviewVAO.VAO()) m_PreviewVAO.DeleteGL();
}

void DdPlanLegacyCity::RenderCursor(const AuMatrix4x4& projection, const AuMatrix4x4& camera,
	DaPerson* person, const QPoint& pixel, AuAtomRenderInfo& info, bool snapped)
{
	Q_UNUSED(snapped);
	if(!person || !g_Au.IsLegacyGeneration() || !info.re || !info.glw) return;
	AuPoint point; auto* world = m_pWnd->FindTerrainLocationAtPixel(person, pixel, point);
	auto* environment = person->FindEnvironmentAtom(); if(!world || !environment) return;
	AuMatrix4x4 scene, terrain, placement;
	if(!person->GetTransformRelativeToParent(scene, environment) || !world->GetTransformRelativeToParent(terrain, environment)) return;
	AuLegacyCityProtocol::Request preview = m_Request;
	AuPoint roadEnd;
	const bool roadDrag = m_Request.industryId == 2 && m_HasStart && m_StartWorld == world->Id().GetID();
	if(preview.action == AuLegacyCityProtocol::Build && preview.industryId != 2 && preview.industryId != 129)
		preview.headingDegrees = float(std::remainder(preview.headingDegrees - world->BuildingConstructionNorthAngle(point, nullptr) * RAD2DEG, 360.));
	if(m_Request.industryId == 129 && m_HasStart && m_StartWorld == world->Id().GetID())
	{
		AuGlobeLLF radiusLocation;
		if(world->LLOfLoc(radiusLocation, point)) point = world->LocOfLL(radiusLocation, world->AltitudeMSL(m_Start));
		preview.biodomeRadiusMeters = float((point - m_Start).Length() / FeetPerMeter()); point = m_Start;
	}
	if(roadDrag)
	{
		ConstrainRoad(*world, point, QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
		roadEnd = point;
		AuPoint middle = (point + m_Start) / 2.;
		// Match the submitted site's center instead of placing the preview at
		// the chord midpoint beneath a curved world's surface.
		AuGlobeLLF center;
		if(world->LLOfLoc(center, middle)) middle = world->LocOfLL(center, world->SurfaceAltitudeMSL(center));
		// BuildingConstructionTransform appends to its matrix. Keep this
		// heading basis separate so the final placement is applied only once.
		AuMatrix4x4 basis;
		world->BuildingConstructionTransform(middle, 0, nullptr, AuPoint(), basis);
		const AuPoint delta = basis.Inverse().ProjectVector(point - m_Start);
		preview.headingDegrees = float(std::atan2(-delta.x, delta.y) * RAD2DEG);
		preview.roadLengthMeters = float((point - m_Start).Length() / FeetPerMeter()); point = middle;
	}
	world->BuildingConstructionTransform(point, preview.headingDegrees * DEG2RAD, nullptr, AuPoint(), placement);
	// Road edges below are world-relative: rebuilding a horizontal rectangle
	// at the moving midpoint changes the apparent start height on uneven land.
	const AuMatrix4x4 worldModel = camera * scene.Inverse() * terrain;
	AuMatrix4x4 model = roadDrag ? worldModel : worldModel * placement;
	QList<AuPointAlpha> vertices, ground;
	bool clearAllowed = true;
	if(m_Request.action == AuLegacyCityProtocol::ClearLand)
	{
		ClearTarget hit; if(!FindClearTarget(*world, point, hit)) return;
		ClearOutline(*world, hit, vertices, ground); clearAllowed = hit.allowed;
		model = worldModel;
	}
	else if(m_Request.industryId == 2)
	{
		const double width = preview.roadWidthMeters * FeetPerMeter() / 2;
		if(roadDrag)
		{
			// Follow the recovered road cursor: integrate from the saved first
			// endpoint, interpolating grade and projecting onto world curvature.
			const AuPoint direction = roadEnd - m_Start;
			const int segments = qMax(1, int(std::ceil(direction.Length() / (10 * FeetPerMeter()))));
			const double startAltitude = world->AltitudeMSL(m_Start), endAltitude = world->AltitudeMSL(roadEnd);
			AuPoint previousLeft, previousRight, firstLeft, firstRight;
			for(int segment = 0; segment <= segments; ++segment)
			{
				const double fraction = double(segment) / segments;
				AuPoint center = segment == 0 ? m_Start : segment == segments ? roadEnd : m_Start + direction * fraction;
				if(segment > 0 && segment < segments)
					world->ElevateLocMSL(center, startAltitude + (endAltitude - startAltitude) * fraction);
				const AuPoint up = -world->GravityDir(center);
				AuPoint right = direction.Cross(up);
				if(right.IsZero()) up.ArbitraryAxis(right);
				right.Unit(width);
				const AuPoint leftEdge = center - right, rightEdge = center + right;
				if(segment == 0)
				{
					firstLeft = leftEdge; firstRight = rightEdge; AddLine(vertices, leftEdge, rightEdge);
				}
				else
				{
					AddLine(vertices, previousLeft, leftEdge); AddLine(vertices, previousRight, rightEdge);
					if(segment == segments) AddLine(vertices, leftEdge, rightEdge);
				}
				previousLeft = leftEdge; previousRight = rightEdge;
			}
			if(preview.roadSlab) { AddLine(vertices, firstLeft, previousRight); AddLine(vertices, firstRight, previousLeft); }
		}
		else
		{
			const AuPoint a(-width, -width, 1), b(width, -width, 1), c(width, width, 1), d(-width, width, 1);
			AddLine(vertices, a, b); AddLine(vertices, b, c); AddLine(vertices, c, d); AddLine(vertices, d, a);
			if(preview.roadSlab) { AddLine(vertices, a, c); AddLine(vertices, b, d); }
		}
	}
	else if(m_Request.industryId == 129)
	{
		const double radius = preview.biodomeRadiusMeters * FeetPerMeter();
		const double edge = AuLegacyBd::Biodome::RoofHeight(radius, radius);
		for(int i = 0; i < 64; ++i)
		{
			const double a = i * PI / 32., b = (i + 1) * PI / 32.;
			AddLine(vertices, AuPoint(std::cos(a)*radius, std::sin(a)*radius, 1), AuPoint(std::cos(b)*radius, std::sin(b)*radius, 1));
			AddLine(vertices, AuPoint(std::cos(a)*radius, std::sin(a)*radius, edge), AuPoint(std::cos(b)*radius, std::sin(b)*radius, edge));
			if(i < 32)
			{
				const double x = std::cos(a)*radius, y = std::cos(b)*radius;
				const double h1 = AuLegacyBd::Biodome::RoofHeight(radius, std::abs(x));
				const double h2 = AuLegacyBd::Biodome::RoofHeight(radius, std::abs(y));
				AddLine(vertices, AuPoint(x, 0, h1), AuPoint(y, 0, h2));
				AddLine(vertices, AuPoint(0, x, h1), AuPoint(0, y, h2));
			}
		}
	}
	else if(!m_PreviewValid)
	{
		if(m_PreviewVAO.VAO()) m_PreviewVAO.DeleteGL();
		m_Preview.clear(); const auto mesh = Preview(preview);
		// Decode/build the authored geometry once per material selection, never
		// once per rendered frame. Rotation only changes the model matrix.
		for(const auto& batch : mesh.batches)
		{
			if(batch.interior || batch.collisionOnly) continue;
			for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
			{
				const auto& a = batch.vertices[i].position; const auto& b = batch.vertices[i + 1].position;
				const auto& c = batch.vertices[i + 2].position;
				const AuPoint p(a.x(), a.y(), a.z()), q(b.x(), b.y(), b.z()), r(c.x(), c.y(), c.z());
				AddLine(m_Preview, p, q); AddLine(m_Preview, q, r); AddLine(m_Preview, r, p);
			}
		}
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			if(const auto* recipe = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId{preview.processId}))
			{
				const double radius = recipe->buildRadius * FeetPerMeter();
				const AuPoint a(-radius, -radius, 1), b(radius, -radius, 1), c(radius, radius, 1), d(-radius, radius, 1);
				AddLine(m_Preview, a, b); AddLine(m_Preview, b, c); AddLine(m_Preview, c, d); AddLine(m_Preview, d, a);
			}
		m_PreviewValid = true;
	}
	auto& effects = *info.re;
	GLboolean depthTest, depthWrite;
	effects.glGetBooleanv(GL_DEPTH_TEST, &depthTest);
	effects.glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
	// Original building/road placement cursors draw over terrain and scenery.
	// Preserve the caller's state so this overlay cannot affect later passes.
	effects.glDisable(GL_DEPTH_TEST); effects.glDepthMask(GL_FALSE);
	effects.GLSL().LineShader().Select(); effects.GLSL().LineShader().SetProjectionModelView(projection * model);
	if(m_Request.action == AuLegacyCityProtocol::ClearLand)
		effects.GLSL().LineShader().SetColor(.9f, clearAllowed ? .35f : .1f, .1f, .9f);
	else effects.GLSL().LineShader().SetColor(.1f, .9f, .1f, .9f);
	effects.glEnable(GL_BLEND); effects.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(!ground.isEmpty())
	{
		AuGLVAO vao; vao.CreateForSprites(effects, ground, GL_STREAM_DRAW);
		if(vao.VAO()) { vao.SelectGL(); vao.DrawGL(GL_LINES); vao.DeselectGL(); vao.DeleteGL(); }
	}
	effects.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	if(m_Request.action != AuLegacyCityProtocol::ClearLand && m_Request.industryId != 2 && m_Request.industryId != 129)
	{
		if(!m_PreviewVAO.VAO()) m_PreviewVAO.CreateForSprites(effects, m_Preview, GL_STATIC_DRAW);
		if(m_PreviewVAO.VAO()) { m_PreviewVAO.SelectGL(); m_PreviewVAO.DrawGL(GL_LINES); m_PreviewVAO.DeselectGL(); }
	}
	else
	{
		AuGLVAO vao; vao.CreateForSprites(effects, vertices, GL_STREAM_DRAW);
		if(vao.VAO()) { vao.SelectGL(); vao.DrawGL(GL_LINES); vao.DeselectGL(); vao.DeleteGL(); }
	}
	effects.glDisable(GL_BLEND); effects.GLSL().LineShader().Deselect();
	effects.glDepthMask(depthWrite);
	if(depthTest) effects.glEnable(GL_DEPTH_TEST);
	if(m_Request.action == AuLegacyCityProtocol::ClearLand || (m_Request.industryId != 2 && m_Request.industryId != 129)) return;
	QFont font; font.setPixelSize(14);
	const QString text = m_Request.industryId == 2 ? (m_HasStart ? QObject::tr("Click road end — %1 m").arg(preview.roadLengthMeters, 0, 'f', 1) : QObject::tr("Click road start"))
		: (m_HasStart ? QObject::tr("Click biodome radius — %1 m").arg(preview.biodomeRadiusMeters, 0, 'f', 1) : QObject::tr("Click biodome center"));
	effects.RenderText(pixel.x() + 16, pixel.y() + 24, text, font, QColor(255, 230, 100), info.glw->width(), info.glw->height());
}
