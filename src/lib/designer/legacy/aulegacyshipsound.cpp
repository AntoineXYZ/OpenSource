#include "aulegacyshipsound.h"
#include "aulegacyshipequipment.h"
#include "ausound.h"

#include <algorithm>
#include <limits>

namespace AuLegacyShip
{
namespace Rendering
{
namespace
{
enum Channel { CargoDoor, ExteriorDoor, Hatch, InteriorDoor, Footsteps, PowerPlant, Sensors, Drive, ChannelCount };
struct Source
{
	ALuint id = 0;
	SOUND_EFFECT sound = SOUND_None;
	QVector3D position;
};
struct DoorSound
{
	SOUND_EFFECT sound = SOUND_None;
	QVector3D position;
};

QVector3D Nearest(const QVector<QVector3D>& locations, const QVector3D& listener)
{
	QVector3D result;
	float distance = std::numeric_limits<float>::max();
	for(const QVector3D& location : locations)
	{
		const float candidate = (location - listener).lengthSquared();
		if(candidate < distance)
		{
			distance = candidate;
			result = location;
		}
	}
	return result;
}
}

class SoundSession::Impl
{
public:
	AuSound* audio = nullptr;
	ALCcontext* context = nullptr; // Borrowed context identity, never owned.
	Source sources[ChannelCount];
	DoorSound pending[4];
	QVector<QVector3D> powerPlants, sensors, drives;
	QVector3D shipCenter;
	QVector3D eye, forward, right, up;

	bool IsReady() const
	{
		return audio && gp_AuGlobal && context && alcGetCurrentContext() == context;
	}

	QVector3D Project(const QVector3D& point) const
	{
		const QVector3D relative = point - eye;
		return QVector3D(QVector3D::dotProduct(relative, right),
			QVector3D::dotProduct(relative, up), -QVector3D::dotProduct(relative, forward));
	}

	void StopSource(Source& source)
	{
		if(source.id)
			audio->StopSound(source.id);
		source.sound = SOUND_None;
	}

	void Position(Source& source)
	{
		if(!source.id)
			return;
		const QVector3D position = Project(source.position);
		alSource3f(source.id, AL_POSITION, position.x(), position.y(), position.z());
	}

