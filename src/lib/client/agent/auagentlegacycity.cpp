#include "auagentlegacycity.h"
#include "auapp.h"
#include "auglobal.h"
#include "aulegacybd.h"
#include "aulegacycitycatalogue.h"
#include "auid.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

AuAgentLegacyCity::AuAgentLegacyCity(QWidget* parent) : QWidget(parent)
{
	auto* layout = new QVBoxLayout(this);
	auto* help = new QLabel(tr("Place original buildings to test their geometry, collision and routes. Doorways and hatches always remain open. Sites level the ground and apply original paving and vegetation rules. Changes persist across restart."), this);
	help->setWordWrap(true); layout->addWidget(help);
	auto* settings = new QGroupBox(tr("Building settings"), this);
	auto* form = new QFormLayout(settings);
	m_Blueprints = new QComboBox(settings); form->addRow(tr("Original blueprint:"), m_Blueprints);
	m_DesignInfo = new QLabel(settings); m_DesignInfo->setWordWrap(true); form->addRow(m_DesignInfo);
	m_Materials = new QComboBox(settings);
	m_Materials->addItem(tr("Wood"), 86); m_Materials->addItem(tr("Lumber"), 48);
	m_Materials->addItem(tr("Stone"), 76); m_Materials->addItem(tr("Metal"), 51);
	m_Materials->addItem(tr("Magmium"), 146); m_Materials->addItem(tr("Vulcium"), 145);
	m_Materials->setCurrentIndex(2); form->addRow(tr("Material:"), m_Materials);
	m_MinimumDeck = new QSpinBox(settings); m_MinimumDeck->setRange(-127, 126);
	m_MaximumDeck = new QSpinBox(settings); m_MaximumDeck->setRange(-127, 126);
	form->addRow(tr("Lowest constructed deck:"), m_MinimumDeck);
	form->addRow(tr("Highest constructed deck:"), m_MaximumDeck);
	m_Constructed = new QCheckBox(tr("Completed geometry (clear for construction footprint)"), settings);
	m_Constructed->setChecked(true); form->addRow(m_Constructed);
	const auto number = [](QWidget* parentWidget, double minimum, double maximum, double value) {
		auto* spin = new QDoubleSpinBox(parentWidget); spin->setRange(minimum, maximum);
		spin->setDecimals(2); spin->setValue(value); return spin;
	};
	m_Distance = number(settings, 5, 500, 40); m_Distance->setSuffix(tr(" m"));
	m_Heading = number(settings, -180, 180, 0); m_Heading->setSuffix(tr(" degrees"));
	m_Elevation = number(settings, -100, 100, 0); m_Elevation->setSuffix(tr(" m"));
	form->addRow(tr("Spawn distance ahead:"), m_Distance);
	form->addRow(tr("Heading:"), m_Heading);
	form->addRow(tr("Elevation above terrain:"), m_Elevation);
	m_Spawn = new QPushButton(tr("Spawn test building ahead"), settings); form->addRow(m_Spawn);
	layout->addWidget(settings);
	auto* existing = new QGroupBox(tr("Buildings within 2 km"), this);
	auto* actions = new QGridLayout(existing);
	m_Buildings = new QComboBox(existing); m_Refresh = new QPushButton(tr("Refresh"), existing);
	m_Buildings->setMinimumContentsLength(28);
	m_Buildings->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	actions->addWidget(m_Buildings, 0, 0, 1, 2); actions->addWidget(m_Refresh, 0, 2);
	m_Apply = new QPushButton(tr("Apply test settings"), existing);
	m_Remove = new QPushButton(tr("Remove selected building"), existing);
	actions->addWidget(m_Apply, 1, 0, 1, 3);
	actions->addWidget(m_Remove, 2, 0, 1, 3);
	auto* applyHelp = new QLabel(tr("Select a building to load its settings. Apply while standing outside it. To test another blueprint, spawn a new building."), existing);
	applyHelp->setWordWrap(true); actions->addWidget(applyHelp, 3, 0, 1, 3);
	layout->addWidget(existing);
	auto* route = new QGroupBox(tr("Route from your avatar"), this);
	auto* routeForm = new QFormLayout(route);
	m_TargetX = number(route, -1000, 1000, 0); m_TargetY = number(route, -1000, 1000, 0);
	m_TargetZ = number(route, -1100, 1100, 4);
	routeForm->addRow(tr("Target X (building feet):"), m_TargetX);
	routeForm->addRow(tr("Target Y (building feet):"), m_TargetY);
	routeForm->addRow(tr("Target Z (building feet):"), m_TargetZ);
	m_Route = new QPushButton(tr("Check route to local point"), route); routeForm->addRow(m_Route);
	layout->addWidget(route);
	m_Status = new QLabel(this); m_Status->setWordWrap(true); m_Status->setTextFormat(Qt::PlainText);
	m_DesignInfo->setTextFormat(Qt::PlainText); layout->addWidget(m_Status); layout->addStretch();
	m_Timeout = new QTimer(this); m_Timeout->setSingleShot(true); m_Timeout->setInterval(60000);
	connect(m_Timeout, &QTimer::timeout, this, [this]() {
		m_PendingToken = 0; m_Status->setText(tr("No server response. Refresh the list before repeating a change.")); UpdateControls();
	});
	connect(m_Spawn, &QPushButton::clicked, this, [this]() { Send(AuLegacyCityProtocol::Spawn); });
	connect(m_Refresh, &QPushButton::clicked, this, &AuAgentLegacyCity::Refresh);
	connect(m_Apply, &QPushButton::clicked, this, [this]() { Send(AuLegacyCityProtocol::Update); });
	connect(m_Remove, &QPushButton::clicked, this, [this]() { Send(AuLegacyCityProtocol::Remove); });
	connect(m_Route, &QPushButton::clicked, this, [this]() { Send(AuLegacyCityProtocol::Route); });
	connect(m_Blueprints, SIGNAL(currentIndexChanged(int)), this, SLOT(BlueprintChanged()));
	connect(m_Buildings, SIGNAL(currentIndexChanged(int)), this, SLOT(BuildingChanged()));
	connect(auApp, &AuApp::LegacyCityReplyReceived, this, &AuAgentLegacyCity::ReceiveReply);
	connect(auApp, &AuApp::LegacyScTransportReset, this, &AuAgentLegacyCity::ResetTransport);
	auto* connectionTimer = new QTimer(this); connectionTimer->setInterval(1000);
	connect(connectionTimer, &QTimer::timeout, this, &AuAgentLegacyCity::UpdateControls); connectionTimer->start();
	UpdateControls();
}

