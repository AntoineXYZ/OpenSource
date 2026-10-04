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
	// Body dimensions approximate a standing human; the original derives
	// them from the character's DNA and current pose.
	float radius = .75f;
	float height = 6;
	float eyeHeight = 5.5f;
	float gravityAcceleration = 32;
	float jumpSpeed = 15;
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
	QVector<quint32> blockedDoors;
};

struct WalkHit
{
	QVector3D point;
	QVector3D normal;
	float distance = 0;
	quint32 doorId = 0;
};

// Local authoring preview collision. No server, live character or world scene
// is required. Positions, speeds and accelerations use the design's feet units.
class WalkWorld
{
public:
	WalkWorld();
	~WalkWorld();
	void SetMesh(const Mesh& mesh, const Design& design);
	void SetSettings(const WalkSettings& settings);
	const WalkSettings& Settings() const;
	void SetDesignPlane(float height);
	// SetMesh clears door states. Reapply current states after rebuilding.
	void SetDoorOpen(quint32 doorId, bool open);
	// Attach to the nearest reachable hatch axis, or release the current ladder.
	// ladderFacing is the initial camera azimuth in degrees, not a look constraint.
	bool ToggleLadder(WalkState& state) const;
	bool FindSpawn(const Design& design, int deck, WalkState& state) const;
	// desiredVelocity is already expressed in world coordinates, in feet/s.
	// Its z component controls free flight and ladder/gravity-well travel.
	void Step(WalkState& state, const QVector3D& desiredVelocity, float seconds,
		bool gravity, bool jump) const;
	// Visible static surfaces, including passable equipment, occlude the ray;
	// invisible obstacle-cell volumes do not. Open panels remain hand targets;
	// sight targeting can exclude them.
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
