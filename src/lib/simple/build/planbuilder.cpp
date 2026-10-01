#include "planbuilder.h"
#include "internal.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace SimpleMode::detail {

PlanBuilder::PlanBuilder(BuildContext& context, QTextStream& logTo)
	: ctx(context), plan(context.plan), D(context.D), S(context.S), H(context.H), HC(context.HC), VF(context.VF), VC(context.VC),
	  design(1, AuId(), "SimpleModePOC"), log(logTo), theme(ThemeOf(context.plan.theme)), roofPoints(context.roofPoints),
	  roofBad(context.roofBad), roofLog(context.roofLog), placedStairs(context.placedStairs), openSpans(context.openSpans),
	  railingLines(context.railingLines), groundSpots(context.groundSpots), stairChecks(context.stairChecks),
	  doorChecks(context.doorChecks), hatchChecks(context.hatchChecks)
{
	design.SetId(0);
	design.SetName(plan.name);
	design.SetStructure(BD_Building);
	design.SetIndustryId((INDUSTRYID)plan.industry);
	AuDesignTex palette[SCTEXI_Count];
	const QStringList& keys = ThemePalette(plan.theme);
	for(int i = 0; i < SCTEXI_Count && i < keys.size(); i++)
	{
		const TextureKind* k = FindTextureKind(keys[i]);
		palette[i].SetTexLibId(quint16(k && k->texlib >= 0 ? k->texlib : MaterialTexlib(plan.theme)));
	}
	design.SetTexturePalette(palette);
}

void PlanBuilder::textureBySide(AuScFaceList& faces, const QString& up, const QString& side, const QString& down, double upScale,
	double sideScale, double downScale)
{
	auto size = [](const QString& key) { const TextureKind* k = FindTextureKind(key); return k ? k->size : 2.0; };
	TextureBySide(faces, TextureSlot(plan, up), size(up) * upScale, TextureSlot(plan, side), size(side) * sideScale,
		TextureSlot(plan, down), size(down) * downScale);
}

void PlanBuilder::PlaceStairs()
{
	static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
	const double S = plan.cell, w = qMin(1.8, S - 0.6);
	for(const Stair& st : plan.stairs)
	{
		StairLayout L = LayoutStair(plan, st);
		if(!L.ok)
		{
			log << "  Skipping stair at " << st.cell.x() << "," << st.cell.y() << " on deck " << st.deck << ": " << L.problem << "\n";
			continue;
		}
		PlacedStair ps{st, L, st.style.isEmpty() ? DefaultStairStyle(plan.theme) : st.style, {}, {}};
		for(const StairLayout::Flight& f : L.flights)
		{
			V2 F = dirs[f.dir], R(F.y, -F.x);
			V2 from((plan.X0(f.from.x()) + plan.X1(f.from.x())) / 2, (plan.Y0(f.from.y()) + plan.Y1(f.from.y())) / 2);
			int cells = 1 + qAbs(f.to.x() - f.from.x()) + qAbs(f.to.y() - f.from.y());
			ps.frames << StairFrame{from - F * (S / 2), F, R, cells * S, w};
		}
		if(!L.landings.isEmpty())
		{
			PlacedStair::Landing land{1e9, 1e9, -1e9, -1e9, L.flights[0].z1, {false, false, false, false}};
			for(const QPoint& c : L.landings)
			{
				land.x0 = qMin(land.x0, plan.X0(c.x()));
				land.x1 = qMax(land.x1, plan.X1(c.x()));
				land.y0 = qMin(land.y0, plan.Y0(c.y()));
				land.y1 = qMax(land.y1, plan.Y1(c.y()));
			}
			const double m = (S - w) / 2;
			for(int s = 0; s < 4; s++)
			{
				bool joined = false;
				for(const QPoint& c : L.landings)
				{
					QPoint q = Neighbour(c, Side(s));
					joined = joined || (L.cells.contains(q) && !L.landings.contains(q));
				}
				land.outer[s] = !joined;
			}
			if(land.outer[North])
				land.y1 -= m;
			if(land.outer[South])
				land.y0 += m;
			if(land.outer[East])
				land.x1 -= m;
			if(land.outer[West])
				land.x0 += m;
			ps.landings << land;
		}
		placedStairs << ps;
	}
}

void PlanBuilder::BuildSite()
{
	double x0 = plan.X0(0) - 2, x1 = plan.X1(plan.Cols() - 1) + 2;
	double y0 = plan.Y0(plan.Rows() - 1) - 2, y1 = plan.Y1(0) + 2;
	AuPointArray bnd;
	bnd.append(Ft(x0, y0, 0));
	bnd.append(Ft(x0, y1, 0));
	bnd.append(Ft(x1, y1, 0));
	bnd.append(Ft(x1, y0, 0));
	AuScSite site;
	site.MakeSite(bnd, AuMatrix4x4());
	Add(design, site, "site", log);
}

