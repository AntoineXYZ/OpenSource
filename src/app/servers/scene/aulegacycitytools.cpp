#include "dtchatchannel.h"
#include "auatominterface.h"
#include "aubox.h"
#include "audbglobal.h"
#include "auether.h"
#include "auglobal.h"
#include "aulegacybd.h"
#include "aulegacybuildinggeometry.h"
#include "aulegacycityprotocol.h"
#include "aulegacyshipequipment.h"
#include "aumatrix4x4.h"
#include "auserverinterface.h"
#include "daatomlist.h"
#include "dalegacycity.h"
#include "daperson.h"
#include "daworldll.h"
#include "dtccpacket.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace
{
struct AvatarUnlock
{
	AuAtomInterface* target;
	~AvatarUnlock() { target->UnlockAvatar(); }
};

void ListBuildings(DaWorldLL& world, const AuPoint& origin, AuLegacyCityProtocol::Reply& reply)
{
	reply.hasList = true;
	DaAtomList atoms; world.ListNormalSubatoms(atoms);
	QVector<DaLegacyCity*> cities;
	for(DaAtom* atom : atoms)
		if(auto* city = dynamic_cast<DaLegacyCity*>(atom)) cities.append(city);
	std::sort(cities.begin(), cities.end(), [](const DaLegacyCity* a, const DaLegacyCity* b) { return a->Id() < b->Id(); });
	for(const auto* city : cities)
	{
		const auto state = city->Snapshot();
		for(const auto& dev : state.developments)
		{
			if(world.LocOfLL(dev.location, dev.altitudeMSL).Distance2(origin) > std::pow(2000 * FeetPerMeter(), 2)) continue;
			if(reply.entries.size() == AuLegacyCityProtocol::MaximumEntries) { reply.truncated = true; return; }
			AuLegacyCityProtocol::Entry e;
			e.cityId = city->Id().GetID(); e.developmentId = dev.id; e.designId = dev.designId;
			e.material = dev.materialId; e.minimumDeck = dev.minimumDeck; e.maximumDeck = dev.maximumDeck;
			e.constructed = dev.constructed;
			e.elevationMeters = float((dev.altitudeMSL - world.SurfaceAltitudeMSL(dev.location)) / FeetPerMeter());
			e.headingDegrees = float(dev.facingRadians * RAD2DEG);
			e.name = state.name; reply.entries.append(e);
		}
	}
}

bool PlacementClear(DaWorldLL& world, const AuLegacyCity::Development& dev,
	const AuLegacyBd::Geometry::Mesh& mesh, const DaAtom* ignore)
{
	AuMatrix4x4 transform;
	world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians,
		nullptr, AuPoint(), transform);
	AuBox bounds(AuPoint(mesh.minimum.x(), mesh.minimum.y(), mesh.minimum.z()),
		AuPoint(mesh.maximum.x(), mesh.maximum.y(), mesh.maximum.z()));
	bounds.Transform(transform);
	DaAtomList peers; world.ListNormalSubatoms(peers);
	for(DaAtom* peer : peers)
	{
		if(peer == ignore || peer->IsEthereal()) continue;
		AuBox occupied;
		if(!peer->RoughExtents(occupied)) continue;
		occupied.Transform(peer->Transform());
		if(bounds.DoesIntersect(occupied)) return false;
	}
	return true;
}
}

void DtChatChannel::ReadLegacyCityPacket(const QByteArray& payload)
{
	using namespace AuLegacyCityProtocol;
	Request request;
	if(!LegacyScAuthorized(false) || !DecodeRequest(payload, request)) { Disconnect(); return; }
	Reply reply; reply.token = request.token; reply.action = request.action;
	if(!LegacyScAuthorized(true)) reply.message = tr("Enable architect or programmer powers to use Legacy city test tools.");
	else if(m_LastLegacyCityRequest.isValid() && m_LastLegacyCityRequest.elapsed() < 200)
		reply.message = tr("Wait a moment before sending another city request.");
	else
	{
		m_LastLegacyCityRequest.restart();
		ProcessLegacyCityRequest(request, reply);
	}
	const QByteArray response = EncodeReply(reply);
	if(response.isEmpty()) return;
	QByteArray packet(1, char(DtChatChannelPacket::PD_LegacyCity)); packet.append(response); SendPacket(packet);
}

