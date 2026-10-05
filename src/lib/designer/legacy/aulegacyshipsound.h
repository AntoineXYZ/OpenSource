#ifndef AuLegacyShipSound_H
#define AuLegacyShipSound_H

#include <QScopedPointer>
#include <QVector3D>

class AuSound;

namespace AuLegacyShip
{
struct Design;
namespace Rendering
{
struct DoorDefinition;

struct SoundListener
{
	QVector3D feet;
	QVector3D eye;
	QVector3D forward = QVector3D(0, 1, 0);
	QVector3D up = QVector3D(0, 0, 1);
	bool inShip = false;
	bool grounded = false;
	// Original footsteps follow a walking pose, including walking in place.
	bool moving = false;
	bool running = false;
	bool crouching = false;
	bool prone = false;
	bool climbing = false;
	// The original designer starts these systems off. These inputs describe
	// working systems, for later console controls; avatar gravity is separate.
	bool powerPlantWorking = false;
	bool sensorsWorking = false;
	int driveState = 0; // 0 = off, 1 = on, 2 = hold, matching the 2016 enum.
};

// Preview sources use the game's AuSound buffers, context and sound settings.
// Positions are projected into the camera's coordinates, as in the 2016 scene.
class SoundSession
{
public:
	SoundSession();
	~SoundSession();
	// Borrow the initialized game backend while its context is current.
	// Clearing after engine shutdown is safe: source cleanup checks context identity.
	void SetAudio(AuSound* audio);
	void SetDesign(const Design& design);
	void Update(const SoundListener& listener);
	// Queue this when a door first advances to its half-open animation frame.
	void DoorTransition(const DoorDefinition& door, bool opening);
	void Stop();

private:
	Q_DISABLE_COPY(SoundSession)
	class Impl;
	QScopedPointer<Impl> m_Impl;
};
}
}

#endif