void PlanBuilder::BuildDeck()
{
	lift = AuMatrix4x4();
	lift.Translate(AuPoint(0, 0, D * H * FeetPerMeter()));

	PlaceDeckItems();
	BuildHull();
	const bool inside = plan.interiors;
	if(inside)
		BuildRooms();
	BuildOutsideTrims();
	if(inside)
	{
		BuildOpenAir();
		BuildUnderVoids();
	}
	BuildRoofs();
	BuildSkylights();
	BuildDormers();
	if(inside)
	{
		BuildRoomVoids();
		BuildOpenVoids();
	}
	BuildWindows();
	BuildDoors();
	if(inside)
	{
		BuildLifts();
		BuildHatches();
		BuildStairs();
		BuildStairWells();
		BuildPaths();
	}
}

static AuScPart::PART_TYPE StationPart(const QString& kind, bool wall)
{
	static const QMap<QString, QPair<AuScPart::PART_TYPE, AuScPart::PART_TYPE>> parts = {
		{"command", {AuScPart::Part_ConsoleCommand, AuScPart::Part_StationCommand}},
		{"firecontrol", {AuScPart::Part_ConsoleFireControl, AuScPart::Part_StationFireControl}},
		{"medic", {AuScPart::Part_ConsoleMedic, AuScPart::Part_StationMedic}},
		{"navigator", {AuScPart::Part_ConsoleNavigator, AuScPart::Part_StationNavigator}},
		{"sensor", {AuScPart::Part_ConsoleSensor, AuScPart::Part_StationSensor}},
		{"shield", {AuScPart::Part_ConsoleShield, AuScPart::Part_StationShield}},
		{"transporter", {AuScPart::Part_ConsoleTransporter, AuScPart::Part_StationTransporter}},
	};
	QPair<AuScPart::PART_TYPE, AuScPart::PART_TYPE> p = parts.value(kind, parts["command"]);
	return wall ? p.second : p.first;
}

static COMMODITYID StationModule(const QString& kind, const QString& module)
{
	static const QMap<QString, COMMODITYID> modules = {
		{"firecontrol/energy", CID_EnergyWeaponBayModule}, {"firecontrol/ballistic", CID_BallisticWeaponBayModule},
		{"firecontrol/launcher", CID_WeaponLauncherBayModule},
		{"medic/surgery", CID_SurgeryMedicalModule}, {"medic/genetics", CID_GeneticsMedicalModule},
		{"medic/cybernetics", CID_CyberneticsMedicalModule},
		{"sensor/sensor", CID_SensorModule},
		{"shield/energy", CID_EnergyShieldModule}, {"shield/kinetic", CID_KineticShieldModule},
		{"shield/cloaking", CID_CloakingShieldModule},
		{"transporter/personnel", CID_PersonnelTransporterModule}, {"transporter/vehicle", CID_VehicleTransporterModule},
	};
	const StationKind* k = FindStationKind(kind);
	QString key = module.isEmpty() && k && !k->modules.isEmpty() ? k->modules.first() : module;
	return modules.value(kind + "/" + key, CID_Invalid);
}

