#include "ausceneserverapp.h"
#include "auglobal.h"
#include "aulegacywealthlevy.h"
#include "aulegacywealthlevyruntime.h"
#include "dgdata.h"
#include "dtclientchannel.h"
#include "dtserverchannel.h"
#include <QDataStream>

void AuSceneServerApp::LegacyWealthLevyUp(QByteArray bytes)
{
	AuLegacyCity::WealthLevyOrder order;
	if(!g_Au.IsLegacyGeneration() || !AuLegacyCity::DecodeWealthLevy(bytes, order)
		|| !order.empire || order.issuer.IsNull()) return;
	const DgData& gd = g_Au.LockDataForRead();
	const bool authorized = gd.EmpireCitizenCanSetTax(order.empire, order.issuer);
	g_Au.UnlockData();
	if(!authorized) return;
	if(!m_tClientChannel) { LegacyWealthLevyDown(bytes); return; }
	QByteArray packet; QDataStream out(&packet, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << quint8(DtServerChannel::P_LegacyWealthLevy) << bytes;
	m_tClientChannel->SendPacket(packet);
}

void AuSceneServerApp::LegacyWealthLevyDown(QByteArray bytes)
{
	AuLegacyCity::WealthLevyOrder order;
	if(!g_Au.IsLegacyGeneration() || !AuLegacyCity::DecodeWealthLevy(bytes, order)
		|| !order.empire || order.issuer.IsNull()) return;
	if(g_Au.IsSceneWorker()) { ApplyLegacyWealthLevy(bytes); return; }
	QByteArray packet; QDataStream out(&packet, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << quint8(DtServerChannel::P_LegacyWealthLevy) << bytes;
	const auto& servers = m_ServerChannel.LockServerList(__FILE__, __LINE__);
	for(const auto& server : servers)
		if(server.second->isRunning()) server.second->SendPacket(packet);
	m_ServerChannel.UnlockServerList(__FILE__, __LINE__);
}
