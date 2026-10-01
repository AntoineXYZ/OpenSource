#ifndef AuSimpleDock_H
#define AuSimpleDock_H

#include "audockwidget.h"
#include <QTimer>
#include <functional>

class EditorWindow;
class QMainWindow;

class AuSimpleModeDock : public AuDockWidget
{
	Q_OBJECT

public:
	AuSimpleModeDock(QMainWindow* frame, const std::function<void(const QString&)>& apply);

	Q_INVOKABLE QString PlanText() const;

public slots:
	void LoadRecipe(const QString& recipe);
	void NewBuilding();

protected:
	void closeEvent(QCloseEvent* e) override;

private slots:
	void PlanChanged();
	void Apply();
	void ShowStatus(const QString& text);
	void ShowDeck(const QString& text);

private:
	EditorWindow*							m_Editor;
	QTimer									m_Timer;
	std::function<void(const QString&)>		m_Apply;
};

#endif
