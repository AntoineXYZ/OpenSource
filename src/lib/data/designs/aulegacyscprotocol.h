#ifndef AuLegacyScProtocol_H
#define AuLegacyScProtocol_H

#include "audata.h"
#include <QByteArray>
#include <QString>
#include <QVector>

namespace AuLegacyScProtocol
{
constexpr int MaximumUploadBytes = 8 * 1024 * 1024;
constexpr int MaximumTextBytes = 1024;
constexpr int MaximumNameCharacters = 128;
constexpr int CatalogPageSize = 64;
constexpr int MaximumPacketBytes = MaximumUploadBytes + 2 * MaximumTextBytes + 16;

enum Action : quint8 { CatalogRequest = 0, UploadRequest = 1, CatalogReply = 2, UploadReply = 3 };

struct Request
{
	Action action = CatalogRequest;
	quint32 token = 0;
	quint32 afterId = 0;
	QString sourceName;
	QString name;
	QByteArray source;
};

struct Entry
{
	quint32 id = 0;
	QString name;
	QString sourceName;
};

struct Reply
{
	Action action = CatalogReply;
	quint32 token = 0;
	quint32 designId = 0;
	quint32 nextAfterId = 0;
	QString name;
	QString message;
	QVector<Entry> entries;
};

// Payloads exclude the outer chat packet type and framing. Lengths are checked
// before allocating strings, file bytes, or catalog entries.
AUDATA_DLL QByteArray EncodeRequest(const Request& request);
AUDATA_DLL bool DecodeRequest(const QByteArray& payload, Request& request, QString& error);
AUDATA_DLL QByteArray EncodeReply(const Reply& reply);
AUDATA_DLL bool DecodeReply(const QByteArray& payload, Reply& reply);
}

#endif
