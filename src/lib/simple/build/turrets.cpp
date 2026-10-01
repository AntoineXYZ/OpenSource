#include "planbuilder.h"
#include "internal.h"

namespace SimpleMode::detail {

namespace {

struct TurretMesh
{
	AuScFaceList base, gun;
	double mountR = 2.6;
	double baseTop = 1.0;
	AuPoint pivot, muzzle, aim, sight;
	COMMODITYID module = CID_BallisticWeaponTurretModule;
};

const double Raised = 30 * M_PI / 180;

const QColor Steel(0x5a, 0x60, 0x68), Dark(0x2a, 0x2d, 0x31), Pale(0xb8, 0xbc, 0xc2);

QVector<V2> Ring(double r)
{
	QVector<V2> ring;
	for(int k = 0; k < 8; k++)
		ring << V2(qCos((k + 0.5) * M_PI / 4), qSin((k + 0.5) * M_PI / 4)) * r;
	return ring;
}

TurretMesh MakeTurretMesh(const QString& design, double k)
{
	TurretMesh t;
	AuScFaceList gun;
	auto box = [&](AuScFaceList& to, double x0, double y0, double z0, double x1, double y1, double z1, const QColor& c) {
		AuScFaceList f;
		AddBox(f, x0 * k, y0 * k, z0 * k, x1 * k, y1 * k, z1 * k);
		Paint(f, c);
		to.AddFaces(f);
	};
	auto ring = [&](AuScFaceList& to, double r, double z0, double z1, const QColor& c) {
		AuScFaceList f;
		AddPrism(f, Ring(r * k), z0 * k, z1 * k);
		Paint(f, c);
		to.AddFaces(f);
	};
	double pivotY = 0, pivotZ = 2.0, length = 7.0, sightX = 1.0, sightZ = 1.0;
	if(design == "laser")
	{
		t.module = CID_EnergyWeaponTurretModule;
		t.mountR = 2.4;
		ring(t.base, 2.2, 0.2, 0.8, Steel);
		box(t.base, -0.8, -0.8, 0.8, 0.8, 0.8, 2.2, Steel);
		box(t.base, -1.8, -0.6, 2.2, 1.8, 0.6, 2.5, Dark);
		for(double x : {-1.8, 1.4})
			box(t.base, x, -0.5, 2.5, x + 0.4, 0.5, 4.0, Dark);
		t.baseTop = 2.5;
		pivotZ = 3.2;
		box(gun, -1.2, -1.5, -0.9, 1.2, 1.8, 0.9, Pale);
		box(gun, -0.8, 1.8, -0.6, 0.8, 2.1, 0.6, QColor(0x40, 0xd8, 0xff));
		box(gun, -0.18, 2.1, -0.18, 0.18, 6.0, 0.18, Steel);
		for(double y : {3.0, 4.2, 5.4})
			box(gun, -0.35, y, -0.35, 0.35, y + 0.15, 0.35, Dark);
		length = 6.0;
		sightX = 1.4;
		sightZ = 1.1;
	}
	else if(design == "missile")
	{
		t.module = CID_WeaponLauncherTurretModule;
		t.mountR = 2.4;
		ring(t.base, 2.2, 0.2, 0.8, Steel);
		box(t.base, -0.8, -0.8, 0.8, 0.8, 0.8, 2.0, Dark);
		t.baseTop = 2.0;
		pivotZ = 2.9;
		box(gun, -2.0, -1.6, -1.1, 2.0, 1.6, 1.1, QColor(0x6a, 0x72, 0x5a));
		for(double x : {-1.2, 0.0, 1.2})
			for(double z : {-0.5, 0.5})
				box(gun, x - 0.4, 1.6, z - 0.4, x + 0.4, 1.63, z + 0.4, Dark);
		length = 1.63;
		sightX = 2.2;
		sightZ = 0.8;
	}
	else
	{
		const bool twin = design == "twin";
		ring(t.base, 2.4, 0.2, 0.8, Steel);
		box(t.base, -1.6, -1.8, 0.8, 1.6, 1.2, 2.8, Steel);
		box(t.base, -1.3, -1.6, 2.8, 1.3, 0.6, 3.1, Dark);
		box(t.base, -0.5, -1.85, 1.3, 0.5, -1.8, 2.2, Dark);
		t.baseTop = 2.8;
		pivotY = 1.0;
		pivotZ = 1.9;
		const double half = twin ? 1.3 : 0.9;
		box(gun, -half, 0.0, -0.6, half, 0.5, 0.6, Dark);
		for(double x : twin ? QList<double>{-0.6, 0.6} : QList<double>{0.0})
		{
			box(gun, x - 0.22, 0.3, -0.22, x + 0.22, 7.0, 0.22, Steel);
			box(gun, x - 0.35, 6.5, -0.35, x + 0.35, 7.1, 0.35, Dark);
		}
		length = 7.1;
		sightX = twin ? 1.7 : 1.2;
		sightZ = 0.8;
	}
	AuMatrix4x4 raise;
	raise.Translate(Ft(0, pivotY * k, pivotZ * k));
	raise.RotateX(Raised);
	gun.Transform(raise);
	t.gun = gun;
	t.pivot = Ft(0, pivotY * k, pivotZ * k);
	t.muzzle = raise.Project(Ft(0, length * k, 0));
	t.sight = raise.Project(Ft(sightX * k, 0, sightZ * k));
	t.aim = raise.ProjectVector(AuPoint(0, 1, 0));
	t.aim.Unit();
	t.mountR *= k;
	t.baseTop *= k;
	return t;
}

}

void PlanBuilder::BuildTurret(int fi)
{
	static const V2 dirs[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
	static const double turn[4] = {0, -M_PI / 2, M_PI, M_PI / 2};
	const Fixture& fx = plan.fixtures[fi];
	const FixtureType* type = FindFixtureType(fx.type);
	const QString style = fx.kind.isEmpty() ? type->kinds.first() : fx.kind;
	const int d = ((fx.dir % 4) + 4) % 4;
	const V2 mid(plan.X1(fx.cell.x()), plan.Y0(fx.cell.y()));
	const double z = TurretOnGround(plan, fx) ? 0.0 : (fx.deck + 1) * H;
	TurretMesh m = MakeTurretMesh(style, plan.cell / 3.0);

	{
		AuScFaceList ring;
		QVector<V2> r = Ring(m.mountR);
		for(V2& p : r)
			p = p + mid;
		AddPrism(ring, r, z, z + 0.2);
		Paint(ring, QColor(0x4a, 0x4e, 0x56));
		AuScMesh mount;
		mount.AddFaces(ring);
		Add(design, mount, "turret mount", log);
	}

	AuScTurret turret;
	turret.AddFaces(m.base);
	turret.AddState(AuScTurret::Gun);
	turret.AddFaces(m.gun);
	turret.SetBasePivot(Ft(0, 0, m.baseTop));
	turret.SetBaseAxis(AuPoint(0, 0, 1));
	turret.SetGunPivot(m.pivot);
	turret.SetGunAxis(AuPoint(1, 0, 0));
	turret.SetGunEmitLoc(m.muzzle);
	turret.SetGunEmitVec(m.aim);
	turret.SetGunSightLoc(m.sight);
	turret.SetModule(m.module);

	const int si = plan.interiors ? TurretGunner(plan, fi) : -1;
	const Station* st = si >= 0 && StationProblem(plan, plan.stations[si], si).isEmpty() ? &plan.stations[si] : nullptr;
	if(!st)
	{
		turret.MakeScreen(Ft(-0.15, -0.15, 0.5), Ft(0.15, 0.15, 0.5), AuMatrix4x4(Ft(0, 0, 0.5), AuPoint(1, 0, 0), AuPoint(0, 1, 0)));
		turret.SetGunner(AuScTurret::Gunner_Sit, Ft(0, 0, 0.5), Ft(0, 0, 0.5));
	}
	AuMatrix4x4 place;
	place.Translate(Ft(mid, z));
	place.RotateZ(turn[d]);
	turret.Transform(place);
	QString gunnerRoom;
	if(st)
	{
		const QChar w = plan.At(st->deck, st->cell);
		gunnerRoom = plan.rooms[w].name;
		const double floorZ = st->deck * H + VF;
		const int sd = ((st->dir % 4) + 4) % 4;
		const V2 smid(midX(st->cell), midY(st->cell)), out = dirs[sd], in = out * -1;
		const V2 xAxis(-in.y, in.x);
		const bool sit = st->pose == "sit";
		V2 face, gunnerAt;
		double z0, z1;
		if(st->wall)
		{
			QChar other = plan.Quarter(st->deck, Neighbour(st->cell, Side(sd)), Opposite(Side(sd)));
			double inset = IsOpenAirRoom(plan, w) ? 0.0 : (other == '.' || IsOpenAirRoom(plan, other)) ? plan.wall : plan.partition / 2;
			face = smid + out * (S / 2 - inset - 0.02);
			z0 = floorZ + (sit ? 0.75 : 1.05);
			z1 = floorZ + (sit ? 1.45 : 1.85);
			gunnerAt = face + in * 0.85;
		}
		else
		{
			const V2 at = smid + out * 0.35;
			const double top = floorZ + (sit ? 0.7 : 1.0);
			AuScFaceList pedestal, stool;
			AddWallBox(pedestal, at, xAxis, in, -0.2, 0.15, 0.7, st->deck * H, top);
			Paint(pedestal, Dark);
			face = at + in * 0.16;
			z0 = top + 0.02;
			z1 = top + 0.5;
			gunnerAt = smid - out * 0.4;
			if(sit)
			{
				AddBox(stool, gunnerAt.x - 0.2, gunnerAt.y - 0.2, st->deck * H, gunnerAt.x + 0.2, gunnerAt.y + 0.2, floorZ + 0.2);
				Paint(stool, Steel);
			}
			AuScMesh mesh;
			mesh.AddFaces(pedestal);
			if(!stool.isEmpty())
				mesh.AddFaces(stool);
			mesh.SetGroupName(gunnerRoom);
			Add(design, mesh, "gunnery console", log);
		}
		AuMatrix4x4 plane(Ft(face, (z0 + z1) / 2), AuPoint(xAxis.x, xAxis.y, 0), AuPoint(0, 0, 1));
		turret.MakeScreen(Ft(face - xAxis * 0.4, z0), Ft(face + xAxis * 0.4, z1), plane);
		turret.SetGunner(sit ? AuScTurret::Gunner_Sit : AuScTurret::Gunner_Stand, Ft(gunnerAt, floorZ), Ft(gunnerAt, floorZ));
	}
	turret.SetState(AuScTurret::Base);
	const quint16 id = Add(design, turret, QString("turret %1").arg(style), log);
	if(id)
		ctx.fixtureChecks << BuildContext::FixtureCheck{id, fx.type, style, gunnerRoom, fx.deck, fx.cell};
}

}