void PlanBuilder::BuildStations()
{
	static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
	for(int si = 0; si < plan.stations.size(); si++)
	{
		const Station& st = plan.stations[si];
		QString problem = StationProblem(plan, st, si);
		if(!problem.isEmpty())
		{
			log << "  Skipping station at " << st.cell.x() << "," << st.cell.y() << " on deck " << st.deck << ": " << problem << "\n";
			continue;
		}
		if(st.kind == "gunner")
			continue;
		const StationKind* kind = FindStationKind(st.kind);
		const QChar w = plan.At(st.deck, st.cell);
		const Room& room = plan.rooms[w];
		const double floorZ = st.deck * H + VF;
		const int d = ((st.dir % 4) + 4) % 4;
		const V2 mid(midX(st.cell), midY(st.cell));
		AuScPart* part = AuScPart::Create(StationPart(st.kind, st.wall));
		if(!part)
			continue;
		V2 operatorAt = mid;
		if(!st.wall)
		{
			static const double turn[4] = {0, -M_PI / 2, M_PI, M_PI / 2};
			AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
			if(icon)
				icon->MakeIcon(Ft(mid, floorZ), turn[d], AuMatrix4x4());
		}
		else
		{
			const V2 out = dirs[d], in = out * -1;
			QChar other = plan.Quarter(st.deck, Neighbour(st.cell, Side(d)), Opposite(Side(d)));
			double inset = IsOpenAirRoom(plan, w) ? 0.0 : (other == '.' || IsOpenAirRoom(plan, other)) ? plan.wall : plan.partition / 2;
			const V2 face = mid + out * (S / 2 - inset - 0.02);
			const V2 xAxis(-in.y, in.x);
			const bool sit = st.pose == "sit";
			const double z0 = floorZ + (sit ? 0.75 : 1.05), z1 = floorZ + (sit ? 1.45 : 1.85);
			AuMatrix4x4 plane(Ft(face, (z0 + z1) / 2), AuPoint(xAxis.x, xAxis.y, 0), AuPoint(0, 0, 1));
			operatorAt = face + in * 0.85;
			AuScStation* station = dynamic_cast<AuScStation*>(part);
			if(station)
				station->MakeStation(sit ? AuScStation::Operator_Sit : AuScStation::Operator_Stand, Ft(operatorAt, floorZ),
					Ft(operatorAt + out * 1.0, floorZ), Ft(face - xAxis * 0.5, z0), Ft(face + xAxis * 0.5, z1), plane);
		}
		part->SetModule(StationModule(st.kind, st.module));
		part->SetWhichEffect(st.effect);
		QList<AuPoint> spots;
		const QPoint spare = StationSpareCell(plan, st);
		if(st.kind == "transporter")
		{
			AuPointArray pads;
			for(const QPoint& p : st.pads.isEmpty() ? QList<QPoint>{spare} : st.pads)
			{
				pads.append(Ft(V2(midX(p), midY(p)), floorZ));
				spots << pads.last();
			}
			part->SetPadLocs(pads);
		}
		if(st.kind == "medic")
		{
			V2 at(midX(spare), midY(spare));
			V2 toward = (operatorAt - at).Unit();
			AuScPart::PATIENT_POSITION pos = st.patient == "prone" ? AuScPart::Patient_Prone
				: st.patient == "sit" ? AuScPart::Patient_Sit : AuScPart::Patient_Stand;
			if(AuScConsoleMedic* m = dynamic_cast<AuScConsoleMedic*>(part))
				m->SetPatient(pos, Ft(at, floorZ), Ft(at + toward, floorZ));
			if(AuScStationMedic* m = dynamic_cast<AuScStationMedic*>(part))
				m->SetPatient(pos, Ft(at, floorZ), Ft(at + toward, floorZ));
			spots << Ft(at, floorZ);
		}
		part->SetGroupName(room.name);
		quint16 id = Add(design, *part, QString("%1 %2").arg(kind->name).arg(st.wall ? "station" : "console"), log);
		delete part;
		if(id)
			ctx.stationChecks << StationCheck{id, st.deck, st.cell, room.name, spots};
	}
}

static void AddLens(AuBdDesign& design, QTextStream& log, const QString& kind, const QColor& color, const V2& at, double z)
{
	AuScFaceList faces;
	if(kind == "beacon")
	{
		QVector<V2> oct;
		for(int k = 0; k < 8; k++)
			oct << at + V2(qCos(k * M_PI / 4), qSin(k * M_PI / 4)) * 0.14;
		AddPrism(faces, oct, z, z + 0.22);
	}
	else if(kind == "strobe")
		AddBox(faces, at.x - 0.09, at.y - 0.09, z, at.x + 0.09, at.y + 0.09, z + 0.06);
	else
		AddBox(faces, at.x - 0.07, at.y - 0.07, z, at.x + 0.07, at.y + 0.07, z + 0.14);
	Paint(faces, color);
	if(kind == "beacon")
	{
		AuScRotatingBeaconLens lens;
		lens.AddFaces(faces);
		lens.SetColor(color);
		lens.SetLightRotationAxis(AuPoint(0, 0, 1));
		Add(design, lens, "rotating beacon", log);
	}
	else if(kind == "strobe")
	{
		AuScStrobeLens lens;
		lens.AddFaces(faces);
		lens.SetColor(color);
		Add(design, lens, "strobe", log);
	}
	else
	{
		AuScNavLightLens lens;
		lens.AddFaces(faces);
		lens.SetColor(color);
		Add(design, lens, "navigation light", log);
	}
}

static QColor LightColor(const QString& name, const QString& kind)
{
	QString c = !name.isEmpty() ? name : kind == "beacon" ? "amber" : kind == "strobe" ? "white" : "red";
	return c == "green" ? QColor(0x30, 0xff, 0x50) : c == "white" ? QColor(0xff, 0xff, 0xf0) : c == "amber" ? QColor(0xff, 0xa0, 0x20)
		: c == "blue" ? QColor(0x40, 0x80, 0xff) : QColor(0xff, 0x30, 0x20);
}

