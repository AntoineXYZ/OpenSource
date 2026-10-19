#include "auapp.h"
#include "auclientinterface.h"
#include "auatominterface.h"
#include "auglobal.h"
#include "aulegacywealthlevy.h"
#include "aulegacywealthlevyruntime.h"
#include "dtccpacket.h"
#include <QDataStream>

void AuApp::RequestLegacyWealthLevy(quint8 percent, const QString& statement, quint8 exemptions)
{
	if(!g_Au.IsLegacyGeneration()) return;
	AuLegacyCity::WealthLevyOrder request;
	request.percent = percent; request.statement = statement.trimmed(); request.exemptions = exemptions;
	const auto bytes = AuLegacyCity::EncodeWealthLevy(request);
	if(bytes.isEmpty()) return;
	if(IsSingleUserMode()) { m_I->GovLegacyWealthLevy(bytes); return; }
	QByteArray packet; QDataStream out(&packet, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << quint8(DtChatChannelPacket::PV_LegacyWealthLevy) << bytes;
	QueueChatPacket(packet);
}

void AuApp::LegacyWealthLevy(QByteArray order)
{
	if(IsSingleUserMode()) ApplyLegacyWealthLevy(order);
}
