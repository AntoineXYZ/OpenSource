//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aubdfinalizedlg.cpp,v 1.11 2022/05/05 16:31:27 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "aubdfinalizedlg.h"
//
#include "auapp.h"
#include "aubddesign.h"
#include "audesign.h"
#include "auglobal.h"
#include "auframe.h"
#include "auscparts.h"
#include <QLabel>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

static const AuBdDesign* SimpleModeDesign(const AuDesign* design)
{
	const AuBdDesign* bd = dynamic_cast<const AuBdDesign*>(design);
	return bd && bd->HasRecipe() ? bd : 0;
}

AuBdFinalizeDlg::AuBdFinalizeDlg(const AuDesign& design, const QImage& thumb, const AuId& archId, const QString& archName,
		QWidget* parent) : AuDialog(parent)
{
	m_DesignId = 0;	// Can be specified by an agent.

	const bool simpleMode = SimpleModeDesign(&design) != 0;
	if(simpleMode)
		setAuDlgTitle(tr("Finalize Simple Mode %1 Blueprint").arg(design.StructureName()));
	else
		setAuDlgTitle(tr("Finalize %1 Blueprint").arg(design.StructureName()));
	setAuDlgIcon(QIcon(":data/ScFinish.png"));
	m_Assembly = false;
	m_Design = design.Clone();
	m_Design->SetId(0);				// Finalized designs are always saved as new. Architect changed as if it was saved/loaded.
	m_Design->ClearArchitectForFileSave(m_Design->ArchitectId());
	m_Design->SetArchitectAfterFileLoad(archId, archName);
	if(!thumb.isNull())
		m_Design->SetThumbnail(thumb);

	QLabel* lbinfo;
	if(auApp->IsSingleUserDesignerMode())
		lbinfo = new QLabel(tr("<html>"
					"<p>"
					"Finalize a building blueprint in the off line designer to analyze the blueprint. "
					"Results are reported as if the blueprint were going to be saved to the library. "
					"</p>"

					"<p>"
					"Finalizing a blueprint does not change the current working blueprint. "
					"</p>"
					"</html>"), this);
	else
		lbinfo = new QLabel(tr("<html>"
					"<p>"
					"Finalizing a building blueprint saves a read-only copy of it to the library. "
					"A blueprint must be finalized before it can be published to the exchange or constructed in the game universe. "
					"<b>Be sure part seals are set as you want them before finalizing.</b> "
					"</p>"

					"<p>"
					"A blueprint does not have to be published to be used. "
					"Use <i>File|Save to Gear...</i> to save a blueprint to a storage device in your gear. "
					"The <i>Construction</i> window lists blueprints in your gear. "
					"</p>"

					"<p>"
					"Finalizing a blueprint does not change the current working blueprint. "
					"</p>"

					"<p>"
					"The screen grab at the right will be saved with the blueprint. "
					"It will be visible in the exchange, if you publish the blueprint. "
					"It will be visible on the construction window, if it is published in the exchange. "
					"</p>"

					"<p>"
					"Use <i>Shift+Esc</i> to hide screen elements if desired, before displaying this window. "
					"</p>"
					"</html>"), this);
	lbinfo->setWordWrap(true);

	QLabel* lbimg = new QLabel(this);
	lbimg->setPixmap(QPixmap::fromImage(thumb));

	m_Verification = new QTextBrowser(this);
	m_Verification->setReadOnly(true);
	m_Verification->setToolTip(tr("<html>"
				"Finalize messages appear here. "
				"</html>"));

	if(simpleMode)
		m_DoDesign = new QPushButton(tr("Finalize Simple Mode %2 '%1'...").arg(design.Name()).arg(design.StructureName()), this);
	else
		m_DoDesign = new QPushButton(tr("Finalize %2 '%1'...").arg(design.Name()).arg(design.StructureName()), this);
	if(auApp->IsSingleUserDesignerMode())
		m_DoDesign->setToolTip(tr("<html>"
					"The blueprint is analyzed to ensure it can be used to construct a building. "
					"Upon success, a confirmation dialog appears before closing this dialog. "
					"</html>"));
	else
		m_DoDesign->setToolTip(tr("<html>"
					"The blueprint is analyzed to ensure it can be used to construct a building. "
					"Upon success, a confirmation dialog appears before the building blueprint is saved into the library. "
					"</html>"));
	connect(m_DoDesign, SIGNAL(clicked()), SLOT(DoSpacecraft()));

	m_DoAssembly = new QPushButton(tr("Finalize Assembly '%1'...").arg(design.Name()), this);
	m_DoAssembly->setToolTip(tr("<html>"
				"The blueprint is analyzed to ensure it can be used as an assembly. "
				"Upon success, a confirmation dialog appears before the blueprint is saved into the library as an "
				"<i>assembly</i>. "
				"<br>"
				"Assemblies can be used as a source of parts when creating other blueprints. "
				"Any portion of a blueprint can be saved for use as an assembly, regardless of its completeness. "
				"</html>"));
	connect(m_DoAssembly, SIGNAL(clicked()), SLOT(DoAssembly()));

	QVBoxLayout* llay = new QVBoxLayout;
	llay->setMargin(0);
	llay->setSpacing(2);
	llay->addWidget(lbinfo);
	llay->addWidget(m_Verification);

	QVBoxLayout* rlay = new QVBoxLayout;
	rlay->setMargin(0);
	rlay->setSpacing(2);
	rlay->addWidget(lbimg);
	rlay->addStretch();
	rlay->addWidget(m_DoAssembly);
	rlay->addWidget(m_DoDesign);

	QHBoxLayout* lay = new QHBoxLayout;
	lay->setMargin(4);
	lay->setSpacing(2);
	lay->addLayout(llay);
	lay->addLayout(rlay);
	setAuDlgLayout(lay);
}