void PlanBuilder::BuildFixtures()
{
	static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
	static const double turn[4] = {0, -M_PI / 2, M_PI, M_PI / 2};
	for(int fi = 0; fi < plan.fixtures.size(); fi++)
	{
		const Fixture& fx = plan.fixtures[fi];
		QString problem = FixtureProblem(plan, fx, fi);
		if(problem.isEmpty() && !plan.interiors && (fx.type == "berth" || fx.type == "gametable"
				|| (fx.type == "parking" && fx.kind != "spacecraft")))
			problem = "It is inside, and the building has no interiors.";
		if(!problem.isEmpty())
		{
			log << "  Skipping " << fx.type << " at " << fx.cell.x() << "," << fx.cell.y() << " on deck " << fx.deck << ": " << problem << "\n";
			continue;
		}
		const FixtureType* type = FindFixtureType(fx.type);
		const QString kind = fx.kind.isEmpty() ? type->kinds.first() : fx.kind;
		const int d = ((fx.dir % 4) + 4) % 4;
		const V2 mid(midX(fx.cell), midY(fx.cell));
		const QChar w = plan.At(fx.deck, fx.cell);
		if(fx.type == "turret")
		{
			BuildTurret(fi);
			continue;
		}
		if(fx.type == "gametable")
		{
			V2 at = mid;
			ParkingCentre(plan, fx, at.x, at.y);
			AuScGameTable table;
			table.MakeIcon(Ft(at, fx.deck * H + VF), turn[d], AuMatrix4x4());
			table.SetGroupName(plan.rooms[w].name);
			quint16 id = Add(design, table, "game table", log);
			if(id)
				ctx.fixtureChecks << BuildContext::FixtureCheck{id, fx.type, kind, plan.rooms[w].name, fx.deck, fx.cell};
			continue;
		}
		if(fx.type == "berth" || fx.type == "parking")
		{
			V2 at = mid;
			if(fx.type == "parking" && kind != "spacecraft")
				ParkingCentre(plan, fx, at.x, at.y);
			AuPoint loc = Ft(at, fx.deck * H + (kind == "spacecraft" ? 0.0 : VF + 0.02));
			AuScPart* part = nullptr;
			if(fx.type == "berth")
				part = kind == "captain" ? (AuScPart*)new AuScBerthCapt : kind == "officer" ? (AuScPart*)new AuScBerthOfficer
					: kind == "troop" ? (AuScPart*)new AuScBerthTroop : kind == "passenger" ? (AuScPart*)new AuScBerthPassenger
					: (AuScPart*)new AuScBerthCrew;
			else
				part = kind == "water" ? (AuScPart*)new AuScWaterVehicleParkingSpot : kind == "rocket" ? (AuScPart*)new AuScRocketParkingSpot
					: kind == "space" ? (AuScPart*)new AuScSpaceVehicleParkingSpot : kind == "spacecraft" ? (AuScPart*)new AuScSpacecraftParkingSpot
					: (AuScPart*)new AuScGroundVehicleParkingSpot;
			AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
			if(icon)
				icon->MakeIcon(loc, turn[d], AuMatrix4x4());
			const QString roomName = plan.rooms.contains(w) ? plan.rooms[w].name : QString();
			part->SetGroupName(roomName);
			quint16 id = Add(design, *part, type->name + " " + kind, log);
			delete part;
			if(id)
				ctx.fixtureChecks << BuildContext::FixtureCheck{id, fx.type, kind, roomName, fx.deck, fx.cell};
			continue;
		}
		const QColor color = LightColor(fx.color, kind);
		if(fx.type == "light")
		{
			if(fx.dir == FixtureTop)
			{
				AddLens(design, log, kind, color, mid, (fx.deck + 1) * H + 0.02);
				continue;
			}
			const double z = fx.deck * H + (fx.height > 0 ? qMin(fx.height, H - 0.3) : H - 0.55);
			const V2 at = mid + dirs[d] * (S / 2 + 0.16);
			AuScFaceList bracket;
			AddWallBox(bracket, mid + dirs[d] * (S / 2 + 0.06), V2(dirs[d].y, -dirs[d].x), dirs[d], -0.06, 0.06, 0.1, z - 0.04, z);
			Paint(bracket, QColor(0x3c, 0x40, 0x46));
			AuScMesh mount;
			mount.AddFaces(bracket);
			Add(design, mount, "light bracket", log);
			AddLens(design, log, kind, color, at, z);
			continue;
		}
		const double top = fx.height > 0 ? fx.height : fx.type == "flagpole" ? 7.0 : 5.0;
		AuScFaceList pole;
		QVector<V2> ring;
		for(int k = 0; k < 8; k++)
			ring << mid + V2(qCos(k * M_PI / 4), qSin(k * M_PI / 4)) * 0.06;
		AddPrism(pole, ring, 0, top);
		AddBox(pole, mid.x - 0.2, mid.y - 0.2, 0, mid.x + 0.2, mid.y + 0.2, 0.15);
		Paint(pole, fx.type == "flagpole" ? QColor(0xd8, 0xd8, 0xd0) : QColor(0x4a, 0x4e, 0x56));
		AuScMesh post;
		post.AddFaces(pole);
		Add(design, post, fx.type == "flagpole" ? "flag pole" : "light pole", log);
		if(fx.type == "lightpole")
		{
			AddLens(design, log, kind, color, mid, top);
			continue;
		}
		const V2 along = dirs[d], center = mid + along * 0.81;
		for(int side : {1, -1})
		{
			const V2 in(along.y * side, -along.x * side);
			const V2 xAxis(-in.y, in.x);
			AuMatrix4x4 plane(Ft(center + in * 0.005, top - 0.6), AuPoint(xAxis.x, xAxis.y, 0), AuPoint(0, 0, 1));
			AuScEmpireFlag flag;
			flag.MakeScreen(Ft(center + in * 0.005 - xAxis * 0.75, top - 1.1), Ft(center + in * 0.005 + xAxis * 0.75, top - 0.1), plane);
			Add(design, flag, "empire flag", log);
		}
	}
}

