#include "aulegacycityprotocol.h"
#include <QDataStream>
#include <QIODevice>
#include <cmath>

namespace
{
using namespace AuLegacyCityProtocol;
bool Valid(const Request& r)
{
	if(!r.token || r.action > Route) return false;
	if(r.action != List && r.action != Spawn && (!r.cityId || !r.developmentId)) return false;
	if(r.action == Spawn || r.action == Update)
	{
		if(!r.designId || r.designId > 74 || r.minimumDeck < -127 || r.maximumDeck > 126
			|| r.minimumDeck > r.maximumDeck || int(r.maximumDeck) - int(r.minimumDeck) > 127) return false;
		if(r.material != 86 && r.material != 48 && r.material != 76
			&& r.material != 51 && r.material != 146 && r.material != 145) return false;
	}
	return std::isfinite(r.distanceMeters) && r.distanceMeters >= 5 && r.distanceMeters <= 500
		&& std::isfinite(r.headingDegrees) && r.headingDegrees >= -180 && r.headingDegrees <= 180
		&& std::isfinite(r.elevationMeters) && std::abs(r.elevationMeters) <= 100
		&& std::isfinite(r.targetX) && std::abs(r.targetX) <= 1000
		&& std::isfinite(r.targetY) && std::abs(r.targetY) <= 1000
		&& std::isfinite(r.targetZ) && std::abs(r.targetZ) <= 1100;
}
bool Text(QDataStream& out, const QString& text, int maximum)
{
	const QByteArray utf8 = text.toUtf8();
	if(utf8.size() > maximum) return false;
	out << quint16(utf8.size());
	return out.writeRawData(utf8.constData(), utf8.size()) == utf8.size();
}
bool ReadText(QDataStream& in, QString& text, int maximum)
{
	quint16 size = 0; in >> size;
	if(in.status() != QDataStream::Ok || size > maximum || size > in.device()->bytesAvailable()) return false;
	QByteArray utf8(int(size), '\0');
	if(in.readRawData(utf8.data(), size) != size) return false;
	text = QString::fromUtf8(utf8);
	return text.toUtf8() == utf8;
}
}

QByteArray AuLegacyCityProtocol::EncodeRequest(const Request& r)
{
	if(!Valid(r)) return QByteArray();
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << quint8(1) << quint8(r.action) << r.token << r.cityId << r.developmentId << r.designId
		// Preserve the retired door boolean's version-1 byte position.
		<< r.material << r.minimumDeck << r.maximumDeck << r.constructed << false
		<< r.distanceMeters << r.headingDegrees << r.elevationMeters << r.targetX << r.targetY << r.targetZ;
	return out.status() == QDataStream::Ok && bytes.size() <= MaximumRequestBytes ? bytes : QByteArray();
}
bool AuLegacyCityProtocol::DecodeRequest(const QByteArray& bytes, Request& result)
{
	if(bytes.size() > MaximumRequestBytes || bytes.size() < 6) return false;
	Request r; quint8 version = 0, action = 0; bool retiredDoorsOpen = false;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	in >> version >> action >> r.token >> r.cityId >> r.developmentId >> r.designId
		>> r.material >> r.minimumDeck >> r.maximumDeck >> r.constructed >> retiredDoorsOpen
		>> r.distanceMeters >> r.headingDegrees >> r.elevationMeters >> r.targetX >> r.targetY >> r.targetZ;
	if(version != 1 || action > Route || in.status() != QDataStream::Ok || !in.atEnd()) return false;
	r.action = static_cast<Action>(action);
	if(!Valid(r)) return false;
	result = r; return true;
}
QByteArray AuLegacyCityProtocol::EncodeReply(const Reply& r)
{
	if(!r.token || r.action > Route || r.entries.size() > MaximumEntries) return QByteArray();
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << quint8(1) << quint8(r.action) << r.token << r.selectedCity << r.selectedDevelopment << r.hasList << r.truncated;
	if(!Text(out, r.message, 4096)) return QByteArray();
	out << quint16(r.entries.size());
	for(const Entry& e : r.entries)
	{
		out << e.cityId << e.developmentId << e.designId << e.material << e.minimumDeck << e.maximumDeck
			<< e.constructed << quint16(0) << e.elevationMeters << e.headingDegrees;
		if(!Text(out, e.name, 512)) return QByteArray();
	}
	return out.status() == QDataStream::Ok && bytes.size() <= MaximumReplyBytes ? bytes : QByteArray();
}
bool AuLegacyCityProtocol::DecodeReply(const QByteArray& bytes, Reply& result)
{
	if(bytes.size() > MaximumReplyBytes || bytes.size() < 6) return false;
	Reply r; quint8 version = 0, action = 0; quint16 count = 0;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	in >> version >> action >> r.token >> r.selectedCity >> r.selectedDevelopment >> r.hasList >> r.truncated;
	if(version != 1 || action > Route || !r.token || !ReadText(in, r.message, 4096)) return false;
	in >> count;
	if(in.status() != QDataStream::Ok || count > MaximumEntries || (!r.hasList && count)
		|| count * 29 > in.device()->bytesAvailable()) return false;
	for(int i = 0; i < count; ++i)
	{
		Entry e; quint16 retiredDoorCount = 0;
		in >> e.cityId >> e.developmentId >> e.designId >> e.material >> e.minimumDeck >> e.maximumDeck
			>> e.constructed >> retiredDoorCount >> e.elevationMeters >> e.headingDegrees;
		if(!e.cityId || !e.developmentId || !e.designId || !std::isfinite(e.elevationMeters) || !std::isfinite(e.headingDegrees)
			|| !ReadText(in, e.name, 512)) return false;
		r.entries.append(e);
	}
	if(in.status() != QDataStream::Ok || !in.atEnd()) return false;
	r.action = static_cast<Action>(action); result = r; return true;
}
