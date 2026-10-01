#pragma once

#include <QIcon>
#include <QMap>
#include <QWidget>

#include "gridview.h"
#include "tilegrid.h"

class QApplication;
class QButtonGroup;
class QLabel;
class QToolButton;

QIcon PaletteIcon(const QString& name);
QString RoomKindIcon(const QString& key);
void ApplyDesignerTheme(QApplication& app);

class DesignPanel : public QWidget
{
	Q_OBJECT

public:
	enum Action { DeckAbove, DeckBelow, DeleteDeck };

	explicit DesignPanel(QWidget* parent = nullptr);

	void SetLines(const bool lines[4]);
	void SetTool(GridView::Tool tool);
	void SetRoom(const QString& type, int tag);
	void SetTheme(const QString& key);
	QString RoomType() const { return m_Type; }
	int Tag() const { return m_Tag; }

signals:
	void ActionTriggered(int action);
	void ThemeChosen(const QString& key);
	void LineToggled(SimpleMode::Line line, bool on);
	void ToolChosen(int tool);
	void RoomChosen(const QString& type, int tag);

private:
	QToolButton* Button(QWidget* parent, const QString& icon, const QString& tip, bool enabled = true);
	QToolButton* ToolButton(QWidget* parent, GridView::Tool tool, const QString& icon, const QString& tip);
	void ChangeTag(int step);

	QButtonGroup* m_Tools = nullptr;
	QButtonGroup* m_Types = nullptr;
	QMap<SimpleMode::Line, QToolButton*> m_LineButtons;
	QMap<QString, QToolButton*> m_TypeButtons;
	QLabel* m_TagLabel = nullptr;
	class QComboBox* m_Theme = nullptr;
	QString m_Type = "hall";
	int m_Tag = 1;
};
