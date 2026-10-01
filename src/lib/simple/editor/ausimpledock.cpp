#include "ausimpledock.h"
#include "editorwindow.h"
#include "../ausimple.h"

#include "aubddesign.h"

#include <QCloseEvent>
#include <QCursor>
#include <QMainWindow>
#include <QToolButton>
#include <QToolTip>

using namespace SimpleMode;

static const char* c_Title = QT_TRANSLATE_NOOP("AuSimpleModeDock", "<b>Simple Mode</b>");

AuSimpleModeDock::AuSimpleModeDock(QMainWindow* frame, const std::function<void(const QString&)>& apply)
	: AuDockWidget(":data/City.png", tr(c_Title), tr("Simple Mode"), frame), m_Apply(apply)
{
	m_Editor = new EditorWindow;
	m_Editor->setWindowFlags(Qt::Widget);
	setWidget(m_Editor);

	if(QWidget* title = titleBarWidget())
		for(QToolButton* b : title->findChildren<QToolButton*>())
			if(b->toolTip().contains("Close"))
				b->hide();

	m_Timer.setSingleShot(true);
	m_Timer.setInterval(500);
	connect(&m_Timer, &QTimer::timeout, this, &AuSimpleModeDock::Apply);
	connect(m_Editor, &EditorWindow::PlanChanged, this, &AuSimpleModeDock::PlanChanged);
	connect(m_Editor, &EditorWindow::Status, this, &AuSimpleModeDock::ShowStatus);
	connect(m_Editor, &EditorWindow::DeckShown, this, &AuSimpleModeDock::ShowDeck);
	m_Editor->ShowDeck(0);
}

void AuSimpleModeDock::LoadRecipe(const QString& recipe)
{
	m_Timer.stop();
	if(!recipe.isEmpty())
		m_Editor->LoadPlanText(recipe);
}

void AuSimpleModeDock::NewBuilding()
{
	m_Timer.stop();
	m_Editor->StartNew();
}

QString AuSimpleModeDock::PlanText() const
{
	return PlanToText(m_Editor->CurrentPlan());
}

void AuSimpleModeDock::closeEvent(QCloseEvent* e)
{
	e->ignore();
}

void AuSimpleModeDock::PlanChanged()
{
	m_Timer.start();
}

void AuSimpleModeDock::Apply()
{
	if(m_Apply)
		m_Apply(PlanToText(m_Editor->CurrentPlan()));
}

void AuSimpleModeDock::ShowStatus(const QString& text)
{
	if(!text.isEmpty() && isVisible())
		QToolTip::showText(QCursor::pos(), text, this);
}

void AuSimpleModeDock::ShowDeck(const QString& text)
{
	SetTitle(QString("%1 %2 %3").arg(tr(c_Title)).arg(QChar(0x00b7)).arg(text.toHtmlEscaped()));
}

namespace {

QDockWidget* CreateDock(QMainWindow* frame, const std::function<void(const QString&)>& apply)
{
	return new AuSimpleModeDock(frame, apply);
}

}

static void InitSimpleModeIcons()
{
	Q_INIT_RESOURCE(ausimpleicons);
}

void AuSimple::RegisterEditor()
{
	InitSimpleModeIcons();
	AuBdDesign::SetSimpleModeDockFactory(&CreateDock);
}
