#pragma once

#include <QWidget>

#include "tilegrid.h"

class GridView : public QWidget
{
	Q_OBJECT

public:
	enum Tool
	{
		PaintRoom,
		Erase,
		DeleteRoom,
		HullDoor,
		InteriorDoor,
		WideHullDoor,
		WideInteriorDoor,
		PlaceHatch,
		CitizenPost,
		TroopPost,
		EraseDetail,
		PlaceConsole,
		PlaceWindow,
		RoofGable,
		RoofHip,
		RoofRound,
		WallOpening,
		HullOpening,
		PlaceRailing,
		PlaceStairStraight,
		PlaceStairTurnLeft,
		PlaceStairTurnRight,
		PlaceStairULeft,
		PlaceStairURight,
		PlaceDormer,
		PlaceSkylight,
		PlaceGap,
		PlaceBerth,
		PlaceParking,
		PlaceLight,
		PlaceLightPole,
		PlaceFlagPole,
		PlaceGameTable,
		PlaceTurret,
		PlaceStation,
		PlaceHullHatch,
	};

	explicit GridView(SimpleMode::Plan& plan, QWidget* parent = nullptr);

	void SetTool(Tool tool);
	Tool CurrentTool() const { return m_Tool; }
	bool DoorTool() const { return HullTool() || m_Tool == InteriorDoor || m_Tool == WideInteriorDoor || m_Tool == WallOpening; }
	bool RoofTool() const { return m_Tool == RoofGable || m_Tool == RoofHip || m_Tool == RoofRound; }
	bool StairTool() const { return m_Tool >= PlaceStairStraight && m_Tool <= PlaceStairURight; }
	void SetCurrentRoom(QChar letter, const SimpleMode::Room& pending = SimpleMode::Room());
	void SetDeck(int deck);
	int Deck() const { return m_Deck; }
	void SetLine(SimpleMode::Line line, bool on);
	bool LineOn(SimpleMode::Line line) const { return m_Lines[line]; }
	const bool* Lines() const { return m_Lines; }

	int SelectedDoor() const;
	void SelectDoor(int index);
	int SelectedHatch() const;
	void SelectHatch(int index);
	int SelectedWindow() const;
	void SelectWindow(int index);
	int SelectedRailing() const;
	void SelectRailing(int index);
	int SelectedStation() const;
	void SelectStation(int index);
	bool FixtureTool() const { return m_Tool >= PlaceBerth && m_Tool <= PlaceTurret; }
	QString FixtureToolType() const;
	int SelectedFixture() const;
	void SelectFixture(int index);
	SimpleMode::Fixture& FixtureTemplate(const QString& type);
	SimpleMode::Station& StationTemplate() { return m_StationTemplate; }
	int SelectedStair() const;
	void SelectStair(int index);
	void TurnStairIndex(int index);
	QChar SelectedRoof() const;
	void SelectRoof(QChar letter);
	int SelectedDormer() const;
	void SelectDormer(int index);
	void TurnDormerIndex(int index);
	int SelectedSkylight() const;
	void SelectSkylight(int index);

	void PlanResized();

	QSize sizeHint() const override;

	static QString RoomLetters();
	static QColor RoomColor(QChar letter);

signals:
	void EditStarting();
	void EditRefused();
	void PlanChanged();
	void Status(const QString& text);
	void DoorSelected(int index);
	void HatchSelected(int index);
	void WindowSelected(int index);
	void RoofSelected(QChar letter);
	void RailingSelected(int index);
	void StationSelected(int index);
	void FixtureSelected(int index);
	void StairSelected(int index);
	void DormerSelected(int index);
	void SkylightSelected(int index);
	void ToolSet(int tool);
	void ToolChanged(int tool);

protected:
	void leaveEvent(QEvent* e) override;
	void mouseMoveEvent(QMouseEvent* e) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;
	void paintEvent(QPaintEvent* e) override;

private:
	QPointF LatticePx(const QPoint& p) const;
	bool QuarterAtPx(const QPoint& px, QPoint& cell, int& quarter) const;
	bool CellAt(const QPoint& px, QPoint& cell) const;
	bool DoorAtPx(const QPoint& px, SimpleMode::Door& door) const;
	int FindDoor(const SimpleMode::Door& door) const;
	bool WindowAtPx(const QPoint& px, SimpleMode::Window& window) const;
	int FindWindow(const SimpleMode::Window& window) const;
	QString Describe(const QPoint& cell, int quarter) const;