void PlanBuilder::BuildPosts()
{
	for(const Post& post : plan.posts)
	{
		QChar w = plan.At(post.deck, post.cell);
		if(w == '.' || w == '*' || IsOpenCell(plan, post.deck, post.cell))
		{
			log << "  Skipping post at " << post.cell.x() << "," << post.cell.y() << " on deck " << post.deck
				<< ": it needs a whole room cell.\n";
			continue;
		}
		AuPoint loc = centre(post.cell, post.deck * H);
		if(post.kind == "citizen")
		{
			AuScCitizenPost part;
			part.MakeIcon(loc, 0, AuMatrix4x4());
			Add(design, part, "citizen post", log);
		}
		else if(post.kind == "troop")
		{
			AuScTroopPost part;
			part.MakeIcon(loc, 0, AuMatrix4x4());
			Add(design, part, "troop post", log);
		}
		else
			log << "  Unknown post kind " << post.kind << " (citizen or troop)\n";
	}
}

void PlanBuilder::SetVolumes(const QMap<QChar, int>& quarters)
{
	int allQuarters = 0;
	for(int q : quarters)
		allQuarters += q;
	design.SetSkeletonVolume(quint32(allQuarters / 4.0 * S * S * H * 0.2));
}

quint16 PlanBuilder::add(AuScPart& part, const QString& what)
{
	if(D)
		part.Transform(lift);
	return Add(design, part, what, log);
}

AuPoint PlanBuilder::up(const AuPoint& p, int deck)
{
	return ctx.up(p, deck);
}

QChar PlanBuilder::whole(const QPoint& p)
{
	return ctx.whole(p);
}

QChar PlanBuilder::facing(const QPoint& p, Side s)
{
	return ctx.facing(p, s);
}

double PlanBuilder::thickness(QChar other)
{
	return ctx.thickness(other);
}

double PlanBuilder::voidInset(const QPoint& p, Side s)
{
	return ctx.voidInset(p, s);
}

bool PlanBuilder::stairCellOn(int deck, const QPoint& p)
{
	return ctx.stairCellOn(deck, p);
}

double PlanBuilder::stairSurface(int deck, const V2& p)
{
	return ctx.stairSurface(deck, p);
}

bool PlanBuilder::hatchAt(int deck, const QPoint& p)
{
	if(!plan.interiors)
		return false;
	for(const Hatch& h : plan.hatches)
		if(!h.roof && h.deck == deck && h.cell == p && deck > 0 && !IsOpenCell(plan, deck, p) && !stairCellOn(deck, p))
		{
			QChar a = plan.At(deck, p), b = plan.At(deck - 1, p);
			if(a != '.' && a != '*' && b != '.' && b != '*' && !IsLiftRoom(plan, a) && !IsLiftRoom(plan, b))
				return true;
		}
	return false;
}

bool PlanBuilder::roofHatchAt(int deck, const QPoint& p)
{
	if(!plan.interiors)
		return false;
	for(int i = 0; i < plan.hatches.size(); i++)
		if(plan.hatches[i].roof && plan.hatches[i].deck == deck && plan.hatches[i].cell == p)
			return !stairCellOn(deck, p) && RoofHatchProblem(plan, plan.hatches[i], i).isEmpty();
	return false;
}

