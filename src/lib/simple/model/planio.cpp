#include "planio.h"

#include <QColor>
#include "../build/internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode {

bool ParsePlanText(const QString& text, Plan& plan, QTextStream& log)
{
	plan = Plan();
	QMap<int, QStringList> grids, roofGrids;
	struct Split { int deck; QPoint cell; QString quarters; };
	QList<Split> splits;
	int deck = 0;
	QStringList* block = nullptr;
	int lineNo = 0;
	for(QString line : text.split('\n'))
	{
		line = line.trimmed();
		lineNo++;
		int hash = line.indexOf('#');
		if(hash >= 0)
			line = line.left(hash).trimmed();
		if(line.isEmpty())
			continue;
		if(block)
		{
			if(line == "end")
				block = nullptr;
			else
				*block << line;
			continue;
		}
		QStringList w = line.split(QRegularExpression("\\s+"));
		QString key = w[0].toLower();
		if(key == "grid")
		{
			if(grids.contains(deck))
			{
				log << "Line " << lineNo << ": deck " << deck << " already has a grid.\n";
				return false;
			}
			block = &grids[deck];
		}
		else if(key == "roofs")
		{
			if(roofGrids.contains(deck))
			{
				log << "Line " << lineNo << ": deck " << deck << " already has its roofs.\n";
				return false;
			}
			block = &roofGrids[deck];
		}
		else if(key == "roof" && w.size() >= 3 && w[1].size() == 1)
		{
			Roof roof;
			roof.letter = w[1][0];
			roof.style = w[2].toLower();
			if(!FindRoofStyle(roof.style))
			{
				QStringList keys;
				for(const RoofStyle& k : RoofStyles())
					keys << k.key;
				log << "Line " << lineNo << ": unknown roof style " << w[2] << ". Known: " << keys.join(", ") << "\n";
				return false;
			}
			static const QMap<QString, int> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
			for(const QString& option : w.mid(3))
			{
				QString o = option.toLower(), value = o.section('=', 1);
				if(o.startsWith("ridge=") && sides.contains(value))
					roof.dir = sides[value] == South ? North : sides[value] == West ? East : sides[value];
				else if(o.startsWith("low=") && sides.contains(value))
					roof.dir = sides[value];
				else if(o.startsWith("pitch=") && value.toDouble() >= 5 && value.toDouble() <= 80)
					roof.pitch = value.toDouble();
				else
				{
					log << "Line " << lineNo << ": roof option " << option << " not understood. Options: ridge=<N|E>, "
						<< "low=<N|E|S|W>, pitch=<5 to 80 degrees>\n";
					return false;
				}
			}
			plan.roofs.insert(roof.letter, roof);
		}
		else if(key == "deck" && w.size() == 2)
		{
			bool ok;
			deck = w[1].toInt(&ok);
			if(!ok || deck < 0 || deck > 63)
			{
				log << "Line " << lineNo << ": deck must be a number from 0 to 63.\n";
				return false;
			}
		}
		else if(key == "name" && w.size() >= 2)
			plan.name = QStringList(w.mid(1)).join(' ');
		else if(key == "industry" && w.size() == 2)
			plan.industry = w[1].toInt();
		else if(key == "cell" && w.size() == 2)
			plan.cell = w[1].toDouble();
		else if(key == "height" && w.size() == 2)
			plan.height = w[1].toDouble();
		else if(key == "wall" && w.size() == 2)
			plan.wall = w[1].toDouble();
		else if(key == "partition" && w.size() == 2)
			plan.partition = w[1].toDouble();
		else if(key == "voidgap" && w.size() == 2)
			plan.voidGap = w[1].toDouble();
		else if(key == "use")
			;
		else if(key == "interiors" && w.size() == 2 && (w[1].toLower() == "yes" || w[1].toLower() == "no"))
			plan.interiors = w[1].toLower() == "yes";
		else if(key == "lowpoly" && w.size() == 2 && (w[1].toLower() == "yes" || w[1].toLower() == "no"))
			plan.lowPoly = w[1].toLower() == "yes";
		else if(key == "texturescale" && w.size() >= 2)
		{
			for(const QString& word : w.mid(1))
			{
				const QString o = word.toLower(), what = o.section('=', 0, 0);
				bool ok = false;
				const int value = o.section('=', 1).toInt(&ok);
				if(!ok || value < 10 || value > 1000 || !QStringList{"outside", "roofing", "outsidetrim"}.contains(what))
				{
					log << "Line " << lineNo << ": texturescale " << word << " not understood. texturescale [outside=<percent>] "
						<< "[roofing=<percent>] [outsidetrim=<percent>], 10 to 1000\n";
					return false;
				}
				plan.scales[what] = value;
			}
		}
		else if(key == "outsidetrim" && w.size() >= 2)
		{
			for(const QString& word : w.mid(1))
			{
				const QString o = word.toLower(), value = o.section('=', 1);
				bool ok = true;
				if(!o.contains('=') && (o == "none" || (o != "plain" && FindThemeKind(o))))
					plan.outsideTrim = o;
				else if(o.startsWith("texture=") && FindTextureKind(value))
					plan.outsideTrimTexture = value;
				else if(o.startsWith("color=") && value.size() == 6 && QColor::isValidColor("#" + value))
					plan.outsideTrimColor = QColor("#" + value).name();
				else if(o.startsWith("strength="))
					plan.outsideTrimStrength = qBound(0, value.toInt(&ok), 100);
				else
					ok = false;
				if(!ok)
				{
					log << "Line " << lineNo << ": outsidetrim " << word << " not understood. outsidetrim [none|wood|stone|metal] "
						<< "[texture=<texture>] [color=<rrggbb>] [strength=<0-100>]\n";
					return false;
				}
			}
		}
		else if((key == "outside" || key == "roofing") && w.size() == 2)
		{
			if(!FindTextureKind(w[1].toLower()))
			{
				log << "Line " << lineNo << ": " << key << " <texture>: no texture called " << w[1] << "\n";
				return false;
			}
			(key == "outside" ? plan.outside : plan.roofing) = w[1].toLower();
		}
		else if(key == "theme" && w.size() == 2)
		{
			plan.theme = w[1].toLower();
			bool known = false;
			QStringList keys;
			for(const ThemeKind& k : ThemeKinds())
			{
				known = known || k.key == plan.theme;
				keys << k.key;
			}
			if(!known)
			{
				log << "Line " << lineNo << ": unknown theme " << w[1] << ". Known: " << keys.join(", ") << "\n";
				return false;
			}
		}
		else if(key == "room" && w.size() >= 4 && w[1].size() == 1)
		{
			Room r;
			r.letter = w[1][0];
			r.type = w[2].toLower();
			r.tag = 0;
			int slash = r.type.indexOf('/');
			if(slash > 0)
			{
				r.tag = r.type.mid(slash + 1).toInt();
				r.type = r.type.left(slash);
				if(r.tag < 1)
				{
					log << "Line " << lineNo << ": a room's tag is a number from 1 up.\n";
					return false;
				}
			}
			QStringList words = w.mid(3);
			while(!words.isEmpty() && words.last().contains('='))
			{
				QString o = words.takeLast().toLower(), value = o.section('=', 1);
				if(o.startsWith("lights=") && (value == "bright" || value == "dim" || value == "dark"))
					r.lights = value;
				else if(o.startsWith("sunlit=") && (value == "yes" || value == "no"))
					r.sunlit = value == "yes";
				else if(o.startsWith("floor=") && FindTextureKind(value))
					r.floor = value;
				else if(o.startsWith("walls=") && FindTextureKind(value))
					r.walls = value;
				else if(o.startsWith("ceiling=") && FindTextureKind(value))
					r.ceiling = value;
				else if(o.startsWith("trim=") && FindTextureKind(value))
					r.trim = value;
				else if(o.startsWith("design=") && (value == "none" || RoomDesignKinds().contains(value)))
					r.design = value;
				else if(o.section('=', 0, 0).endsWith("color") && QColor::isValidColor("#" + value) && value.size() == 6
						&& QStringList{"floor", "walls", "ceiling", "trim"}.contains(o.section('=', 0, 0).chopped(5)))
					r.colors[o.section('=', 0, 0).chopped(5)] = QColor("#" + value).name();
				else if(o.section('=', 0, 0).endsWith("scale") && value.toInt() >= 10 && value.toInt() <= 1000
						&& QStringList{"floor", "walls", "ceiling", "trim"}.contains(o.section('=', 0, 0).chopped(5)))
					r.scales[o.section('=', 0, 0).chopped(5)] = value.toInt();
				else if(o.section('=', 0, 0).endsWith("strength") && value.toInt() >= 0 && value.toInt() <= 100
						&& QStringList{"floor", "walls", "ceiling", "trim"}.contains(o.section('=', 0, 0).chopped(8)))
					r.strengths[o.section('=', 0, 0).chopped(8)] = value.toInt();
				else
				{
					log << "Line " << lineNo << ": room option " << o << " not understood. Options: lights=bright|dim|dark, "
						<< "sunlit=yes|no, floor=|walls=|ceiling=|trim=<texture>, floorcolor=|wallscolor=|ceilingcolor=|trimcolor=<rrggbb>, "
						<< "floorstrength=|wallsstrength=|ceilingstrength=|trimstrength=<0-100>\n";
					return false;
				}
			}
			r.name = words.join(' ');
			static const QMap<QString, QString> older = {
				{"corridor", "hall"}, {"home", "stateroom"}, {"kitchen", "galley"}, {"office", "hall"}, {"shop", "hold"},
				{"store", "hold"}, {"armory", "barracks"}, {"clinic", "sickbay"},
			};
			if(older.contains(r.type))
				r.type = older[r.type];
			if(!FindRoomKind(r.type))
			{
				QStringList keys;
				for(const RoomKind& k : RoomKinds())
					keys << k.key;
				log << "Line " << lineNo << ": unknown room type " << r.type << ". Known: " << keys.join(", ") << "\n";
				return false;
			}
			plan.rooms.insert(r.letter, r);
		}
		else if(key == "split" && w.size() == 4 && w[3].size() == 4)
			splits << Split{deck, QPoint(w[1].toInt(), w[2].toInt()), w[3]};
		else if(key == "door" && ((w.size() >= 5 && w[1].toLower() == "at") || (w.size() >= 4 && w[1].toLower() != "at")))
		{
			Door d{deck, QPoint(), Horizontal, false};
			QStringList options;
			if(w[1].toLower() == "at")
			{
				static const QMap<QString, Line> lines = {{"h", Horizontal}, {"v", Vertical}, {"/", Slash}, {"\\", Backslash}};
				if(!lines.contains(w[4].toLower()))
				{
					log << "Line " << lineNo << ": door at <x> <y> <H, V, / or \\> [options]\n";
					return false;
				}
				d.at = QPoint(w[2].toInt(), w[3].toInt());
				d.line = lines[w[4].toLower()];
				options = w.mid(5);
			}
			else
			{
				static const QMap<QString, Side> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
				if(!sides.contains(w[3].toLower()))
				{
					log << "Line " << lineNo << ": door side must be N, E, S or W\n";
					return false;
				}
				int c = w[1].toInt(), r = w[2].toInt();
				switch(sides[w[3].toLower()])
				{
				case North: d.at = QPoint(4 * c + 2, 4 * r); d.line = Horizontal; break;
				case South: d.at = QPoint(4 * c + 2, 4 * r + 4); d.line = Horizontal; break;
				case East: d.at = QPoint(4 * c + 4, 4 * r + 2); d.line = Vertical; break;
				case West: d.at = QPoint(4 * c, 4 * r + 2); d.line = Vertical; break;
				}
				options = w.mid(4);
			}
			for(const QString& option : options)
			{
				QString o = option.toLower();
				QString value = o.section('=', 1);
				if(o == "auto")
					d.automatic = true;
				else if(o == "locked")
					d.locked = true;
				else if(o == "vehicles")
					d.vehicles = true;
				else if(o == "wide")
					d.wide = true;
				else if(o.startsWith("style=") && FindDoorStyle(value))
					d.style = value;
				else if(o.startsWith("op=") && (value == "switches" || value == "door" || value == "console"))
					d.operation = value;
				else if(o.startsWith("console=") && value.contains(','))
					d.console = QPoint(value.section(',', 0, 0).toInt(), value.section(',', 1).toInt());
				else
				{
					QStringList keys;
					for(const DoorStyle& s : DoorStyles())
						keys << s.key;
					log << "Line " << lineNo << ": door option " << option << " not understood. Options: auto, locked, vehicles, wide, "
						<< "style=<" << keys.join("|") << ">, op=<switches|door|console>, console=<x>,<y>\n";
					return false;
				}
			}
			plan.doors << d;
		}
		else if(key == "window" && ((w.size() >= 5 && w[1].toLower() == "at") || (w.size() >= 4 && w[1].toLower() != "at")))
		{
			Window win{deck, QPoint(), Horizontal, QString(), 0};
			QStringList options;
			if(w[1].toLower() == "at")
			{
				static const QMap<QString, Line> lines = {{"h", Horizontal}, {"v", Vertical}, {"/", Slash}, {"\\", Backslash}};
				if(!lines.contains(w[4].toLower()))
				{
					log << "Line " << lineNo << ": window at <x> <y> <H, V, / or \\> [options]\n";
					return false;
				}
				win.at = QPoint(w[2].toInt(), w[3].toInt());
				win.line = lines[w[4].toLower()];
				options = w.mid(5);
			}
			else
			{
				static const QMap<QString, Side> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
				if(!sides.contains(w[3].toLower()))
				{
					log << "Line " << lineNo << ": window side must be N, E, S or W\n";
					return false;
				}
				int c = w[1].toInt(), r = w[2].toInt();
				switch(sides[w[3].toLower()])
				{
				case North: win.at = QPoint(4 * c + 2, 4 * r); win.line = Horizontal; break;
				case South: win.at = QPoint(4 * c + 2, 4 * r + 4); win.line = Horizontal; break;
				case East: win.at = QPoint(4 * c + 4, 4 * r + 2); win.line = Vertical; break;
				case West: win.at = QPoint(4 * c, 4 * r + 2); win.line = Vertical; break;
				}
				options = w.mid(4);
			}
			for(const QString& option : options)
			{
				QString o = option.toLower(), value = o.section('=', 1);
				if(o.startsWith("shape=") && FindWindowShape(value))
					win.shape = value;
				else if(o.startsWith("tech=") && value.toInt() >= 1 && value.toInt() <= WindowTechNames().size())
					win.tech = value.toInt();
				else
				{
					QStringList shapes;
					for(const WindowShape& s : WindowShapes())
						shapes << s.key;
					log << "Line " << lineNo << ": window option " << option << " not understood. Options: shape=<" << shapes.join("|")
						<< ">, tech=<1 to " << WindowTechNames().size() << ">\n";
					return false;
				}
			}
			plan.windows << win;
		}
		else if((key == "opening" || key == "railing" || key == "gap")
				&& ((w.size() >= 5 && w[1].toLower() == "at") || (w.size() >= 4 && w[1].toLower() != "at")))
		{
			QPoint at;
			Line line = Horizontal;
			QStringList options;
			if(w[1].toLower() == "at")
			{
				static const QMap<QString, Line> lines = {{"h", Horizontal}, {"v", Vertical}, {"/", Slash}, {"\\", Backslash}};
				if(!lines.contains(w[4].toLower()))
				{
					log << "Line " << lineNo << ": " << key << " at <x> <y> <H, V, / or \\> [options]\n";
					return false;
				}
				at = QPoint(w[2].toInt(), w[3].toInt());
				line = lines[w[4].toLower()];
				options = w.mid(5);
			}
			else
			{
				static const QMap<QString, Side> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
				if(!sides.contains(w[3].toLower()))
				{
					log << "Line " << lineNo << ": " << key << " side must be N, E, S or W\n";
					return false;
				}
				int c = w[1].toInt(), r = w[2].toInt();
				switch(sides[w[3].toLower()])
				{
				case North: at = QPoint(4 * c + 2, 4 * r); line = Horizontal; break;
				case South: at = QPoint(4 * c + 2, 4 * r + 4); line = Horizontal; break;
				case East: at = QPoint(4 * c + 4, 4 * r + 2); line = Vertical; break;
				case West: at = QPoint(4 * c, 4 * r + 2); line = Vertical; break;
				}
				options = w.mid(4);
			}
			if(key == "opening")
			{
				Door d{deck, at, line, false};
				d.opening = true;
				for(const QString& option : options)
				{
					QString o = option.toLower(), value = o.section('=', 1);
					if(o == "wide")
						d.wide = true;
					else if(o.startsWith("style=") && FindOpeningStyle(value))
						d.style = value;
					else
					{
						log << "Line " << lineNo << ": opening option " << option << " not understood. Options: wide, "
							<< "style=<plain|arch|tall|full>\n";
						return false;
					}
				}
				plan.doors << d;
			}
			else if(key == "gap")
			{
				RailingGap gap{deck, at, line};
				for(const QString& option : options)
				{
					if(option.toLower() == "full")
						gap.full = true;
					else
					{
						log << "Line " << lineNo << ": gap option " << option << " not understood. Options: full\n";
						return false;
					}
				}
				plan.gaps << gap;
			}
			else
			{
				Railing railing{deck, at, line, QString()};
				for(const QString& option : options)
				{
					QString o = option.toLower(), value = o.section('=', 1);
					if(o.startsWith("style=") && FindRailingStyle(value))
						railing.style = value;
					else
					{
						QStringList keys;
						for(const RailingStyle& k : RailingStyles())
							keys << k.key;
						log << "Line " << lineNo << ": railing option " << option << " not understood. Options: style=<"
							<< keys.join("|") << ">\n";
						return false;
					}
				}
				plan.railings << railing;
			}
		}
		else if(key == "stair" && w.size() >= 4)
		{
			static const QMap<QString, int> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
			if(!sides.contains(w[3].toLower()))
			{
				log << "Line " << lineNo << ": stair <column> <row> <N, E, S or W> [kind=<kind>] [style=<style>]\n";
				return false;
			}
			Stair stair;
			stair.deck = deck;
			stair.cell = QPoint(w[1].toInt(), w[2].toInt());
			stair.dir = sides[w[3].toLower()];
			for(const QString& option : w.mid(4))
			{
				QString o = option.toLower(), value = o.section('=', 1);
				if(o.startsWith("kind=") && StairKindKeys().contains(value))
					stair.kind = StairKindKeys().indexOf(value);
				else if(o.startsWith("style=") && FindStairStyle(value))
					stair.style = value;
				else
				{
					QStringList styles;
					for(const StairStyle& k : StairStyles())
						styles << k.key;
					log << "Line " << lineNo << ": stair option " << option << " not understood. Options: kind=<"
						<< StairKindKeys().join("|") << ">, style=<" << styles.join("|") << ">\n";
					return false;
				}
			}
			plan.stairs << stair;
		}
		else if(key == "dormer" && w.size() >= 4)
		{
			static const QMap<QString, int> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
			if(!sides.contains(w[3].toLower()))
			{
				log << "Line " << lineNo << ": dormer <column> <row> <N, E, S or W> [style=<style>]\n";
				return false;
			}
			Dormer dormer{deck, QPoint(w[1].toInt(), w[2].toInt()), sides[w[3].toLower()], QString()};
			for(const QString& option : w.mid(4))
			{
				QString o = option.toLower(), value = o.section('=', 1);
				if(o.startsWith("style=") && FindDormerStyle(value))
					dormer.style = value;
				else
				{
					QStringList styles;
					for(const DetailStyle& s : DormerStyles())
						styles << s.key;
					log << "Line " << lineNo << ": dormer option " << option << " not understood. Options: style=<" << styles.join("|")
						<< ">\n";
					return false;
				}
			}
			plan.dormers << dormer;
		}
		else if(key == "skylight" && w.size() >= 3)
		{
			Skylight skylight{deck, QPoint(w[1].toInt(), w[2].toInt()), QString()};
			for(const QString& option : w.mid(3))
			{
				QString o = option.toLower(), value = o.section('=', 1);
				if(o.startsWith("style=") && FindSkylightStyle(value))
					skylight.style = value;
				else
				{
					QStringList styles;
					for(const DetailStyle& s : SkylightStyles())
						styles << s.key;
					log << "Line " << lineNo << ": skylight option " << option << " not understood. Options: style=<"
						<< styles.join("|") << ">\n";
					return false;
				}
			}
			plan.skylights << skylight;
		}
		else if(key == "hatch" && w.size() >= 3)
		{
			Hatch h{deck, QPoint(w[1].toInt(), w[2].toInt()), QString(), QString()};
			for(const QString& option : w.mid(3))
			{
				QString o = option.toLower(), value = o.section('=', 1);
				static const QMap<QString, int> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
				if(o == "roof")
					h.roof = true;
				else if(o.startsWith("style=") && FindHatchStyle(value))
					h.style = value;
				else if(o.startsWith("ladder=") && FindLadderStyle(value))
					h.ladder = value;
				else if(o.startsWith("side=") && sides.contains(value))
					h.side = sides[value];
				else
				{
					QStringList styles, ladders;
					for(const HatchStyle& s : HatchStyles())
						styles << s.key;
					for(const HatchStyle& s : LadderStyles())
						ladders << s.key;
					log << "Line " << lineNo << ": hatch option " << option << " not understood. Options: roof, style=<" << styles.join("|")
						<< ">, ladder=<" << ladders.join("|") << ">, side=<N|E|S|W>\n";
					return false;
				}
			}
			plan.hatches << h;
		}
		else if((key == "berth" || key == "parking" || key == "light" || key == "flagpole" || key == "gametable" || key == "turret")
					&& w.size() >= 4
				|| key == "lightpole" && w.size() >= 3)
		{
			static const QMap<QString, int> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}, {"top", FixtureTop}};
			Fixture fx{deck, QPoint(w[1].toInt(), w[2].toInt())};
			fx.type = key;
			QStringList options = w.mid(key == "lightpole" ? 3 : 4);
			if(key != "lightpole")
			{
				if(!sides.contains(w[3].toLower()) || (w[3].toLower() == "top" && key != "light"))
				{
					log << "Line " << lineNo << ": " << key << " <column> <row> <N|E|S|W" << (key == "light" ? "|top" : "") << "> [options]\n";
					return false;
				}
				fx.dir = sides[w[3].toLower()];
			}
			for(const QString& option : options)
			{
				QString o = option.toLower(), value = o.section('=', 1);
				bool ok = true;
				if(o.startsWith("kind="))
					fx.kind = value;
				else if(o.startsWith("color=") && (key == "light" || key == "lightpole"))
					fx.color = value;
				else if(o.startsWith("height=") && key != "berth" && key != "parking")
					fx.height = value.toDouble(&ok);
				else
					ok = false;
				if(!ok)
				{
					log << "Line " << lineNo << ": " << key << " option " << option << " not understood.\n";
					return false;
				}
			}
			plan.fixtures << fx;
		}
		else if(key == "station" && w.size() >= 4)
		{
			static const QMap<QString, int> sides = {{"n", North}, {"e", East}, {"s", South}, {"w", West}};
			if(!sides.contains(w[3].toLower()))
			{
				log << "Line " << lineNo << ": station <column> <row> <N|E|S|W> [kind=command]\n";
				return false;
			}
			Station st{deck, QPoint(w[1].toInt(), w[2].toInt()), sides[w[3].toLower()]};
			for(const QString& option : w.mid(4))
			{
				QString o = option.toLower(), value = o.section('=', 1);
				bool ok = true;
				if(o.startsWith("kind=") && FindStationKind(value))
					st.kind = value;
				else if(o == "wall")
					st.wall = true;
				else if(o.startsWith("module="))
					st.module = value;
				else if(o.startsWith("pose=") && (value == "sit" || value == "stand"))
					st.pose = value == "stand" ? QString() : value;
				else if(o.startsWith("patient=") && (value == "stand" || value == "sit" || value == "prone"))
					st.patient = value == "stand" ? QString() : value;
				else if(o.startsWith("effect="))
					st.effect = qBound(0, value.toInt(&ok), 9);
				else if(o.startsWith("turret="))
					st.turret = qMax(0, value.toInt(&ok));
				else if(o.startsWith("pads="))
				{
					for(const QString& pad : value.split(';', Qt::SkipEmptyParts))
					{
						bool okx, oky;
						QPoint p(pad.section(',', 0, 0).toInt(&okx), pad.section(',', 1, 1).toInt(&oky));
						ok = ok && okx && oky;
						st.pads << p;
					}
				}
				else
					ok = false;
				if(!ok)
				{
					QStringList kinds;
					for(const StationKind& k : StationKinds())
						kinds << k.key;
					log << "Line " << lineNo << ": station option " << option << " not understood. Options: kind=<" << kinds.join("|")
						<< ">, wall, module=<module>, pose=sit|stand, patient=stand|sit|prone, pads=<c>,<r>;..., effect=0-9, turret=<n>\n";
					return false;
				}
			}
			plan.stations << st;
		}
		else if(key == "post" && w.size() == 4)
			plan.posts << Post{deck, QPoint(w[1].toInt(), w[2].toInt()), w[3].toLower()};
		else
		{
			log << "Line " << lineNo << ": not understood: " << line << "\n";
			return false;
		}
	}
	if(grids.isEmpty())
	{
		log << "No grid.\n";
		return false;
	}
	for(Room& r : plan.rooms)
		if(r.tag == 0)
		{
			int tag = 1;
			bool taken = true;
			while(taken)
			{
				taken = false;
				for(const Room& o : plan.rooms)
					taken = taken || (o.letter != r.letter && o.type == r.type && o.tag == tag);
				if(taken)
					tag++;
			}
			r.tag = tag;
		}
	int rows = grids.first().size(), cols = rows ? grids.first().first().size() : 0;
	for(int d = 0; d <= grids.lastKey(); d++)
	{
		if(!grids.contains(d))
		{
			log << "Deck " << d << " has no grid.\n";
			return false;
		}
		const QStringList& grid = grids[d];
		if(grid.size() != rows)
		{
			log << "Every deck must have the same number of rows.\n";
			return false;
		}
		QStringList quarters;
		for(const QString& row : grid)
		{
			if(row.size() != cols)
			{
				log << "Grid rows must all be the same width.\n";
				return false;
			}
			QString q;
			for(QChar ch : row)
				q += QString(4, ch);
			quarters << q;
		}
		plan.decks << quarters;
	}
	for(const Split& s : splits)
	{
		if(s.deck >= plan.Decks() || !plan.OnGrid(s.cell) || plan.At(s.deck, s.cell) != '*')
		{
			log << "split " << s.cell.x() << " " << s.cell.y() << " on deck " << s.deck << ": the grid must have '*' there.\n";
			return false;
		}
		for(int q = 0; q < 4; q++)
			plan.SetQuarter(s.deck, s.cell, q, s.quarters[q]);
	}
	for(int d = 0; d < plan.Decks(); d++)
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
				if(plan.Quarter(d, QPoint(c, r), 0) == '*')
				{
					log << "Cell " << c << "," << r << " on deck " << d << " is '*' but has no split line.\n";
					return false;
				}
	for(auto it = roofGrids.cbegin(); it != roofGrids.cend(); ++it)
	{
		if(it.key() >= plan.Decks() || it.value().size() != rows)
		{
			log << "The roofs of deck " << it.key() << " must have a row for every row of its grid.\n";
			return false;
		}
		for(int r = 0; r < rows; r++)
		{
			const QString& row = it.value()[r];
			if(row.size() != cols)
			{
				log << "The roofs of deck " << it.key() << " must have a character for every cell.\n";
				return false;
			}
			for(int c = 0; c < cols; c++)
			{
				if(row[c] == '.')
					continue;
				if(!plan.roofs.contains(row[c]))
				{
					log << "Roof " << row[c] << " on deck " << it.key() << " has no roof line.\n";
					return false;
				}
				plan.SetRoof(it.key(), QPoint(c, r), row[c]);
			}
		}
	}
	return true;
}

