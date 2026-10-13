#ifndef AuLegacyScNpcOcclusion_H
#define AuLegacyScNpcOcclusion_H

#include "auatom.h"
#include "aubox.h"
#include "auscface.h"
#include <QSet>
#include <QVector>

// Immutable, CPU-only occluders shared with the existing legacy render model.
// This affects NPC visibility only; ship draw batches and geometry stay intact.
class AUATOM_DLL AuLegacyScNpcOcclusion
{
public:
	void Add(const AuScTriRenderList& triangles, quint16 doorId);
	void Build();
	bool Occluded(const AuBox& bounds, const AuPoint& eye, const QSet<quint16>& closedDoors) const;
private:
	struct Face
	{
		AuPoint points[4], normal, minimum, maximum;
		quint16 door = 0;
		int count = 3;
	};
	struct Node
	{
		AuPoint minimum, maximum;
		int first = 0, count = 0, left = -1, right = -1;
	};
	QVector<Face> m_Faces;
	QVector<Node> m_Nodes;
	int BuildNode(int first, int count);
	static bool Covers(const Face& face, const AuBox& bounds, const AuPoint& eye);
};

#endif
