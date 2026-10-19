#include "aulegacywealthlevy.h"
#include <QDataStream>
#include <QIODevice>

namespace AuLegacyCity
{
namespace
{
bool Valid(const WealthLevyOrder& order)
{
	return order.percent >= 1 && order.percent <= 100 && order.exemptions <= 15
		&& !order.statement.trimmed().isEmpty() && order.statement.size() <= 200
		&& order.issuerName.size() <= 256;
}
bool WriteText(QDataStream& out, const QString& text)
{
	const auto utf8 = text.toUtf8();
	out << quint16(utf8.size());
	return out.writeRawData(utf8.constData(), utf8.size()) == utf8.size();
}
bool ReadText(QDataStream& in, QString& text, quint16 maximum)
{
	quint16 size = 0; in >> size;
	if(in.status() != QDataStream::Ok || size > maximum || size > in.device()->bytesAvailable()) return false;
	QByteArray bytes(int(size), '\0');
	if(in.readRawData(bytes.data(), size) != size) return false;
	text = QString::fromUtf8(bytes);
	return text.toUtf8() == bytes;
}
}
QByteArray EncodeWealthLevy(const WealthLevyOrder& order)
{
	if(!Valid(order)) return QByteArray();
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << quint8(1) << order.empire << order.issuer << order.percent << order.exemptions;
	if(!WriteText(out, order.issuerName) || !WriteText(out, order.statement)) return QByteArray();
	return out.status() == QDataStream::Ok && bytes.size() <= MaximumWealthLevyBytes ? bytes : QByteArray();
}
bool DecodeWealthLevy(const QByteArray& bytes, WealthLevyOrder& output)
{
	if(bytes.isEmpty() || bytes.size() > MaximumWealthLevyBytes) return false;
	WealthLevyOrder order; quint8 version = 0;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	in >> version >> order.empire >> order.issuer >> order.percent >> order.exemptions;
	if(version != 1 || !ReadText(in, order.issuerName, 1024) || !ReadText(in, order.statement, 800)
		|| in.status() != QDataStream::Ok || !in.atEnd() || !Valid(order)) return false;
	output = order; return true;
}
bool ReadWealthLevyEnvelope(QDataStream& in, QByteArray& bytes)
{
	quint32 size = 0; in >> size;
	if(in.status() != QDataStream::Ok || !size || size > MaximumWealthLevyBytes
		|| size > quint64(in.device()->bytesAvailable())) return false;
	QByteArray bounded(int(size), '\0');
	if(in.readRawData(bounded.data(), int(size)) != int(size) || !in.atEnd()) return false;
	bytes = bounded; return true;
}
}
