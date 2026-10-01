#include "planbuilder.h"
#include "internal.h"

namespace SimpleMode::detail {

void PlanBuilder::BuildLifts()
{
	for(const Room& room : plan.rooms)
	{
		if(!IsLiftRoom(plan, room.letter))
			continue;
		QList<int> decks = LiftDecks(plan, room.letter);
		if(decks.isEmpty() || decks.first() != D)
			continue;
		const QString problem = LiftProblem(plan, room.letter);
		if(!problem.isEmpty())
		{
			log << "  Skipping " << room.name << ": " << problem << "\n";
			continue;
		}

		QStringList stops;
		QList<int> stopDecks;
		QSet<int> doorSides;
		static const V2 outward[4] = {V2(0, 1), V2(1, 0), V2(0, -1), V2(-1, 0)};
		for(int d : decks)
			for(const Door& door : plan.doors)
			{
				if(door.deck != d)
					continue;
				DoorPlace dp = PlaceDoor(plan, door);
				if(!dp.ok || LiftOf(plan, dp) != room.letter)
					continue;
				stops << LiftStopName(plan, room.letter, d);
				stopDecks << d;
				V2 n(dp.nx, dp.ny);
				if(dp.there == room.letter)
					n = n * -1;
				for(int s = 0; s < 4; s++)
					if(n.Dot(outward[s]) > 0.7)
						doorSides.insert(s);
				break;
			}
		if(stops.isEmpty())
		{
			log << "  " << room.name << " has no door on any deck, so no stops: its shaft is built with no car in it.\n";
			continue;
		}

		QRect box;
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
				if(plan.At(D, QPoint(c, r)) == room.letter)
					box |= QRect(c, r, 1, 1);
		double wallIn[4] = {0, 0, 0, 0}, voidIn[4] = {0, 0, 0, 0};
		for(int r = box.top(); r <= box.bottom(); r++)
			for(int c = box.left(); c <= box.right(); c++)
				for(int s = 0; s < 4; s++)
				{
					QPoint cell(c, r);
					if(facing(cell, Side(s)) == room.letter)
						continue;
					wallIn[s] = qMax(wallIn[s], thickness(facing(cell, Side(s))));
					voidIn[s] = qMax(voidIn[s], voidInset(cell, Side(s)));
				}
		const double x0 = plan.X0(box.left()), x1 = plan.X1(box.right()), y0 = plan.Y0(box.bottom()), y1 = plan.Y1(box.top());
		const double gap = 0.03;
		const double cx0 = x0 + wallIn[West] + gap, cx1 = x1 - wallIn[East] - gap;
		const double cy0 = y0 + wallIn[South] + gap, cy1 = y1 - wallIn[North] - gap;

		AuScFaceList floor, roof, posts, rails, lamp;
		AddBox(floor, cx0, cy0, -0.1, cx1, cy1, 0.02);
		AddBox(roof, cx0, cy0, HC - 0.02, cx1, cy1, HC + 0.1);
		const double p = 0.07;
		for(double x : {cx0, cx1 - p})
			for(double y : {cy0, cy1 - p})
				AddBox(posts, x, y, 0.02, x + p, y + p, HC - 0.02);
		const double rz = 0.95, rt = 0.04, ro = 0.06;
		if(!doorSides.contains(North))
			AddBox(rails, cx0 + p, cy1 - ro - rt, rz, cx1 - p, cy1 - ro, rz + rt);
		if(!doorSides.contains(South))
			AddBox(rails, cx0 + p, cy0 + ro, rz, cx1 - p, cy0 + ro + rt, rz + rt);
		if(!doorSides.contains(East))
			AddBox(rails, cx1 - ro - rt, cy0 + p, rz, cx1 - ro, cy1 - p, rz + rt);
		if(!doorSides.contains(West))
			AddBox(rails, cx0 + ro, cy0 + p, rz, cx0 + ro + rt, cy1 - p, rz + rt);
		const double mx = (cx0 + cx1) / 2, my = (cy0 + cy1) / 2, lw = qMin(0.5, (cx1 - cx0) / 3), ld = qMin(0.5, (cy1 - cy0) / 3);
		AddBox(lamp, mx - lw, my - ld, HC - 0.035, mx + lw, my + ld, HC - 0.02);
		const QColor metal = theme == Stone ? QColor(0x3e, 0x3e, 0x44) : theme == Metal ? QColor(0x9a, 0xa2, 0xaa)
			: theme == Wood ? QColor(0x8a, 0x5a, 0x32) : QColor(0xb0, 0xb0, 0xb0);
		Paint(floor, theme == Wood ? QColor(0x6a, 0x48, 0x2a) : QColor(0x55, 0x58, 0x5e));
		Paint(roof, QColor(0xc8, 0xc8, 0xc0));
		Paint(posts, metal);
		Paint(rails, theme == Metal ? QColor(0xd8, 0xb0, 0x20) : metal);
		Paint(lamp, QColor(0xff, 0xf4, 0xd0));

		AuScFaceList inside;
		AddBox(inside, x0 + voidIn[West], y0 + voidIn[South], VF, x1 - voidIn[East], y1 - voidIn[North], VC);
		inside.SetOpacity(VOID_OPACITY);

		AuScLift lift;
		lift.SetGravity(G_Natural, AuPoint(0, 0, 1));
		for(AuScFaceList* list : {&floor, &roof, &posts, &rails, &lamp})
			lift.AddFaces(*list);
		lift.AddState(AuScLift::RoomVoid);
		lift.AddFaces(inside);
		lift.SetState(AuScLift::Car);
		for(int i = 0; i < stops.size(); i++)
		{
			AuMatrix4x4 up;
			up.Translate(AuPoint(0, 0, (stopDecks[i] - D) * H * FeetPerMeter()));
			lift.AddStop(stops[i]);
			lift.SetStop(stops[i]);
			lift.TransformLiftStop(up);
		}
		lift.SetStop(stops.first());
		lift.SetGroupName(room.name);
		if(add(lift, QString("turbo lift %1").arg(room.name)))
		{
			log << "  " << room.name << ": stops " << stops.join(", ") << ".\n";
		}
	}
}

}