AuBdFinalizeDlg::~AuBdFinalizeDlg()
{
	if(m_Design)
		delete m_Design;
}

void AuBdFinalizeDlg::setVisible(bool visible)
{
	if(visible)
	{
		AuDialog::setVisible(visible);
		RestoreGeometry();
	}
	else if(isVisible())
	{
		SaveGeometry();
		AuDialog::setVisible(visible);
	}
}

void AuBdFinalizeDlg::RestoreGeometry()
{
	int posX = g_Au.Settings().readNumEntry("Widget/FinalizeBuildingDlg/PosX",-123);
	int posY = g_Au.Settings().readNumEntry("Widget/FinalizeBuildingDlg/PosY",-123);
	QSize szdef = sizeHint();
	int sizeX = g_Au.Settings().readNumEntry("Widget/FinalizeBuildingDlg/SizeX",szdef.width());
	int sizeY = g_Au.Settings().readNumEntry("Widget/FinalizeBuildingDlg/SizeY",szdef.height());
	resize(sizeX, sizeY);
	if(posX != -123 && posY != -123)
		move(posX, posY);
}

void AuBdFinalizeDlg::SaveGeometry()
{
	QPoint ps = pos();
	QSize sz = size();
	g_Au.Settings().writeEntry("Widget/FinalizeBuildingDlg/PosX",ps.x());
	g_Au.Settings().writeEntry("Widget/FinalizeBuildingDlg/PosY",ps.y());
	g_Au.Settings().writeEntry("Widget/FinalizeBuildingDlg/SizeX",sz.width());
	g_Au.Settings().writeEntry("Widget/FinalizeBuildingDlg/SizeY",sz.height());
}

