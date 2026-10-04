#ifndef AuLegacyShipEquipment_H
#define AuLegacyShipEquipment_H

#include "aulegacyshipmesh.h"
#include <QHash>

namespace AuLegacyShip
{
namespace Rendering
{
// Procedural models and structural details recovered from the 2016 renderer.
// Resource-backed consoles, furniture and vehicles have a separate importer.
bool EquipmentSupported(int detailType);
void AppendEquipment(const Design& design, Mesh& mesh);

enum class DoorFrame
{
	Closed = 0,
	HalfOpen = 1,
	Open = 2
};

struct DoorAnimation
{
	DoorFrame frame = DoorFrame::Closed;
	bool open = false;
};

// The original changes the atlas region on a shared 100 ms device tick.
// Collision follows the logical open state, independently of the texture frame.
constexpr double DoorAnimationTickSeconds = .1;
constexpr double DoorInteractionDistance = 6;
bool IsDoorDetail(int detailType);
bool IsHatchDetail(int detailType);
bool IsAutomaticDoorDetail(int detailType);
bool IsDoorMaterial(Material material);
Material DoorMaterialFrame(Material closedMaterial, DoorFrame frame);
bool AdvanceDoorAnimation(DoorAnimation& animation);

struct DoorDefinition
{
	quint32 id = 0;
	int type = 0;
	int deck = 0;
	QPointF a, b, center;
	int roomType = 0;
	int adjacentRoomType = 0;
};

QVector<DoorDefinition> DoorDefinitions(const Design& design, const Mesh& mesh);
QVector<quint32> DoorOperationGroup(const QVector<DoorDefinition>& doors, quint32 id);
double DoorDistanceSquared(const DoorDefinition& door, const QPointF& point);
quint32 ClosestDoor(const QVector<DoorDefinition>& doors,
	const QHash<quint32, DoorAnimation>& animations, const QVector3D& feet, bool open);
}
}

#endif
