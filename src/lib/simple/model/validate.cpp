#include "../build/internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode {

bool CheckPlan(const Plan& plan, QTextStream& log)
{
	if(!plan.Decks() || !plan.Rows() || !plan.Cols())
	{
		log << "The grid is empty.\n";
		return false;
	}
	for(const QStringList& grid : plan.decks)
	{
		if(grid.size() != plan.Rows())
		{
			log << "Every deck must have the same number of rows.\n";
			return false;
		}
		for(const QString& row : grid)
			if(row.size() != plan.Cols() * 4)
			{
				log << "Grid rows must all be the same width.\n";
				return false;
			}
	}
	QMap<QString, QChar> names;
	for(const Room& room : plan.rooms)
	{
		const RoomKind* kind = FindRoomKind(room.type);
		if(!kind)
		{
			log << "Room " << room.name << " has an unknown type " << room.type << ".\n";
			return false;
		}
		bool painted = false;
		for(const QStringList& grid : plan.decks)
			for(const QString& row : grid)
				painted = painted || row.contains(room.letter);
		if(!painted)
			continue;
		if(!kind->buildable)
		{
			log << "Room " << room.name << " is a " << kind->name << ", which cannot be built yet.\n";
			return false;
		}
		if(names.contains(room.name.toLower()))
		{
			log << "Two rooms are called " << room.name << "; every room needs its own name.\n";
			return false;
		}
		names.insert(room.name.toLower(), room.letter);
	}
	for(const QStringList& grid : plan.decks)
		for(const QString& row : grid)
			for(QChar ch : row)
				if(ch != '.' && !plan.rooms.contains(ch))
				{
					log << "The grid uses room letter " << ch << " but there is no 'room " << ch << " ...' line.\n";
					return false;
				}
	for(const Room& room : plan.rooms)
	{
		QString problem;
		if(!RoomShapeOk(plan, room.letter, problem))
		{
			log << problem << "\n";
			return false;
		}
		problem = IsLiftRoom(plan, room.letter) ? LiftProblem(plan, room.letter) : QString();
		if(!problem.isEmpty())
		{
			log << problem << "\n";
			return false;
		}
	}
	for(int i = 0; i < plan.stairs.size(); i++)
		for(int j = i + 1; j < plan.stairs.size(); j++)
		{
			if(plan.stairs[i].deck != plan.stairs[j].deck)
				continue;
			QList<QPoint> a = LayoutStair(plan, plan.stairs[i]).cells, b = LayoutStair(plan, plan.stairs[j]).cells;
			for(const QPoint& c : a)
				if(b.contains(c))
				{
					log << "Two stairs on deck " << plan.stairs[i].deck << " share cell " << c.x() << "," << c.y() << ".\n";
					return false;
				}
		}
	for(const Room& room : plan.rooms)
	{
		if(room.type != "opentobelow")
			continue;
		QChar below;
		for(int d = 0; d < plan.Decks(); d++)
			for(int r = 0; r < plan.Rows(); r++)
				for(int c = 0; c < plan.Cols(); c++)
					for(int q = 0; q < 4; q++)
					{
						QPoint cell(c, r);
						if(plan.Quarter(d, cell, q) != room.letter)
							continue;
						if(!IsOpenQuarter(plan, d, cell, q))
						{
							log << room.name << " (Open to Deck Below) at " << c << "," << r << " on deck " << d
								<< " must be over a room on the deck below.\n";
							return false;
						}
						QChar space = SpaceRoom(plan, d, cell, q);
						if(!below.isNull() && space != below)
						{
							log << room.name << " (Open to Deck Below) is over two different rooms; it must be over one.\n";
							return false;
						}
						if(IsOpenAirRoom(plan, space))
						{
							log << room.name << " (Open to Deck Below) is over " << plan.rooms[space].name << ", which is open to the "
								"air: there is nothing to open up.\n";
							return false;
						}
						below = space;
					}
	}
	return true;
}

}
