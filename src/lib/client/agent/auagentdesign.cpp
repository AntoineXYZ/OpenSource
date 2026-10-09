//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auagentdesign.cpp,v 1.7 2022/02/19 18:03:30 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "auagentdesign.h"
//
#include "auapp.h"
#include "auglobal.h"
#include "auframe.h"
#include "aulegacyscprotocol.h"
#include "dadesignbase.h"
#include "dgdata.h"
#include <QGridLayout>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

AuAgentDesign::AuAgentDesign(QWidget* parent) : QWidget(parent)
{
	const DgData& gd = g_Au.LockDataForRead();
	bool isProgrammer = gd.ClientPlayerIsProgrammer();	// also true if architect
	//bool isArchitect = gd.ClientPlayerIsArchitect();
	g_Au.UnlockData();

	QPushButton* pbloadsc = new QPushButton(QIcon(g_Au.CommodityBitmapName(CID_ScDesignDrawing)), tr("Load Spacecraft Design..."));
	connect(pbloadsc, SIGNAL(clicked()), SLOT(LoadScDesign()));
	if(isProgrammer)
		pbloadsc->setToolTip(QString("<html>Loads a spacecraft design given its blueprint number.<br>"
					"%1 Loads the design in a design studio.<br>"
					"%1 Spawns a ship outside a design studio. "
					"You are placed at the helm but a berth is not assigned.</html>")
				.arg(g_Au.MiddleDotSymbol()));
	else
		pbloadsc->setToolTip(QString("<html>Loads a spacecraft design given its blueprint number if you are in a "
					"design studio.</html>"));

	QPushButton* pbloadbd = new QPushButton(QIcon(g_Au.CommodityBitmapName(CID_BdDesignDrawing)), tr("Load Building Design..."));
	connect(pbloadbd, SIGNAL(clicked()), SLOT(LoadBdDesign()));
	pbloadbd->setToolTip(QString("<html>Loads a building design given its blueprint number if you are in a "
				"design studio.</html>"));

	QPushButton* pbdesigner = new QPushButton(QIcon(":data/bDesignStudio.png"), tr("Enter Design Studio"));
	connect(pbdesigner, SIGNAL(clicked()), SLOT(EnterDesigner()));
	pbdesigner->setToolTip(QString("<html>Enters a design studio instance. "
				"You must be on a world. "
				"</html>"));

	// Construct layout.
	QGroupBox* legacy = new QGroupBox(tr("Legacy spacecraft"), this);
	legacy->setVisible(isProgrammer);
	m_LegacyAllowed = isProgrammer;
	m_LegacyFileName = new QLineEdit(legacy);
	m_LegacyFileName->setReadOnly(true);
	m_LegacyFileName->setPlaceholderText(tr("Select a .ship file"));
	m_LegacyBrowse = new QPushButton(tr("Browse..."), legacy);
	m_LegacyName = new QLineEdit(legacy);
	m_LegacyName->setMaxLength(AuLegacyScProtocol::MaximumNameCharacters);
	m_LegacyName->setPlaceholderText(tr("Spacecraft design name"));
	m_LegacyUpload = new QPushButton(tr("Upload design"), legacy);
	m_LegacyDesigns = new QComboBox(legacy);
	m_LegacyRefresh = new QPushButton(tr("Refresh"), legacy);
	m_LegacySpawn = new QPushButton(tr("Spawn nearby"), legacy);
	m_LegacySpawn->setToolTip(tr("Create the selected legacy spacecraft near your avatar."));
	m_LegacyStatus = new QLabel(legacy);
	m_LegacyStatus->setTextFormat(Qt::PlainText);
	m_LegacyStatus->setWordWrap(true);
	m_LegacyTimeout = new QTimer(this);
	m_LegacyTimeout->setSingleShot(true);
	m_LegacyTimeout->setInterval(120000);
	connect(m_LegacyBrowse, SIGNAL(clicked()), SLOT(BrowseLegacyScDesign()));
	connect(m_LegacyUpload, SIGNAL(clicked()), SLOT(UploadLegacyScDesign()));
	connect(m_LegacyRefresh, SIGNAL(clicked()), SLOT(RefreshLegacyScDesigns()));
	connect(m_LegacySpawn, SIGNAL(clicked()), SLOT(SpawnLegacyScDesign()));
	connect(m_LegacyName, SIGNAL(textChanged(QString)), SLOT(UpdateLegacyControls()));
	connect(m_LegacyDesigns, SIGNAL(currentIndexChanged(int)), SLOT(LegacyScSelectionChanged()));
	connect(m_LegacyTimeout, SIGNAL(timeout()), SLOT(LegacyScTimedOut()));
	connect(auApp, SIGNAL(LegacyScReplyReceived(QByteArray)), SLOT(LegacyScReply(QByteArray)));
	connect(auApp, SIGNAL(LegacyScTransportReset()), SLOT(LegacyScDisconnected()));
	QGridLayout* legacyLayout = new QGridLayout(legacy);
	legacyLayout->addWidget(new QLabel(tr("File:"), legacy), 0, 0);
	legacyLayout->addWidget(m_LegacyFileName, 0, 1);
	legacyLayout->addWidget(m_LegacyBrowse, 0, 2);
	legacyLayout->addWidget(new QLabel(tr("Name:"), legacy), 1, 0);
	legacyLayout->addWidget(m_LegacyName, 1, 1);
	legacyLayout->addWidget(m_LegacyUpload, 1, 2);
	legacyLayout->addWidget(new QLabel(tr("Stored design:"), legacy), 2, 0);
	legacyLayout->addWidget(m_LegacyDesigns, 2, 1);
	legacyLayout->addWidget(m_LegacyRefresh, 2, 2);
	legacyLayout->addWidget(m_LegacySpawn, 3, 1, 1, 2);
	legacyLayout->addWidget(m_LegacyStatus, 4, 0, 1, 3);
	legacyLayout->setColumnStretch(1, 1);
	UpdateLegacyControls();

	QGridLayout* lay = new QGridLayout;
	int row=0;
	lay->addWidget(pbdesigner, row++,0);
	lay->addWidget(pbloadsc, row++,0);
	lay->addWidget(pbloadbd, row++,0);
	lay->addWidget(legacy, row++, 0, 1, 2);
	lay->setRowStretch(row, 1);
	lay->setColumnStretch(1, 1);
	setLayout(lay);
}