double PlanBuilder::midX(const QPoint& p)
{
	return (plan.X0(p.x()) + plan.X1(p.x())) / 2;
}

double PlanBuilder::midY(const QPoint& p)
{
	return (plan.Y0(p.y()) + plan.Y1(p.y())) / 2;
}

AuPoint PlanBuilder::centre(const QPoint& c, double z)
{
	return Ft(midX(c), midY(c), z);
}

QList<Piece> PlanBuilder::piecesOf(int deck)
{
	return ctx.piecesOf(deck);
}

int PlanBuilder::pieceIndex(const QList<Piece>& pieces, const QPoint& cell, int q)
{
	for(int i = 0; i < pieces.size(); i++)
		if(pieces[i].cell == cell && (pieces[i].quarter < 0 || pieces[i].quarter == q))
			return i;
	return -1;
}

QVector<V2> PlanBuilder::quarterVoidUncut(const QPoint& cell, int q)
{
	QVector<V2> t = QuarterPoly(plan, cell, q);
	QChar room = plan.Quarter(D, cell, q);
	V2 base[3], dir[3];
	for(int k = 0; k < 3; k++)
	{
		QPoint oc;
		int oq;
		QuarterAcross(cell, q, k, oc, oq);
		QChar other = plan.Quarter(D, oc, oq);
		double shift = other == room ? -VOID_OVERLAP : thickness(other) + VOID_INSET;
		if(IsOpenEdge(plan, room, other))
			shift = OpenEdgeInset(plan, room) + VOID_INSET;
		else if(IsOpenAirRoom(plan, room) && other != '.' && !IsOpenAirRoom(plan, other))
			shift = SHELL_INSET + VOID_INSET;
		dir[k] = (t[(k + 1) % 3] - t[k]).Unit();
		base[k] = t[k] + dir[k].Left() * shift;
	}
	QVector<V2> v;
	for(int k = 0; k < 3; k++)
		v << Intersect(base[(k + 2) % 3], dir[(k + 2) % 3], base[k], dir[k]);
	return v;
}

double PlanBuilder::openFloorInset(QChar room, const QPoint& cell)
{
	double inset = OpenEdgeInset(plan, room);
	if(inset <= 0.2 || D == 0 || plan.At(D - 1, cell) != '.')
		return 0;
	return inset - 0.08;
}

QList<VoidBox> PlanBuilder::nearRects(QChar room)
{
	auto hulledAt = [&](int deck, const QPoint& cell) {
		for(int q = 0; q < 4; q++)
		{
			QChar ch = plan.Quarter(deck, cell, q);
			if(ch != '.' && !IsOpenAirRoom(plan, ch))
				return true;
		}
		return false;
	};
	auto building = [&](const QPoint& cell) { return hulledAt(D, cell) || hulledAt(D - 1, cell); };
	const double N = OPEN_AIR_NEAR;
	QList<VoidBox> list;
	for(int r = 0; r < plan.Rows(); r++)
		for(int c = 0; c < plan.Cols(); c++)
		{
			QPoint cell(c, r);
			bool mine = false;
			for(int q = 0; q < 4; q++)
				mine = mine || plan.Quarter(D, cell, q) == room;
			if(!mine)
				continue;
			const double x0 = plan.X0(c), x1 = plan.X1(c), y0 = plan.Y0(r), y1 = plan.Y1(r);
			if(whole(cell) == '*' || hulledAt(D - 1, cell))
			{
				list << VoidBox{x0, y0, x1, y1};
				continue;
			}
			if(building(Neighbour(cell, North)))
				list << VoidBox{x0, y1 - N, x1, y1};
			if(building(Neighbour(cell, South)))
				list << VoidBox{x0, y0, x1, y0 + N};
			if(building(Neighbour(cell, East)))
				list << VoidBox{x1 - N, y0, x1, y1};
			if(building(Neighbour(cell, West)))
				list << VoidBox{x0, y0, x0 + N, y1};
			if(building(QPoint(c + 1, r - 1)))
				list << VoidBox{x1 - N, y1 - N, x1, y1};
			if(building(QPoint(c - 1, r - 1)))
				list << VoidBox{x0, y1 - N, x0 + N, y1};
			if(building(QPoint(c + 1, r + 1)))
				list << VoidBox{x1 - N, y0, x1, y0 + N};
			if(building(QPoint(c - 1, r + 1)))
				list << VoidBox{x0, y0, x0 + N, y0 + N};
		}
	for(const VoidBox& b : list)
		ctx.nearAreas << qMakePair(D, b);
	return list;
}