bool AuAgentLegacyCity::LoadBlueprints()
{
	if(m_Blueprints->count()) return true;
	QString error;
	if(!AuLegacyBd::Catalog::LoadBuiltIn(error) || !AuLegacyCityCatalogue::LoadBuiltIn(error))
	{ m_Status->setText(error); return false; }
	const QSignalBlocker block(m_Blueprints);
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	for(quint32 id = 1; id <= 74; ++id)
	{
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(id);
		if(!design) continue;
		const auto* industry = catalogue->Industry(AuLegacyCity::IndustryId{design->header.industryId});
		const QString name = industry ? industry->name : design->header.name;
		m_Blueprints->addItem(tr("B%1 — %2").arg(id).arg(name), id);
	}
	BlueprintChanged(); return m_Blueprints->count() != 0;
}

void AuAgentLegacyCity::setVisible(bool visible)
{
	QWidget::setVisible(visible);
	if(visible) Refresh();
}

void AuAgentLegacyCity::BlueprintChanged()
{
	const auto design = AuLegacyBd::Catalog::BuiltIn().Find(m_Blueprints->currentData().toUInt());
	if(!design) return;
	m_MinimumDeck->setValue(design->MinimumDeck()); m_MaximumDeck->setValue(design->MaximumDeck());
	m_DesignInfo->setText(tr("%1 rooms, %2 details. Source decks %3 through %4. Each legacy deck is 8 feet high." )
		.arg(design->rooms.size()).arg(design->details.size()).arg(design->MinimumDeck()).arg(design->MaximumDeck()));
}

void AuAgentLegacyCity::BuildingChanged()
{
	const int index = m_Buildings->currentData().toInt();
	if(m_Buildings->currentIndex() < 0 || index < 0 || index >= m_Entries.size()) return;
	const auto& e = m_Entries[index];
	m_Blueprints->setCurrentIndex(m_Blueprints->findData(e.designId));
	m_Materials->setCurrentIndex(m_Materials->findData(e.material));
	m_MinimumDeck->setValue(e.minimumDeck); m_MaximumDeck->setValue(e.maximumDeck);
	m_Constructed->setChecked(e.constructed); m_Elevation->setValue(e.elevationMeters); m_Heading->setValue(e.headingDegrees);
	UpdateControls();
}