	void Play(Source& source, SOUND_EFFECT sound, const QVector3D& position,
		float gain, float pitch, bool loop, bool restart = false,
		float minimumGain = 0, float maximumGain = 1, bool footsteps = false)
	{
		// Apply the current per-effect and overall engine settings once.
		// The original ship renderer likewise configures its own sources
		// around buffers supplied by the shared game sound service.
		const float effectGain = AuSound::SoundGain(sound);
		const float master = g_Au.SoundEffectsGain();
		gain *= effectGain * master;
		if(gain <= .001f)
		{
			StopSource(source);
			return;
		}
		if(restart || source.sound != sound)
			StopSource(source);
		if(!source.id)
		{
			const ALuint buffer = audio->AudioBuffer(sound);
			if(!buffer)
				return;
			alGenSources(1, &source.id);
			if(!source.id)
				return;
			alSourcei(source.id, AL_BUFFER, buffer);
			alSourcei(source.id, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
			// The game projects atom sound locations into camera space.
			// Keep our sources relative without changing its shared listener.
			alSourcei(source.id, AL_SOURCE_RELATIVE, AL_TRUE);
			source.sound = sound;
		}
		source.position = position;
		Position(source);
		alSourcef(source.id, AL_GAIN, gain);
		alSourcef(source.id, AL_PITCH, pitch);
		// Creature audio in 2016 explicitly uses one foot; ship sources
		// use the distance and rolloff in the shared sound effect table.
		alSourcef(source.id, AL_REFERENCE_DISTANCE,
			footsteps ? 1.f : float(AuSound::SoundReferenceDistance(sound)));
		alSourcef(source.id, AL_ROLLOFF_FACTOR,
			footsteps ? 1.f : AuSound::SoundRolloffFactor(sound));
		alSourcef(source.id, AL_MIN_GAIN, std::min(1.f, minimumGain * effectGain * master));
		alSourcef(source.id, AL_MAX_GAIN, maximumGain);
		ALint state = AL_INITIAL;
		alGetSourcei(source.id, AL_SOURCE_STATE, &state);
		if(state != AL_PLAYING)
			alSourcePlay(source.id);
	}
};

SoundSession::SoundSession() : m_Impl(new Impl) {}
SoundSession::~SoundSession() { Stop(); }

void SoundSession::SetAudio(AuSound* audio)
{
	ALCcontext* context = audio ? alcGetCurrentContext() : nullptr;
	if(m_Impl->audio == audio && m_Impl->context == context)
		return;
	Stop();
	m_Impl->audio = audio;
	m_Impl->context = context;
}

void SoundSession::SetDesign(const Design& design)
{
	Stop();
	m_Impl->powerPlants.clear();
	m_Impl->sensors.clear();
	m_Impl->drives.clear();
	QVector3D minimum, maximum;
	bool first = true;
	for(const Room& room : design.rooms)
		for(const QPointF& point : RoomBoundary(room))
		{
			const QVector3D lower(point.x(), point.y(), room.deck * DeckHeight);
			const QVector3D upper(point.x(), point.y(), (room.deck + 1) * DeckHeight);
			if(first)
			{
				minimum = lower;
				maximum = upper;
				first = false;
			}
			else
				for(int axis = 0; axis < 3; ++axis)
				{
					minimum[axis] = std::min(minimum[axis], lower[axis]);
					maximum[axis] = std::max(maximum[axis], upper[axis]);
				}
		}
	m_Impl->shipCenter = (minimum + maximum) * .5f;
	for(const Detail& detail : design.details)
	{
		if(detail.hiddenRaw || detail.deck < MinimumDeck || detail.deck > MaximumDeck)
			continue;
		const QPointF point = DecodePoint(detail.packedXY);
		const QVector3D location(point.x(), point.y(), detail.deck * DeckHeight + 4);
		switch(detail.detailType)
		{
		case 10: m_Impl->drives.append(location); break;
		case 14: m_Impl->powerPlants.append(location); break;
		case 16: m_Impl->sensors.append(location); break;
		default: break;
		}
	}
}

void SoundSession::DoorTransition(const DoorDefinition& door, bool opening)
{
	if(!IsDoorDetail(door.type))
		return;
	int channel;
	SOUND_EFFECT sound;
	if(door.type == 3 || door.type == 53 || door.type == 54)
	{
		channel = CargoDoor;
		sound = opening ? SOUND_DoorCargoOpen : SOUND_DoorCargoClose;
	}
	else if(door.type == 50)
	{
		channel = ExteriorDoor;
		sound = opening ? SOUND_DoorExtOpen : SOUND_DoorExtClose;
	}
	else if(IsHatchDetail(door.type))
	{
		channel = Hatch;
		sound = opening ? SOUND_DoorHatchOpen : SOUND_DoorHatchClose;
	}
	else
	{
		channel = InteriorDoor;
		sound = opening ? SOUND_DoorIntOpen : SOUND_DoorIntClose;
	}
	const QPointF center = (door.a + door.b) * .5;
	m_Impl->pending[channel].sound = sound;
	m_Impl->pending[channel].position = QVector3D(center.x(), center.y(),
		door.deck * DeckHeight + (channel == Hatch ? 0 : 4));
}

void SoundSession::Update(const SoundListener& listener)
{
	if(!m_Impl->IsReady())
		return;
	m_Impl->eye = listener.eye;
	m_Impl->forward = listener.forward.normalized();
	m_Impl->right = QVector3D::crossProduct(m_Impl->forward, listener.up).normalized();
	m_Impl->up = QVector3D::crossProduct(m_Impl->right, m_Impl->forward).normalized();
	for(int channel = CargoDoor; channel <= InteriorDoor; ++channel)
	{
		DoorSound& pending = m_Impl->pending[channel];
		Source& source = m_Impl->sources[channel];
		if(pending.sound != SOUND_None)
		{
			const float gain = channel == Hatch ? (listener.inShip ? 1.f : 2.f)
				: channel == InteriorDoor ? 2.f : (listener.inShip ? 2.f : 10.f);
			m_Impl->Play(source, pending.sound,
				listener.inShip ? pending.position : m_Impl->shipCenter, gain, 1, false, true);
			pending.sound = SOUND_None;
		}
		else if(source.id)
		{
			if(!m_Impl->audio->IsSoundPlaying(source.id))
				m_Impl->StopSource(source);
			else
				m_Impl->Position(source);
		}
	}
	Source& footsteps = m_Impl->sources[Footsteps];
	if(listener.moving && (listener.grounded || listener.climbing))
	{
		const float speed = listener.climbing || listener.crouching ? .5f : listener.prone ? 1.f : listener.running ? 2.f : 1.f;
		// Default 2016 human DNA Speed = 4. The local preview uses one pair
		// of feet; the original scales loudness by sqrt(body volume) / 2.
		const float pitch = 1.5f * speed / (1.f + 4.f / 5.f);
		const SOUND_EFFECT sound = listener.prone
			? (listener.inShip ? SOUND_DragMetal : SOUND_DragHardFloor)
			: (listener.inShip ? SOUND_FootstepsMetal : SOUND_FootstepsHardFloor);
		m_Impl->Play(footsteps, sound, listener.feet, listener.prone ? .25f : 1.f,
			pitch, true, false, 0, 1, true);
	}
	else
		m_Impl->StopSource(footsteps);

	if(listener.powerPlantWorking && !m_Impl->powerPlants.isEmpty())
		m_Impl->Play(m_Impl->sources[PowerPlant], SOUND_PowerPlant,
			listener.inShip ? Nearest(m_Impl->powerPlants, listener.eye) : m_Impl->shipCenter,
			listener.inShip ? .5f : 1.f, 1, true, false, listener.inShip ? .0125f : 0);
	else
		m_Impl->StopSource(m_Impl->sources[PowerPlant]);
	if(listener.sensorsWorking && !m_Impl->sensors.isEmpty())
		m_Impl->Play(m_Impl->sources[Sensors], SOUND_Telemetry,
			listener.inShip ? Nearest(m_Impl->sensors, listener.eye) : m_Impl->shipCenter, 1, 1, true);
	else
		m_Impl->StopSource(m_Impl->sources[Sensors]);
	if((listener.driveState == 1 || listener.driveState == 2) && !m_Impl->drives.isEmpty())
	{
		const bool hold = listener.driveState == 2;
		m_Impl->Play(m_Impl->sources[Drive], listener.inShip ? SOUND_GravityDriveInt : SOUND_GravityDriveExt,
			listener.inShip ? Nearest(m_Impl->drives, listener.eye) : m_Impl->shipCenter,
			listener.inShip ? (hold ? .3f : 1.f) : (hold ? 5.f : 10.f), 1, true, false,
			listener.inShip ? (hold ? .0125f : .025f) : 0, hold ? .2f : 1.f);
	}
	else
		m_Impl->StopSource(m_Impl->sources[Drive]);
}

void SoundSession::Stop()
{
	for(DoorSound& pending : m_Impl->pending)
		pending.sound = SOUND_None;
	for(Source& source : m_Impl->sources)
	{
		if(m_Impl->IsReady())
			m_Impl->StopSource(source);
		else
		{
			// A replaced/destroyed game context already owns cleanup of its
			// sources. Never send their old IDs to a different context.
			source.id = 0;
			source.sound = SOUND_None;
		}
	}
}
}
}