void DtChatChannel::ProcessLegacyCityRequest(const AuLegacyCityProtocol::Request& request, AuLegacyCityProtocol::Reply& reply)
{
	using namespace AuLegacyCityProtocol;
	if(request.action == RetiredDoors)
	{
		reply.message = tr("Legacy building doorways and hatches always remain open."); return;
	}
	QSharedPointer<const AuLegacyBd::Design> design;
	AuLegacyBd::Geometry::Mesh mesh;
	if(request.action == Spawn || request.action == Update)
	{
		design = AuLegacyBd::Catalog::BuiltIn().Find(request.designId);
		if(!design) { reply.message = tr("The selected original public blueprint is unavailable."); return; }
		AuLegacyBd::Geometry::Options options;
		options.minimumDeck = request.minimumDeck; options.maximumDeck = request.maximumDeck;
		options.materialCommodity = request.material; options.constructed = request.constructed;
		mesh = AuLegacyBd::Geometry::Build(*design, options);
		if(!mesh.TriangleCount()) { reply.message = tr("These deck limits do not produce building geometry."); return; }
	}
	// Geometry preparation above does not hold the scene deletion lock. Recheck
	// session ownership and active powers before accessing or changing any atom.
	if(!LegacyScAuthorized(false)) { reply.message = tr("Your session changed. Refresh after reconnecting."); return; }
	DaPerson* person = m_I->LockAvatar(); AvatarUnlock unlock{m_I};
	if(!person || !(person->HasArchitectPower() || person->HasProgrammerPower()))
	{ reply.message = tr("Enable architect or programmer powers first."); return; }
	auto* world = dynamic_cast<DaWorldLL*>(person->FindParentPlanetInEnv());
	AuEther* ether = person->Ether(); AuMatrix4x4 avatarTransform;
	if(!world || !ether || !person->GetTransformRelativeToParent(avatarTransform, world))
	{ reply.message = tr("Move onto a planet or ringworld before testing Legacy cities."); return; }
	const AuPoint origin = avatarTransform.Translation();
	if(request.action == List) { ListBuildings(*world, origin, reply); return; }

	DaLegacyCity* city = nullptr;
	AuLegacyCity::State state;
	int developmentIndex = -1;
	if(request.action != Spawn)
	{
		city = dynamic_cast<DaLegacyCity*>(ether->FindNormalAtom(AuId(request.cityId)));
		if(!city || city->Parent() != world) { reply.message = tr("That Legacy city is no longer on your current world. Refresh the list."); return; }
		state = city->Snapshot();
		for(int i = 0; i < state.developments.size(); ++i)
			if(state.developments[i].id == request.developmentId) { developmentIndex = i; break; }
		if(developmentIndex < 0) { reply.message = tr("That development no longer exists. Refresh the list."); return; }
		const auto& dev = state.developments[developmentIndex];
		if(world->LocOfLL(dev.location, dev.altitudeMSL).Distance2(origin) > std::pow(2000 * FeetPerMeter(), 2))
		{ reply.message = tr("Move within 2 km of the selected building."); return; }
		reply.selectedCity = city->Id().GetID(); reply.selectedDevelopment = dev.id;
	}

	if(request.action == Spawn)
	{
		AuPoint up = -world->GravityDir(origin); up.Unit();
		AuPoint forward = avatarTransform.ProjectVector(AuPoint(0, 1, 0));
		forward -= up * forward.Dot(up);
		if(forward.IsZero()) up.ArbitraryAxis(forward);
		forward.Unit();
		const AuPoint position = origin + forward * (request.distanceMeters * FeetPerMeter());
		AuLegacyCity::Development dev; dev.id = 1; dev.designId = request.designId;
		if(!world->LLOfLoc(dev.location, position)) { reply.message = tr("The requested site is outside this world's terrain. Move closer to the surface."); return; }
		dev.industryId = design->header.industryId; dev.materialId = request.material;
		dev.minimumDeck = request.minimumDeck; dev.maximumDeck = request.maximumDeck;
		dev.completedLevels = quint8(int(dev.maximumDeck) - int(dev.minimumDeck) + 1);
		dev.constructed = request.constructed;
		dev.altitudeMSL = world->SurfaceAltitudeMSL(dev.location) + request.elevationMeters * FeetPerMeter();
		dev.facingRadians = request.headingDegrees * DEG2RAD;
		if(!PlacementClear(*world, dev, mesh, nullptr))
		{ reply.message = tr("The test building would overlap an actor, vehicle, ship or city. Increase the distance or choose an open site."); return; }
		if(!g_AuDb.HasAtomIds()) { reply.message = tr("The server is restocking atom IDs. Try again shortly."); return; }
		std::unique_ptr<DaLegacyCity> created(new DaLegacyCity);
		created->Create();
		if(created->Id().IsNull()) { reply.message = tr("The server could not allocate a city ID."); return; }
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians,
			nullptr, AuPoint(), placement);
		created->SetPosAndOrient(AuAtomPos(world->Id(), placement));
		state.name = tr("Legacy test B%1").arg(request.designId);
		state.controllingEmpire = state.allegianceEmpire = person->EmpireId();
		state.developments.append(dev);
		QString error;
		if(!created->SetState(state, error)) { reply.message = error; return; }
		created->ConnectSignals(); city = created.release(); ether->AddAtom(city);
		city->SetNormal(); city->SetModified(); g_Au.AtomAdded(city->Id());
		reply.selectedCity = city->Id().GetID(); reply.selectedDevelopment = dev.id;
		reply.message = tr("Created B%1 as city %2. %3 triangles.")
			.arg(request.designId).arg(city->Id().StringId()).arg(mesh.TriangleCount());
	}
	else if(request.action == Remove)
	{
		state.developments.removeAt(developmentIndex);
		if(state.developments.isEmpty())
		{
			city->Kill();
			if(!city->IsDead()) { reply.message = tr("The city could not be removed while it contains a player."); return; }
			world->TrashSubatom(city->Id());
		}
		else { QString error; if(!city->SetState(state, error)) { reply.message = error; return; } }
		reply.selectedCity = reply.selectedDevelopment = 0;
		reply.message = tr("Removed the selected Legacy test building.");
	}
	else if(request.action == Route)
	{
		const auto& dev = state.developments[developmentIndex];
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians,
			nullptr, AuPoint(), placement);
		QVector<AuLegacyBd::Paths::RoutePoint> points;
		if(!city->Route(dev.id, origin, placement.Project(AuPoint(request.targetX, request.targetY, request.targetZ)), points))
			reply.message = tr("No accessible route from your avatar to that local point. Check floor limits and walls.");
		else reply.message = tr("Route found: %1 waypoints in the current route leg. Target: (%2, %3, %4) feet in the building.")
			.arg(points.size()).arg(request.targetX).arg(request.targetY).arg(request.targetZ);
	}
	else
	{
		auto& dev = state.developments[developmentIndex];
		if(request.action == Update)
		{
			if(request.designId != dev.designId) { reply.message = tr("Select the building's existing blueprint before applying settings. Spawn a new building to test another blueprint."); return; }
			dev.materialId = request.material; dev.minimumDeck = request.minimumDeck; dev.maximumDeck = request.maximumDeck;
			dev.completedLevels = quint8(int(dev.maximumDeck) - int(dev.minimumDeck) + 1); dev.constructed = request.constructed;
			dev.facingRadians = request.headingDegrees * DEG2RAD;
			dev.altitudeMSL = world->SurfaceAltitudeMSL(dev.location) + request.elevationMeters * FeetPerMeter();
			if(!PlacementClear(*world, dev, mesh, city)) { reply.message = tr("These test settings would overlap another actor or structure."); return; }
			reply.message = tr("Updated the test building's material, constructed decks and placement settings.");
		}
		QString error; if(!city->SetState(state, error)) { reply.message = error; return; }
	}
	if(request.action != Route)
	{
		world->InvalidateEnvironmentRecursive();
		g_AuDb.PostCriticalUpdates();
	}
	ListBuildings(*world, origin, reply);
}