QList<QPair<VoidBox, bool>> PlanBuilder::splitNear(const VoidBox& box, const QList<VoidBox>& nearList)
{
	QList<double> xs{box.x0, box.x1}, ys{box.y0, box.y1};
	for(const VoidBox& n : nearList)
	{
		for(double x : {n.x0, n.x1})
			if(x > box.x0 + 1e-6 && x < box.x1 - 1e-6 && !xs.contains(x))
				xs << x;
		for(double y : {n.y0, n.y1})
			if(y > box.y0 + 1e-6 && y < box.y1 - 1e-6 && !ys.contains(y))
				ys << y;
	}
	std::sort(xs.begin(), xs.end());
	std::sort(ys.begin(), ys.end());
	QList<QPair<VoidBox, bool>> pieces;
	for(int i = 0; i + 1 < xs.size(); i++)
		for(int j = 0; j + 1 < ys.size(); j++)
		{
			VoidBox b{xs[i], ys[j], xs[i + 1], ys[j + 1]};
			if(b.x1 - b.x0 < 1e-3 || b.y1 - b.y0 < 1e-3)
				continue;
			double mx = (b.x0 + b.x1) / 2, my = (b.y0 + b.y1) / 2;
			bool isNear = false;
			for(const VoidBox& n : nearList)
				isNear = isNear || (mx > n.x0 && mx < n.x1 && my > n.y0 && my < n.y1);
			pieces << qMakePair(b, isNear);
		}
	auto same = [](double a, double b) { return qAbs(a - b) < 1e-6; };
	for(bool again = true; again; )
	{
		again = false;
		for(int i = 0; i < pieces.size() && !again; i++)
			for(int j = 0; j < pieces.size() && !again; j++)
			{
				if(i == j || pieces[i].second != pieces[j].second)
					continue;
				VoidBox& a = pieces[i].first;
				const VoidBox& b = pieces[j].first;
				bool rowWise = same(a.y0, b.y0) && same(a.y1, b.y1) && same(a.x1, b.x0);
				bool columnWise = same(a.x0, b.x0) && same(a.x1, b.x1) && same(a.y1, b.y0);
				if(!rowWise && !columnWise)
					continue;
				a = VoidBox{a.x0, a.y0, rowWise ? b.x1 : a.x1, columnWise ? b.y1 : a.y1};
				pieces.removeAt(j);
				again = true;
			}
	}
	return pieces;
}

bool PlanBuilder::coveredAbove(const QPoint& cell, int q)
{
	QChar roof = plan.RoofAt(D, cell);
	return plan.Quarter(D + 1, cell, q) != '.' || (roof != '.' && plan.roofs.contains(roof));
}

bool PlanBuilder::coveredAbove(const QPoint& cell)
{
	for(int q = 0; q < 4; q++)
		if(coveredAbove(cell, q))
			return true;
	return false;
}

bool PlanBuilder::arrivesOnOpenAir(const PlacedStair& ps)
{
	const int up = ps.stair.deck + 1;
	return !ps.frames.isEmpty() && IsOpenAirRoom(plan, plan.At(up, ps.layout.arrival))
		&& plan.Quarter(up, ps.layout.top, ps.layout.arrivalSide) == '.';
}

const PlacedStair* PlanBuilder::stairArrivingAt(const QPoint& cell, Side s)
{
	for(const PlacedStair& ps : placedStairs)
		if(ps.stair.deck == D - 1 && ps.layout.arrival == cell && Opposite(Side(ps.layout.arrivalSide)) == s && arrivesOnOpenAir(ps))
			return &ps;
	return nullptr;
}

const QList<QList<Edge>>& PlanBuilder::outlineOf(int deck, QChar room)
{
	return ctx.outlineOf(deck, room);
}

