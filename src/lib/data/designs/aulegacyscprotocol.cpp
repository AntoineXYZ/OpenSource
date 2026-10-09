#include "aulegacyscprotocol.h"

#include <QDataStream>
#include <QIODevice>

namespace
{
using namespace AuLegacyScProtocol;

bool WriteText(QDataStream& out, const QString& text)
{
	const QByteArray utf8 = text.toUtf8();
	if(utf8.size() > MaximumTextBytes)
		return false;
	out << quint16(utf8.size());
	return out.writeRawData(utf8.constData(), utf8.size()) == utf8.size();
}

bool ReadText(QDataStream& in, QString& text)
{
	quint16 size = 0;
	in >> size;
	if(in.status() != QDataStream::Ok || size > MaximumTextBytes || size > in.device()->bytesAvailable())
		return false;
	QByteArray utf8(int(size), '\0');
	if(in.readRawData(utf8.data(), size) != size)
		return false;
	text = QString::fromUtf8(utf8);
	return text.toUtf8() == utf8;
}
}

QByteArray AuLegacyScProtocol::EncodeRequest(const Request& request)
{
	QByteArray payload;
	QDataStream out(&payload, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	out << quint8(request.action) << request.token;
	if(request.action == CatalogRequest)
		out << request.afterId;
	else if(request.action == UploadRequest)
	{
		if(request.source.isEmpty() || request.source.size() > MaximumUploadBytes
			|| !WriteText(out, request.sourceName) || !WriteText(out, request.name))
			return QByteArray();
		out << quint32(request.source.size());
		out.writeRawData(request.source.constData(), request.source.size());
	}
	else
		return QByteArray();
	return out.status() == QDataStream::Ok ? payload : QByteArray();
}

bool AuLegacyScProtocol::DecodeRequest(const QByteArray& payload, Request& request, QString& error)
{
	request = Request();
	error = QStringLiteral("Malformed legacy spacecraft request.");
	if(payload.size() < 5 || payload.size() > MaximumPacketBytes - 1)
		return false;
	QDataStream in(payload);
	in.setVersion(QDataStream::Qt_4_1);
	quint8 action = 0;
	in >> action >> request.token;
	if(action == CatalogRequest)
	{
		request.action = CatalogRequest;
		in >> request.afterId;
	}
	else if(action == UploadRequest)
	{
		request.action = UploadRequest;
		if(!ReadText(in, request.sourceName) || !ReadText(in, request.name))
			return false;
		quint32 size = 0;
		in >> size;
		if(in.status() != QDataStream::Ok || !size || size > MaximumUploadBytes
			|| size != in.device()->bytesAvailable())
			return false;
		request.source = payload.right(int(size));
		in.skipRawData(int(size));
	}
	else
		return false;
	if(!request.token || in.status() != QDataStream::Ok || !in.atEnd())
		return false;
	error.clear();
	return true;
}

QByteArray AuLegacyScProtocol::EncodeReply(const Reply& reply)
{
	QByteArray payload;
	QDataStream out(&payload, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	out << quint8(reply.action) << reply.token;
	if(!WriteText(out, reply.message))
		return QByteArray();
	if(reply.action == CatalogReply)
	{
		if(reply.entries.size() > CatalogPageSize)
			return QByteArray();
		out << reply.nextAfterId << quint16(reply.entries.size());
		for(const Entry& entry : reply.entries)
		{
			out << entry.id;
			if(!WriteText(out, entry.name) || !WriteText(out, entry.sourceName))
				return QByteArray();
		}
	}
	else if(reply.action == UploadReply)
	{
		out << reply.designId;
		if(!WriteText(out, reply.name))
			return QByteArray();
	}
	else
		return QByteArray();
	return out.status() == QDataStream::Ok ? payload : QByteArray();
}

bool AuLegacyScProtocol::DecodeReply(const QByteArray& payload, Reply& reply)
{
	reply = Reply();
	if(payload.size() < 7 || payload.size() > 2 * MaximumTextBytes * (CatalogPageSize + 1) + 512)
		return false;
	QDataStream in(payload);
	in.setVersion(QDataStream::Qt_4_1);
	quint8 action = 0;
	in >> action >> reply.token;
	if(!ReadText(in, reply.message))
		return false;
	if(action == CatalogReply)
	{
		reply.action = CatalogReply;
		quint16 count = 0;
		in >> reply.nextAfterId >> count;
		if(in.status() != QDataStream::Ok || count > CatalogPageSize)
			return false;
		quint32 previousId = 0;
		for(int index = 0; index < count; ++index)
		{
			Entry entry;
			in >> entry.id;
			if(!entry.id || entry.id <= previousId || !ReadText(in, entry.name) || !ReadText(in, entry.sourceName))
				return false;
			previousId = entry.id;
			reply.entries.append(entry);
		}
		if(reply.nextAfterId && (reply.entries.isEmpty() || reply.nextAfterId != previousId))
			return false;
	}
	else if(action == UploadReply)
	{
		reply.action = UploadReply;
		in >> reply.designId;
		if(!ReadText(in, reply.name))
			return false;
	}
	else
		return false;
	return reply.token && in.status() == QDataStream::Ok && in.atEnd();
}