AuAgentDesign::~AuAgentDesign()
{
}

void AuAgentDesign::setVisible(bool visible)
{
	QWidget::setVisible(visible);
	if(visible && m_LegacyAllowed)
		RefreshLegacyScDesigns();
}

void AuAgentDesign::LoadScDesign()
{
	quint32 designId = g_Au.Settings().readNumEntry("Widget/Admin/LoadShipReference", 0);
	QString designDef;
	if(designId)
		designDef = QVariant(designId).toString();

	bool ok;
	QString sid = QInputDialog::getText(this, tr("Load Spacecraft Design"),
			tr("<html>"
				"Enter the blueprint number of the ship design to load."
				"</html>"),
			QLineEdit::Normal, designDef, &ok);
	designId = QVariant(sid).toUInt();
	if(ok && designId)
	{
		g_Au.Settings().writeEntry("Widget/Admin/LoadShipReference", (int)designId);
		auApp->DesignOpen(designId, DaDesignBase::DESIGN_Spacecraft);	// Try to load an Sc design.
	}
}

void AuAgentDesign::LoadBdDesign()
{
	quint32 designId = g_Au.Settings().readNumEntry("Widget/Admin/LoadBuildingReference", 0);
	QString designDef;
	if(designId)
		designDef = QVariant(designId).toString();

	bool ok;
	QString sid = QInputDialog::getText(this, tr("Load Building Design"),
			tr("<html>"
				"Enter the blueprint number of the building design to load."
				"</html>"),
			QLineEdit::Normal, designDef, &ok);
	designId = QVariant(sid).toUInt();
	if(ok && designId)
	{
		g_Au.Settings().writeEntry("Widget/Admin/LoadBuildingReference", (int)designId);
		auApp->DesignOpen(designId, DaDesignBase::DESIGN_Building);	// Try to load a Bd design.
	}
}

void AuAgentDesign::EnterDesigner()
{
	auApp->EnterStudio(0, QString());
}