QList<QVector<V2>> PlanBuilder::carveVoid(QChar room, QVector<V2> v, const QVector<V2>& base)
{
	const QList<QList<Edge>>& loops = outlineOf(D, room);
	for(const QList<Edge>& loop : loops)
		for(const Edge& e : loop)
		{
			V2 u = (e.b - e.a).Unit();
			double len = (e.b - e.a).Length();
			bool through = false, outside = false;
			for(const V2& p : base)
			{
				double along = u.Dot(p - e.a), across = u.Cross(p - e.a);
				if(qAbs(across) < 1e-6 && along > -1e-6 && along < len + 1e-6)
					through = true;
				if(across < -1e-6)
					outside = true;
			}
			bool alongSide = false;
			for(int k = 0; k < base.size(); k++)
			{
				V2 side = base[(k + 1) % base.size()] - base[k];
				if(side.Length() > 1e-9 && qAbs(u.Cross(side.Unit())) < 1e-6 && qAbs(u.Cross(base[k] - e.a)) < 1e-6)
					alongSide = true;
			}
			if(through && !outside && !alongSide)
				v = ClipLeftOf(v, e.a, u, e.inset + VOID_INSET);
		}

	QList<QVector<V2>> pieces;
	pieces << v;
	for(const QList<Edge>& loop : loops)
		for(int i = 0; i < loop.size(); i++)
		{
			const Edge& e1 = loop[i];
			const Edge& e2 = loop[(i + 1) % loop.size()];
			V2 u1 = (e1.b - e1.a).Unit(), u2 = (e2.b - e2.a).Unit();
			if(u1.Cross(u2) > -1e-9 || (e1.b - e2.a).Length() > 1e-6)
				continue;
			V2 tip = e1.b;
			double c1 = e1.inset + VOID_INSET, c2 = e2.inset + VOID_INSET;
			struct Half { V2 n; double k; };
			const double tipAngle = M_PI - qAcos(qBound(-1.0, u1.Dot(u2), 1.0));
			const double reach = qMax(c1, c2) / qTan(qMax(0.05, tipAngle / 2)) * 1.5 + 0.1;
			const double len1 = qMin((e1.b - e1.a).Length(), reach), len2 = qMin((e2.b - e2.a).Length(), reach);
			const QList<QList<Half>> wedges = {{
				{u1.Left() * -1, -(c1 + u1.Left().Dot(tip))},
				{u2.Left() * -1, -(c2 + u2.Left().Dot(tip))},
				{u1, u1.Dot(tip) - len1},
				{u2 * -1, -(u2.Dot(tip) + len2)},
			}};
			for(const QList<Half>& wedge : wedges)
			{
				QList<QVector<V2>> next;
				for(const QVector<V2>& piece : pieces)
				{
					QVector<V2> inWedge = piece;
					for(const Half& h : wedge)
						inWedge = ClipHalf(inWedge, h.n, h.k);
					if(inWedge.size() < 3 || Area(inWedge) < 1e-6)
					{
						next << piece;
						continue;
					}
					QVector<V2> rest = piece;
					for(const Half& h : wedge)
					{
						QVector<V2> out = ClipHalf(rest, h.n * -1, -h.k);
						if(out.size() >= 3 && Area(out) > 1e-4)
							next << out;
						rest = ClipHalf(rest, h.n, h.k);
						if(rest.size() < 3)
							break;
					}
				}
				pieces = next;
			}
		}
	return pieces;
}

QVector<V2> PlanBuilder::rectPoly(double x0, double y0, double x1, double y1)
{
	return QVector<V2>{V2(x0, y0), V2(x1, y0), V2(x1, y1), V2(x0, y1)};
}

WallRun PlanBuilder::wallRun(const V2& a, const V2& b, const V2& n, double top, const QList<QPair<double, double>>& doors)
{
	WallRun w;
	w.a = a;
	w.u = (b - a).Unit();
	w.n = n;
	w.len = (b - a).Length();
	w.top = top;
	w.doors = doors;
	w.lowPoly = plan.lowPoly;
	w.slot = [this](const QString& key) { return TextureSlot(plan, key); };
	for(const SwitchSpot& sp : switchSpots)
	{
		if(w.n.Dot(sp.n) * sp.side < 0.9)
			continue;
		double s = (sp.p - a).Dot(w.u);
		if(qAbs(w.u.Cross(sp.p - a)) < 0.35 && s > -0.2 && s < w.len + 0.2)
			w.keep << WallRun::Keep{s - 0.12, s + 0.12, SWITCH_HEIGHT - 0.2, SWITCH_HEIGHT + 0.2};
	}
	for(const ConsoleSpot& c : consoles)
	{
		double s = (c.at - a).Dot(w.u);
		if(qAbs(w.u.Cross(c.at - a)) < 0.02 && s > -0.3 && s < w.len + 0.3)
			w.keep << WallRun::Keep{s - 0.3, s + 0.3, 0.8, 1.7};
	}
	for(const WindowSpot& ws : windowSpots)
	{
		double s = (ws.m - a).Dot(w.u);
		if(qAbs(w.u.Cross(ws.u)) < 1e-3 && qAbs(w.u.Cross(ws.m - a)) < 0.35 && s > -ws.half - ws.margin && s < w.len + ws.half + ws.margin)
			w.keep << WallRun::Keep{s - ws.half - ws.margin, s + ws.half + ws.margin, ws.sill - 0.1, ws.head + 0.14};
	}
	return w;
}

void PlanBuilder::addPath(const AuPoint& from, const AuPoint& to, quint16 doorId)
{
	AuScPathWalk path;
	path.MakePath(from, to);
	if(doorId)
		path.SetAssociatedPart(doorId, AuScDoor::Open);
	add(path, "walk path");
}

}