	bool HullTool() const { return m_Tool == HullDoor || m_Tool == WideHullDoor || m_Tool == HullOpening; }
	bool OpeningTool() const { return m_Tool == WallOpening || m_Tool == HullOpening; }
	bool RailingAtPx(const QPoint& px, SimpleMode::Railing& railing) const;
	bool GapAtPx(const QPoint& px, SimpleMode::RailingGap& gap) const;
	int FindGapAt(const QPoint& px) const;
	void ClickGap(const QPoint& px);
	void FlipGapWidth(const QPoint& px);
	int FindRailing(const SimpleMode::Railing& railing) const;
	void ToggleRailing(const SimpleMode::Railing& railing);
	void CycleRailingStyle(const SimpleMode::Railing& railing);
	bool WideTool() const { return m_Tool == WideHullDoor || m_Tool == WideInteriorDoor; }
	bool DoorKindFits(const SimpleMode::DoorPlace& place, QString& problem) const;

	void Paint(const QPoint& cell, int quarter);
	void RemoveRoom(QChar letter);
	void EraseDetailAt(const QPoint& px);
	void ToggleAutomatic(const SimpleMode::Door& door);
	void ToggleDoor(const SimpleMode::Door& door);
	void ToggleHatch(const QPoint& cell);
	void ToggleRoofHatch(const QPoint& cell);
	bool HatchTool() const { return m_Tool == PlaceHatch || m_Tool == PlaceHullHatch; }
	void ToggleWindow(const SimpleMode::Window& window);
	void CycleWindowTech(const SimpleMode::Window& window);
	void PaintRoof(const QPoint& cell);
	int StairAt(const QPoint& cell) const;
	void PlaceStairAt(const QPoint& cell);
	void TurnStair(const QPoint& cell);
	bool StairFits(const SimpleMode::Stair& stair, int ignore, QString& problem) const;
	void TurnRoof(const QPoint& cell);
	int DormerAt(const QPoint& cell) const;
	int SkylightIn(const QPoint& cell) const;
	int SideAtPx(const QPoint& px, const QPoint& cell) const;
	void PlaceDormerAt(const QPoint& cell, int side);
	void PlaceSkylightAt(const QPoint& cell);
	void CycleSkylight(const QPoint& cell);
	void TogglePost(const QPoint& cell, const QString& kind);
	int FixtureAt(const QPoint& cell, const QString& type, int dir) const;
	void ClickFixture(const QPoint& px);
	void TurnFixture(const QPoint& px);
	int StationAt(const QPoint& cell) const;
	void ClickStation(const QPoint& px);
	void TurnStation(const QPoint& cell);

	SimpleMode::Plan& m_Plan;
	Tool m_Tool = PaintRoom;
	Tool m_ToolBeforeConsole = InteriorDoor;
	bool m_HasSelection = false;
	SimpleMode::Door m_Selection;
	bool m_HasHatchSelection = false;
	int m_HatchDeck = 0;
	QPoint m_HatchCell;
	bool m_HatchRoof = false;
	bool m_HasWindowSelection = false;
	SimpleMode::Window m_WindowSelection;
	QChar m_RoofSelection;
	bool m_HasStairSelection = false;
	SimpleMode::Stair m_StairSelection;
	bool m_HasFixtureSelection = false;
	SimpleMode::Fixture m_FixtureSelection{0, QPoint()};
	QMap<QString, SimpleMode::Fixture> m_FixtureTemplates;
	bool m_HasStationSelection = false;
	int m_StationSelectionDeck = 0;
	QPoint m_StationSelectionCell;
	SimpleMode::Station m_StationTemplate{0, QPoint(), SimpleMode::North};
	bool m_HasRailingSelection = false;
	SimpleMode::Railing m_RailingSelection;
	bool m_HasDormerSelection = false;
	int m_DormerDeck = 0;
	QPoint m_DormerCell;
	bool m_HasSkylightSelection = false;
	int m_SkylightDeck = 0;
	QPoint m_SkylightCell;
	bool m_HoverRailingValid = false;
	SimpleMode::Railing m_HoverRailing;
	bool m_HoverGapValid = false;
	SimpleMode::RailingGap m_HoverGap;
	QChar m_RoofStroke;
	QChar m_Room;
	SimpleMode::Room m_PendingRoom;
	int m_Deck = 0;
	bool m_Lines[4] = {true, true, false, false};
	bool m_Painting = false;

	bool m_HoverValid = false;
	QPoint m_HoverCell;
	int m_HoverQuarter = 0;
	bool m_HoverDoorValid = false;
	SimpleMode::Door m_HoverDoor;
	bool m_HoverWindowValid = false;
	SimpleMode::Window m_HoverWindow;

	static const int CellPx = 48;
	static const int Margin = 26;
};

void TidyPlan(SimpleMode::Plan& plan);

QChar NewRoof(SimpleMode::Plan& plan, const QString& style, int dir = SimpleMode::East);

int RoofTheBuilding(SimpleMode::Plan& plan);