void AuAgentDesign::UpdateLegacyControls()
{
	const bool online = m_LegacyAllowed && !auApp->IsSingleUserMode() && auApp->IsSceneConnected();
	const bool idle = !m_LegacyUploadToken && !m_LegacyCatalogToken && !m_LegacySpawnToken;
	m_LegacyBrowse->setEnabled(online && idle);
	m_LegacyName->setEnabled(online && idle);
	m_LegacyUpload->setEnabled(online && idle && !m_LegacyFile.isEmpty() && !m_LegacyName->text().trimmed().isEmpty());
	m_LegacyRefresh->setEnabled(online && idle);
	m_LegacyDesigns->setEnabled(online && idle);
	m_LegacySpawn->setEnabled(online && idle && m_LegacyDesigns->currentData().toUInt());
	if(!online)
		m_LegacyStatus->setText(tr("Connect to an online scene server to manage legacy designs."));
}

void AuAgentDesign::BrowseLegacyScDesign()
{
	const QString last = g_Au.Settings().readEntry("Widget/Admin/LegacyShipFile", QString());
	const QString fileName = QFileDialog::getOpenFileName(this, tr("Upload legacy spacecraft design"), last,
		tr("Legacy spacecraft (*.ship)"), nullptr,
		g_Au.UseNativeFileDialog() ? QFileDialog::Options() : QFileDialog::DontUseNativeDialog);
	if(fileName.isEmpty())
		return;
	m_LegacyFile = fileName;
	m_LegacyFileName->setText(QFileInfo(fileName).fileName());
	m_LegacyFileName->setToolTip(fileName);
	m_LegacyName->setText(QFileInfo(fileName).completeBaseName());
	g_Au.Settings().writeEntry("Widget/Admin/LegacyShipFile", fileName);
	m_LegacyStatus->setText(tr("Enter a name, then upload the design."));
	UpdateLegacyControls();
}

void AuAgentDesign::UploadLegacyScDesign()
{
	if(m_LegacyUploadToken || m_LegacyCatalogToken || m_LegacySpawnToken)
		return;
	QFile file(m_LegacyFile);
	if(!file.open(QIODevice::ReadOnly))
	{
		m_LegacyStatus->setText(tr("Unable to read the selected file: %1").arg(file.errorString()));
		return;
	}
	const QByteArray source = file.read(qint64(AuLegacyScProtocol::MaximumUploadBytes) + 1);
	if(file.error() != QFile::NoError || source.isEmpty() || source.size() > AuLegacyScProtocol::MaximumUploadBytes || !file.atEnd())
	{
		m_LegacyStatus->setText(tr("Select a readable .ship file between 1 byte and 8 MiB."));
		return;
	}
	m_LegacyUploadToken = auApp->UploadLegacyScDesign(QFileInfo(m_LegacyFile).fileName(), m_LegacyName->text().trimmed(), source);
	if(!m_LegacyUploadToken)
	{
		m_LegacyStatus->setText(tr("Unable to send the upload. Check your connection and filename."));
		return;
	}
	m_LegacyTimeout->start();
	m_LegacyStatus->setText(tr("Uploading design..."));
	UpdateLegacyControls();
}

void AuAgentDesign::RefreshLegacyScDesigns()
{
	if(m_LegacyUploadToken || m_LegacyCatalogToken || m_LegacySpawnToken)
		return;
	if(!m_LegacyWantedId)
		m_LegacyWantedId = m_LegacyDesigns->currentData().toUInt();
	m_LegacyAfterId = 0;
	m_LegacyCatalogToken = auApp->RequestLegacyScCatalog();
	if(m_LegacyCatalogToken)
	{
		m_LegacyTimeout->start();
		m_LegacyStatus->setText(tr("Loading stored designs..."));
	}
	UpdateLegacyControls();
}

void AuAgentDesign::SpawnLegacyScDesign()
{
	if(m_LegacyUploadToken || m_LegacyCatalogToken || m_LegacySpawnToken)
		return;
	const quint32 designId = m_LegacyDesigns->currentData().toUInt();
	if(!designId)
		return;
	m_LegacySpawnToken = auApp->SpawnLegacyScDesign(designId);
	if(!m_LegacySpawnToken)
	{
		m_LegacyStatus->setText(tr("Unable to send the spawn request. Check your connection."));
		return;
	}
	m_LegacyTimeout->start();
	m_LegacyStatus->setText(tr("Spawning spacecraft..."));
	UpdateLegacyControls();
}