void AuBdFinalizeDlg::DoAssembly()
{
	// We will only do this once.
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	m_DoAssembly->setEnabled(false);
	m_DoDesign->setEnabled(false);

	// Start with an initial message.
	m_Verification->setHtml(tr("Working..."));

	// Finalize the assembly.
	QStringList msgs;
	bool ok = m_Design->FinalizeAssembly(msgs);

	// Show the result.
	QString result;
	for(QStringList::const_iterator mi = msgs.cbegin(); mi != msgs.cend(); mi++)
	{
		if(mi != msgs.cbegin())
			result += "<br>";
		result += *mi;
	}
	m_Verification->setHtml(result);
	QApplication::restoreOverrideCursor();

	// Stop on failure.
	if(!ok)
		return;

	// Assembly is ok. See if they really want to save it.
	QString smsg;
	if(auApp->IsSingleUserDesignerMode())
		smsg = tr("<html>"
				"<p>"
				"The building assembly blueprint finalized successfully. "
				"If you were on line, the finalized assembly blueprint could be saved to the library at this time. "
				"</p>"
				"</html>");
	else
		smsg = tr("<html>"
				"<p>"
				"The building assembly blueprint finalized successfully. "
				"Press 'Ok' to save the finalized assembly blueprint to the library. "
				"</p>"
				"</html>");
	AuFrame* frame = auApp->Frame();
	QMessageBox mb(g_Au.ApplicationName(), smsg, QMessageBox::Information, QMessageBox::Ok, QMessageBox::Cancel,
			QMessageBox::NoButton, frame);
	if(frame->ExecMessageBox(mb) != QMessageBox::Ok)
	{
		reject();
		return;
	}

	// That's as far as we go in single-player mode.
	if(auApp->IsSingleUserDesignerMode())
		reject();

	m_Assembly = true;
	accept();
}

void AuBdFinalizeDlg::DoSpacecraft()
{
	// We will only do this once.
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	m_DoAssembly->setEnabled(false);
	m_DoDesign->setEnabled(false);

	// Start with an initial message.
	m_Verification->setHtml(tr("Working..."));

	QProgressDialog progress(tr("Finalizing Building Blueprint..."), tr("Cancel Finalize"), 0, 1, this);
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(0);
	progress.setValue(0);

	const AuBdDesign* simple = SimpleModeDesign(m_Design);
	int planSize = 0, modelSize = 0;
	if(simple)
	{
		QByteArray parts;
		QDataStream out(&parts, QIODevice::WriteOnly);
		out.setVersion(QDataStream::Qt_4_1);
		out << simple->Parts();
		modelSize = qCompress(parts).size();
		planSize = simple->RecipeText().toUtf8().size();
	}

	// Finalize the design.
	QStringList msgs;
	bool ok = m_Design->FinalizeDesign(msgs, progress);
	if(simple)
		msgs.prepend(tr("<b>Simple Mode blueprint.</b> It is stored as its plan (%1 KB) instead of its full model (%2 KB "
				"compressed). The server builds it again from the plan, checks it, and adds it to the library; "
				"everyone who sees it builds it from the plan too.")
				.arg(qMax(1, (planSize + 1023) / 1024)).arg(qMax(1, (modelSize + 1023) / 1024)));
	if(progress.wasCanceled())
		msgs.append(tr("Finalize canceled."));

	// Show the result.
	QString result;
	for(QStringList::const_iterator mi = msgs.cbegin(); mi != msgs.cend(); mi++)
	{
		if(mi != msgs.cbegin())
			result += "<br>";
		result += *mi;
	}
	m_Verification->setHtml(result);
	QApplication::restoreOverrideCursor();
	progress.cancel();

	// Stop on failure.
	if(!ok)
		return;

	// Design is ok. See if they really want to save it.
	QString smsg;
	if(auApp->IsSingleUserDesignerMode())
		smsg = tr("<html>"
				"<p>"
				"The building blueprint finalized successfully. "
				"If you were on line, the finalized building blueprint could be saved to the library at this time. "
				"</p>"
				"</html>");
	else if(simple)
		smsg = tr("<html>"
				"<p>"
				"The Simple Mode building blueprint finalized successfully. "
				"Press 'Ok' to save it to the library as its plan. "
				"</p>"
				"</html>");
	else
		smsg = tr("<html>"
				"<p>"
				"The building blueprint finalized successfully. "
				"Press 'Ok' to save the finalized building blueprint to the library. "
				"</p>"
				"</html>");
	AuFrame* frame = auApp->Frame();
	QMessageBox mb(g_Au.ApplicationName(), smsg, QMessageBox::Information, QMessageBox::Ok, QMessageBox::Cancel,
			QMessageBox::NoButton, frame);
	if(frame->ExecMessageBox(mb) != QMessageBox::Ok)
	{
		reject();
		return;
	}

	// That's as far as we go in single-player mode.
	if(auApp->IsSingleUserDesignerMode())
		reject();

	m_Assembly = false;
	accept();
}

QString AuBdFinalizeDlg::Report() const
{
	return m_Verification->toHtml();
}