void AuAgentLegacyCity::UpdateControls()
{
	const bool online = g_Au.IsLegacyGeneration() && !auApp->IsSingleUserMode() && auApp->IsSceneConnected();
	if(online && !m_Blueprints->count()) LoadBlueprints();
	const bool idle = online && !m_PendingToken;
	m_Spawn->setEnabled(idle && m_Blueprints->currentIndex() >= 0); m_Refresh->setEnabled(idle);
	m_Blueprints->setEnabled(idle); m_Materials->setEnabled(idle); m_Buildings->setEnabled(idle);
	QWidget* settings[] = {m_MinimumDeck, m_MaximumDeck, m_Constructed, m_Distance, m_Heading,
		m_Elevation, m_TargetX, m_TargetY, m_TargetZ};
	for(auto* setting : settings) setting->setEnabled(idle);
	const bool selected = idle && m_Buildings->currentIndex() >= 0;
	for(auto* button : {m_Apply, m_Remove, m_Route}) button->setEnabled(selected);
	if(!online) m_Status->setText(tr("Connect to a Legacy server and enable architect or programmer powers to test cities."));
}

void AuAgentLegacyCity::Refresh() { Send(AuLegacyCityProtocol::List); }

void AuAgentLegacyCity::Send(AuLegacyCityProtocol::Action action)
{
	using namespace AuLegacyCityProtocol;
	if(m_PendingToken || !g_Au.IsLegacyGeneration()) return;
	Request request; request.action = action;
	request.designId = m_Blueprints->currentData().toUInt(); request.material = quint16(m_Materials->currentData().toUInt());
	request.minimumDeck = qint8(m_MinimumDeck->value()); request.maximumDeck = qint8(m_MaximumDeck->value());
	request.constructed = m_Constructed->isChecked(); request.distanceMeters = float(m_Distance->value());
	request.headingDegrees = float(m_Heading->value()); request.elevationMeters = float(m_Elevation->value());
	request.targetX = float(m_TargetX->value()); request.targetY = float(m_TargetY->value()); request.targetZ = float(m_TargetZ->value());
	if(action != List && action != Spawn)
	{
		const int index = m_Buildings->currentData().toInt();
		if(m_Buildings->currentIndex() < 0 || index < 0 || index >= m_Entries.size()) return;
		request.cityId = m_Entries[index].cityId; request.developmentId = m_Entries[index].developmentId;
	}
	if((action == Spawn || action == Update) && (request.minimumDeck > request.maximumDeck
		|| int(request.maximumDeck) - int(request.minimumDeck) > 127))
	{ m_Status->setText(tr("Choose ordered deck limits spanning at most 128 levels.")); return; }
	m_PendingToken = auApp->RequestLegacyCityTool(request);
	if(m_PendingToken) { m_Timeout->start(); m_Status->setText(tr("Waiting for the Legacy city tools...")); }
	else m_Status->setText(tr("Unable to send the request. Check the Legacy connection and settings."));
	UpdateControls();
}

void AuAgentLegacyCity::ReceiveReply(QByteArray payload)
{
	using namespace AuLegacyCityProtocol;
	Reply reply;
	if(!DecodeReply(payload, reply) || !m_PendingToken || reply.token != m_PendingToken) return;
	m_PendingToken = 0; m_Timeout->stop();
	if(!reply.hasList) { m_Status->setText(reply.message); UpdateControls(); return; }
	quint32 wantedCity = reply.selectedCity, wantedDevelopment = reply.selectedDevelopment;
	const int previous = m_Buildings->currentData().toInt();
	if(!wantedCity && m_Buildings->currentIndex() >= 0 && previous >= 0 && previous < m_Entries.size())
	{ wantedCity = m_Entries[previous].cityId; wantedDevelopment = m_Entries[previous].developmentId; }
	{
		const QSignalBlocker block(m_Buildings); m_Buildings->clear(); m_Entries = reply.entries;
		int selected = -1;
		for(int i = 0; i < m_Entries.size(); ++i)
		{
			const auto& e = m_Entries[i];
			m_Buildings->addItem(tr("%1 / %2 / development %3 / B%4")
				.arg(e.name).arg(AuId(e.cityId).StringId()).arg(e.developmentId).arg(e.designId), i);
			if(e.cityId == wantedCity && e.developmentId == wantedDevelopment) selected = i;
		}
		if(selected >= 0) m_Buildings->setCurrentIndex(selected);
	}
	BuildingChanged();
	QString status = reply.message.isEmpty() ? tr("%1 buildings available within 2 km.").arg(reply.entries.size()) : reply.message;
	if(reply.truncated) status += tr(" Showing the first 256 buildings; move closer to the desired site.");
	m_Status->setText(status); UpdateControls();
}

void AuAgentLegacyCity::ResetTransport()
{
	m_PendingToken = 0; m_Timeout->stop(); m_Entries.clear(); m_Buildings->clear();
	m_Status->setText(tr("Connection changed. Refresh the building list after reconnecting.")); UpdateControls();
}