void AuAgentDesign::LegacyScReply(QByteArray payload)
{
	using namespace AuLegacyScProtocol;
	Reply reply;
	if(!DecodeReply(payload, reply))
		return;
	if(reply.action == SpawnReply)
	{
		if(reply.token != m_LegacySpawnToken)
			return;
		m_LegacySpawnToken = 0;
		m_LegacyTimeout->stop();
		m_LegacyStatus->setText(reply.message);
		UpdateLegacyControls();
		return;
	}
	if(reply.action == UploadReply)
	{
		if(reply.token != m_LegacyUploadToken)
			return;
		m_LegacyUploadToken = 0;
		m_LegacyTimeout->stop();
		m_LegacyStatus->setText(reply.message);
		if(reply.designId)
		{
			m_LegacyWantedId = reply.designId;
			QTimer::singleShot(250, this, SLOT(RefreshLegacyScDesigns()));
		}
		UpdateLegacyControls();
		return;
	}
	if(reply.action != CatalogReply || reply.token != m_LegacyCatalogToken)
		return;
	if(!reply.message.isEmpty())
	{
		m_LegacyCatalogToken = 0;
		m_LegacyTimeout->stop();
		m_LegacyStatus->setText(reply.message);
		UpdateLegacyControls();
		return;
	}
	const QSignalBlocker blockSelection(m_LegacyDesigns);
	if(!m_LegacyAfterId)
		m_LegacyDesigns->clear();
	for(const Entry& entry : reply.entries)
	{
		if(entry.id <= m_LegacyAfterId)
			continue;
		QString name = entry.name.isEmpty() ? entry.sourceName : entry.name;
		if(name.isEmpty()) name = tr("Unnamed design");
		m_LegacyDesigns->addItem(tr("%1 (ID %2)").arg(name).arg(entry.id), QVariant(entry.id));
	}
	const int selected = m_LegacyDesigns->findData(QVariant(m_LegacyWantedId));
	if(selected >= 0)
		m_LegacyDesigns->setCurrentIndex(selected);
	if(reply.nextAfterId > m_LegacyAfterId && m_LegacyDesigns->count() < 4096)
	{
		m_LegacyAfterId = reply.nextAfterId;
		const quint32 token = m_LegacyCatalogToken;
		QTimer::singleShot(250, this, [this, token]() {
			if(m_LegacyCatalogToken != token)
				return;
			if(!auApp->RequestLegacyScCatalog(m_LegacyAfterId, token))
				LegacyScDisconnected();
		});
		m_LegacyTimeout->start();
		return;
	}
	m_LegacyCatalogToken = 0;
	m_LegacyTimeout->stop();
	m_LegacyStatus->setText(reply.nextAfterId ? tr("Showing the first 4096 stored designs.")
		: tr("%1 stored designs available.").arg(m_LegacyDesigns->count()));
	UpdateLegacyControls();
}

void AuAgentDesign::LegacyScSelectionChanged()
{
	m_LegacyWantedId = m_LegacyDesigns->currentData().toUInt();
	UpdateLegacyControls();
}

void AuAgentDesign::LegacyScDisconnected()
{
	const bool spawning = m_LegacySpawnToken != 0;
	const bool pending = m_LegacyUploadToken || m_LegacyCatalogToken || spawning;
	m_LegacyUploadToken = m_LegacyCatalogToken = m_LegacySpawnToken = 0;
	m_LegacyTimeout->stop();
	if(pending)
		m_LegacyStatus->setText(spawning ? tr("Connection changed. Check the scene before spawning another spacecraft.")
			: tr("Connection changed. Refresh to check which designs were stored."));
	UpdateLegacyControls();
}

void AuAgentDesign::LegacyScTimedOut()
{
	const bool spawning = m_LegacySpawnToken != 0;
	m_LegacyUploadToken = m_LegacyCatalogToken = m_LegacySpawnToken = 0;
	m_LegacyStatus->setText(spawning ? tr("No server response. Check the scene before spawning another spacecraft.")
		: tr("No server response. Refresh to check whether the design was stored."));
	UpdateLegacyControls();
}