bool ParsePlan(const QString& path, Plan& plan, QTextStream& log)
{
	QFile f(path);
	if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		log << "Cannot read " << path << "\n";
		return false;
	}
	return ParsePlanText(QString::fromUtf8(f.readAll()), plan, log);
}

std::optional<Plan> TryLoadPlan(const QString& path, QTextStream& log)
{
	Plan plan;
	if(!ParsePlan(path, plan, log))
		return std::nullopt;
	return plan;
}

QString PlanToText(const Plan& plan)
{
	static const char* lines[] = {"H", "V", "/", "\\"};
	QString text;
	QTextStream out(&text);
	out << "name " << plan.name << "\n"
		<< "industry " << plan.industry << "\n"
		<< "cell " << plan.cell << "\n"
		<< "height " << plan.height << "\n"
		<< "wall " << plan.wall << "\n"
		<< "partition " << plan.partition << "\n"
		<< "voidgap " << plan.voidGap << "\n"
		<< "theme " << plan.theme << "\n";
	if(!plan.interiors)
		out << "interiors no\n";
	if(plan.lowPoly)
		out << "lowpoly yes\n";
	if(!plan.outside.isEmpty())
		out << "outside " << plan.outside << "\n";
	if(!plan.roofing.isEmpty())
		out << "roofing " << plan.roofing << "\n";
	{
		QString scales;
		for(const QString& what : {"outside", "roofing", "outsidetrim"})
			if(plan.scales.value(what, 100) != 100)
				scales += QString(" %1=%2").arg(what).arg(plan.scales[what]);
		if(!scales.isEmpty())
			out << "texturescale" << scales << "\n";
	}
	if(!plan.outsideTrim.isEmpty() || !plan.outsideTrimTexture.isEmpty() || !plan.outsideTrimColor.isEmpty())
		out << "outsidetrim" << (plan.outsideTrim.isEmpty() ? QString() : " " + plan.outsideTrim)
			<< (plan.outsideTrimTexture.isEmpty() ? QString() : " texture=" + plan.outsideTrimTexture)
			<< (plan.outsideTrimColor.isEmpty() ? QString() : " color=" + plan.outsideTrimColor.mid(1))
			<< (plan.outsideTrimStrength == 50 ? QString() : QString(" strength=%1").arg(plan.outsideTrimStrength)) << "\n";
	auto positionText = [&](int deck, const QPoint& at, Line line) {
		int x = at.x(), y = at.y();
		if(line == Horizontal && x % 4 == 2 && y % 4 == 0)
		{
			QPoint below((x - 2) / 4, y / 4), above((x - 2) / 4, y / 4 - 1);
			if(plan.OnGrid(below) && plan.Quarter(deck, below, North) != '.')
				return QString("%1 %2 N").arg(below.x()).arg(below.y());
			if(plan.OnGrid(above))
				return QString("%1 %2 S").arg(above.x()).arg(above.y());
		}
		if(line == Vertical && y % 4 == 2 && x % 4 == 0)
		{
			QPoint east(x / 4, (y - 2) / 4), west(x / 4 - 1, (y - 2) / 4);
			if(plan.OnGrid(east) && plan.Quarter(deck, east, West) != '.')
				return QString("%1 %2 W").arg(east.x()).arg(east.y());
			if(plan.OnGrid(west))
				return QString("%1 %2 E").arg(west.x()).arg(west.y());
		}
		return QString("at %1 %2 %3").arg(x).arg(y).arg(lines[line]);
	};
	auto doorLine = [&](const Door& d) {
		if(d.opening)
			return "opening " + positionText(d.deck, d.at, d.line) + (d.wide ? " wide" : "")
				+ (d.style.isEmpty() || d.style == "plain" ? QString() : " style=" + d.style);
		QString aut = d.automatic ? " auto" : "";
		if(d.wide)
			aut += " wide";
		if(!d.style.isEmpty())
			aut += " style=" + d.style;
		if(d.operation != "switches")
			aut += " op=" + d.operation;
		if(d.console != QPoint(-1, -1))
			aut += QString(" console=%1,%2").arg(d.console.x()).arg(d.console.y());
		if(d.locked)
			aut += " locked";
		if(d.vehicles)
			aut += " vehicles";
		return "door " + positionText(d.deck, d.at, d.line) + aut;
	};
	auto windowLine = [&](const Window& w) {
		QString options;
		if(!w.shape.isEmpty())
			options += " shape=" + w.shape;
		if(w.tech)
			options += QString(" tech=%1").arg(w.tech);
		return "window " + positionText(w.deck, w.at, w.line) + options;
	};
	auto deckLines = [&](int deck) {
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
			{
				QPoint cell(c, r);
				if(plan.At(deck, cell) != '*')
					continue;
				QString q;
				for(int i = 0; i < 4; i++)
					q += plan.Quarter(deck, cell, i);
				out << "split " << c << " " << r << " " << q << "\n";
			}
		for(const Door& d : plan.doors)
			if(d.deck == deck)
				out << doorLine(d) << "\n";
		for(const Window& w : plan.windows)
			if(w.deck == deck)
				out << windowLine(w) << "\n";
		for(const Stair& st : plan.stairs)
			if(st.deck == deck)
				out << "stair " << st.cell.x() << " " << st.cell.y() << " " << "NESW"[((st.dir % 4) + 4) % 4]
					<< (st.kind == StairStraight ? QString() : " kind=" + StairKindKeys().value(st.kind))
					<< (st.style.isEmpty() ? QString() : " style=" + st.style) << "\n";
		for(const Railing& r : plan.railings)
			if(r.deck == deck)
				out << "railing " << positionText(r.deck, r.at, r.line) << (r.style.isEmpty() ? QString() : " style=" + r.style) << "\n";
		for(const RailingGap& g : plan.gaps)
			if(g.deck == deck)
				out << "gap " << positionText(g.deck, g.at, g.line) << (g.full ? " full" : "") << "\n";
		for(const Hatch& h : plan.hatches)
			if(h.deck == deck)
				out << "hatch " << h.cell.x() << " " << h.cell.y() << (h.roof ? " roof" : "") << (h.style.isEmpty() ? QString() : " style=" + h.style)
					<< (h.ladder.isEmpty() ? QString() : " ladder=" + h.ladder)
					<< (h.side == North ? QString() : QString(" side=") + "NESW"[h.side]) << "\n";
		for(const Post& p : plan.posts)
			if(p.deck == deck)
				out << "post " << p.cell.x() << " " << p.cell.y() << " " << p.kind << "\n";
		for(const Fixture& fx : plan.fixtures)
			if(fx.deck == deck)
				out << fx.type << " " << fx.cell.x() << " " << fx.cell.y()
					<< (fx.type == "lightpole" ? QString() : fx.dir == FixtureTop ? QString(" top") : QString(" ") + "NESW"[((fx.dir % 4) + 4) % 4])
					<< (fx.kind.isEmpty() ? QString() : " kind=" + fx.kind) << (fx.color.isEmpty() ? QString() : " color=" + fx.color)
					<< (fx.height > 0 ? QString(" height=%1").arg(fx.height) : QString()) << "\n";
		for(const Station& st : plan.stations)
			if(st.deck == deck)
			{
				QStringList pads;
				for(const QPoint& p : st.pads)
					pads << QString("%1,%2").arg(p.x()).arg(p.y());
				out << "station " << st.cell.x() << " " << st.cell.y() << " " << "NESW"[((st.dir % 4) + 4) % 4]
					<< (st.kind != "command" ? " kind=" + st.kind : QString()) << (st.wall ? " wall" : "")
					<< (st.module.isEmpty() ? QString() : " module=" + st.module) << (st.pose.isEmpty() ? QString() : " pose=" + st.pose)
					<< (st.patient.isEmpty() ? QString() : " patient=" + st.patient)
					<< (pads.isEmpty() ? QString() : " pads=" + pads.join(';')) << (st.effect ? QString(" effect=%1").arg(st.effect) : QString())
					<< (st.turret ? QString(" turret=%1").arg(st.turret) : QString()) << "\n";
			}
		for(const Dormer& d : plan.dormers)
			if(d.deck == deck)
				out << "dormer " << d.cell.x() << " " << d.cell.y() << " " << "NESW"[((d.side % 4) + 4) % 4]
					<< (d.style.isEmpty() ? QString() : " style=" + d.style) << "\n";
		for(const Skylight& s : plan.skylights)
			if(s.deck == deck)
				out << "skylight " << s.cell.x() << " " << s.cell.y() << (s.style.isEmpty() ? QString() : " style=" + s.style) << "\n";
	};
	bool manyDecks = plan.Decks() > 1;
	for(int d = 0; d < plan.Decks(); d++)
	{
		out << "\n";
		if(manyDecks)
			out << "deck " << d << "\n";
		out << "grid\n";
		for(int r = 0; r < plan.Rows(); r++)
		{
			QString row;
			for(int c = 0; c < plan.Cols(); c++)
				row += plan.At(d, QPoint(c, r));
			out << row << "\n";
		}
		out << "end\n";
		bool roofed = false;
		for(int r = 0; r < plan.Rows(); r++)
			for(int c = 0; c < plan.Cols(); c++)
				roofed = roofed || plan.RoofAt(d, QPoint(c, r)) != '.';
		if(roofed)
		{
			out << "roofs\n";
			for(int r = 0; r < plan.Rows(); r++)
			{
				QString row;
				for(int c = 0; c < plan.Cols(); c++)
					row += plan.RoofAt(d, QPoint(c, r));
				out << row << "\n";
			}
			out << "end\n";
		}
		if(manyDecks)
			deckLines(d);
	}
	out << "\n";
	for(const Room& r : plan.rooms)
	{
		out << "room " << r.letter << " " << r.type << (r.tag > 1 ? QString("/%1").arg(r.tag) : QString()) << " " << r.name
			<< (r.lights != "bright" ? " lights=" + r.lights : QString()) << (r.sunlit ? QString() : QString(" sunlit=no"))
			<< (r.floor.isEmpty() ? QString() : " floor=" + r.floor) << (r.walls.isEmpty() ? QString() : " walls=" + r.walls)
			<< (r.ceiling.isEmpty() ? QString() : " ceiling=" + r.ceiling) << (r.trim.isEmpty() ? QString() : " trim=" + r.trim)
			<< (r.design.isEmpty() ? QString() : " design=" + r.design);
		for(const QString& surface : {"floor", "walls", "ceiling", "trim"})
		{
			if(r.colors.contains(surface))
				out << " " << surface << "color=" << r.colors[surface].mid(1);
			if(r.strengths.contains(surface) && r.strengths[surface] != 50)
				out << " " << surface << "strength=" << r.strengths[surface];
			if(r.scales.contains(surface) && r.scales[surface] != 100)
				out << " " << surface << "scale=" << r.scales[surface];
		}
		out << "\n";
	}
	for(const Roof& r : plan.roofs)
	{
		const RoofStyle* style = FindRoofStyle(r.style);
		out << "roof " << r.letter << " " << r.style;
		if(style && style->ridged)
			out << " ridge=" << (r.dir == North || r.dir == South ? "N" : "E");
		else if(style && style->sloped)
			out << " low=" << "NESW"[r.dir];
		if(r.pitch > 0)
			out << " pitch=" << r.pitch;
		out << "\n";
	}
	if(!manyDecks)
		deckLines(0);
	out.flush();
	return text;
}

}
