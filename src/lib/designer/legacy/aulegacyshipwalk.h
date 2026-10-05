#ifndef AuLegacyShipWalk_H
#define AuLegacyShipWalk_H

#include "aulegacyshipmesh.h"
#include <QScopedPointer>

namespace AuLegacyShip
{
namespace Rendering
{
struct WalkSettings
{
	// Defaults until the native character supplies its current body dimensions.
	float radius = .75f;
	float height = 6;
	float eyeHeight = 5.5f;
	float stepHeight = .75f;
};

struct WalkState
{
	QVector3D feet;
	QVector3D velocity;
	bool grounded = false;
	bool onLadder = false;
	bool ladderEngaged = false;
	QVector3D ladderAnchor;
	float ladderFacing = 0;
	float ladderBottom = 0;
	float ladderTop = 0;
	bool gravitySuspended = false;
	bool gravityActive = false;
	bool inShip = false;
	int consoleType = 0;
	quint32 consoleId = 0;
	QVector<quint32> blockedDoors;
};

struct WalkHit
{
	QVector3D point;
	QVector3D normal;
	float distance = 0;
	quint32 doorId = 0;
	quint32 detailId = 0;
};

struct WalkEnvironment
{
	int roomType = 0;
	bool inShip = false;
	bool gravity = false;
	bool gravityWell = false;
	float fallDistance = 1000000;
};

// Legacy geometry adapter for the native character controller, in feet.
class WalkWorld
{
public:
	WalkWorld();
	~WalkWorld();
	void SetMesh(const Mesh& mesh, const Design& design);
	void SetSettings(const WalkSettings& settings);
	bool TrySetBodySize(const QVector3D& feet, float height, float eyeHeight, bool requireClear);
	const WalkSettings& Settings() const;
	void SetDesignPlane(float height);
	// SetMesh clears door states. Reapply current states after rebuilding.
	void SetDoorOpen(quint32 doorId, bool open);
	// Attach to the nearest reachable hatch axis, or release the current ladder.
	// ladderFacing is the initial camera azimuth in degrees, not a look constraint.
	bool ToggleLadder(WalkState& state) const;
	bool EnterConsole(const Design& design, WalkState& state, float cgHeight) const;
	void SetConsoleRotation(quint32 consoleId, const QVector3D& floorPoint, float facing);
	bool FindSpawn(const Design& design, int deck, WalkState& state) const;
	WalkEnvironment Environment(const QVector3D& feet, bool studioGravity, float bodyOriginHeight) const;
	// Geometry adapter only: native DaPerson integrates forces and velocities.
	void MoveDisplacement(WalkState& state, const QVector3D& displacement, bool studioGravity) const;
	// Hand rays include visible passable equipment and open panels. Passing
	// includeOpenDoors=false applies legacy sight eligibility and skips open
	// panels. Invisible obstacle-cell volumes never occlude either ray type.
	bool Raycast(const QVector3D& origin, const QVector3D& direction,
		float maximumDistance, WalkHit& hit, bool includeOpenDoors = true) const;

private:
	Q_DISABLE_COPY(WalkWorld)
	class Impl;
	QScopedPointer<Impl> m_Impl;
};
}
}

#endif
