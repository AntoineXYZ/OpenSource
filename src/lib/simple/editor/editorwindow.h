#pragma once

#include <QColor>
#include <QList>
#include <QMap>
#include <functional>
#include <QMainWindow>

#include "tilegrid.h"

class DesignPanel;
class GridView;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;

class EditorWindow : public QMainWindow
{
	Q_OBJECT

public:
	EditorWindow();

	void ShowDeck(int deck);
	bool LoadPlanText(const QString& text);
	void StartNew();
	const SimpleMode::Plan& CurrentPlan() const
	{
		return m_Plan;
	}

signals:
	void PlanChanged();
	void Status(const QString& text);
	void DeckShown(const QString& text);

private slots:
	void Undo();
	void Redo();

	void PanelAction(int action);
	void RoomPicked(const QString& type, int tag);
	void RoomListClicked();
	void RoomNameEdited(const QString& name);
	void RoomLightsEdited();
	void TexturesEdited();
	void SetRoomColor(const QString& surface, const QColor& color, int strength);
	void SetOutsideTrimColor(const QColor& color, int strength);
	void SetRoomScale(const QString& surface, int percent);
	void SetBuildingScale(const QString& what, int percent);
	void RefreshStation();
	void RefreshFixture();
	void FixtureEdited();
	void StationEdited();
	void SettingsEdited();
	void ThemeChosen(const QString& key);
	void GridSizeEdited();
	void DeckAbove();
	void DeckBelow();
	void DeleteDeck();

	void RefreshDoor();
	void DoorEdited();
	void RefreshHatch();
	void HatchEdited();
	void RefreshWindow();
	void WindowEdited();
	void RefreshRoof();
	void RoofEdited();
	void RefreshRailing();
	void RailingEdited();
	void RefreshStair();
	void StairEdited();
	void RefreshDormer();
	void DormerEdited();
	void RefreshSkylight();
	void SkylightEdited();
	void UpdatePanels();

	void PushUndo();
	void PlanEdited();

private:
	void BuildUi();
	void ShowStatus(const QString& text, int ms = 0);
	QChar NextLetter() const;
	QChar CurrentRoom() const;
	void RefreshAll();
	void RefreshRooms();
	void RefreshSettings();
	void RefreshDeck();
	void SetModified(bool modified);
	void UpdateActions();

	SimpleMode::Plan m_Plan;
	QList<SimpleMode::Plan> m_Undo;
	QList<SimpleMode::Plan> m_Redo;
	bool m_Refreshing = false;

	GridView* m_View = nullptr;
	DesignPanel* m_Panel = NULL;
	QLabel* m_RoomLabel = nullptr;
	QLineEdit* m_RoomName = nullptr;
	class QComboBox* m_RoomLights = nullptr;
	class QComboBox* m_RoomFloor = nullptr;
	class QComboBox* m_RoomWalls = nullptr;
	class QComboBox* m_RoomCeiling = nullptr;
	class QComboBox* m_RoomTrim = nullptr;
	class QComboBox* m_RoomDesign = nullptr;
	class QComboBox* m_OutsideTrim = nullptr;
	QCheckBox* m_Interiors = nullptr;
	QCheckBox* m_LowPoly = nullptr;
	class QComboBox* m_OutsideTrimTexture = nullptr;
	class QToolButton* m_OutsideTrimColor = nullptr;
	void ShowColor(class QToolButton* button, const QColor& color, int strength, bool usualIsPaint, int scale);
	void AddSizeMenu(class QMenu* menu, const std::function<void(int)>& set);
	class QToolButton* m_OutsideSize = nullptr;
	class QToolButton* m_RoofingSize = nullptr;
	QMap<QString, class QToolButton*> m_RoomColors;
	class QComboBox* m_Outside = nullptr;
	class QComboBox* m_Roofing = nullptr;
	void FillTextures(QComboBox* box, const QString& usual, const QString& picked, bool material);
	class QGroupBox* m_FixtureBox = nullptr;
	QLabel* m_FixtureLabel = nullptr;
	class QComboBox* m_FixtureKind = nullptr;
	class QComboBox* m_FixtureColor = nullptr;
	class QGroupBox* m_StationBox = nullptr;
	QLabel* m_StationLabel = nullptr;
	class QComboBox* m_StationKind = nullptr;
	class QComboBox* m_StationMount = nullptr;
	class QComboBox* m_StationModule = nullptr;
	class QComboBox* m_StationPose = nullptr;
	class QComboBox* m_StationPatient = nullptr;
	class QComboBox* m_StationTurret = nullptr;
	QCheckBox* m_RoomSunlit = nullptr;
	QListWidget* m_Rooms = nullptr;
	class QGroupBox* m_DoorBox = nullptr;
	QLabel* m_DoorLabel = nullptr;
	class QComboBox* m_DoorStyle = nullptr;
	class QComboBox* m_DoorOp = nullptr;
	class QPushButton* m_PlaceConsole = nullptr;
	class QPushButton* m_RemoveDoor = nullptr;
	QCheckBox* m_DoorAuto = nullptr;
	QCheckBox* m_DoorLocked = nullptr;
	QCheckBox* m_DoorVehicles = nullptr;
	QCheckBox* m_DoorWide = nullptr;
	class QGroupBox* m_HatchBox = nullptr;
	QLabel* m_HatchLabel = nullptr;
	class QComboBox* m_HatchStyle = nullptr;
	class QComboBox* m_LadderStyle = nullptr;
	class QGroupBox* m_WindowBox = nullptr;
	QLabel* m_WindowLabel = nullptr;
	class QComboBox* m_WindowShape = nullptr;
	class QComboBox* m_WindowTech = nullptr;
	class QGroupBox* m_DormerBox = nullptr;
	QLabel* m_DormerLabel = nullptr;
	class QComboBox* m_DormerStyle = nullptr;
	class QGroupBox* m_SkylightBox = nullptr;
	QLabel* m_SkylightLabel = nullptr;
	class QComboBox* m_SkylightStyle = nullptr;
	class QGroupBox* m_StairBox = nullptr;
	QLabel* m_StairLabel = nullptr;
	class QComboBox* m_StairKind = nullptr;
	class QComboBox* m_StairStyle = nullptr;
	class QPushButton* m_RoofAll = nullptr;
	class QGroupBox* m_RailingBox = nullptr;
	QLabel* m_RailingLabel = nullptr;
	class QComboBox* m_RailingStyle = nullptr;
	class QGroupBox* m_RoofBox = nullptr;
	QLabel* m_RoofLabel = nullptr;
	class QComboBox* m_RoofStyle = nullptr;
	class QPushButton* m_TurnRoof = nullptr;
	QSpinBox* m_RoofPitch = nullptr;
	class QPushButton* m_RemoveRoof = nullptr;
	QLineEdit* m_Name = nullptr;
	QDoubleSpinBox* m_Cell = nullptr;
	QDoubleSpinBox* m_Height = nullptr;
	QDoubleSpinBox* m_Wall = nullptr;
	QDoubleSpinBox* m_Partition = nullptr;
	QDoubleSpinBox* m_VoidGap = nullptr;
	QSpinBox* m_Cols = nullptr;
	QSpinBox* m_Rows = nullptr;
	QAction* m_UndoAction = nullptr;
	QAction* m_RedoAction = nullptr;
};
