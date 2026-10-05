#include "aulegacyshiprender.h"
#include "aulegacyship.h"
#include "aulegacyshipmesh.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipwalk.h"
#include "aulegacyshiphud.h"
#include "aulegacyshipperformance.h"
#include "aulegacyshipsound.h"
#include "aulegacyshipatlas.h"
#include "aulegacyshipeffects.h"
#include "aulegacyshiphelm.h"
#include "auapp.h"
#include "auclientglobal.h"
#include "augleffects.h"
#include "audesignerframe.h"
#include "audesignerview.h"
#include "daperson.h"
#include "dadesigner.h"
#include "auether.h"
#include "auqueryenvironment.h"
#include "augear.h"
#include "dicontrolinput.h"
#include "aujoystick.h"
#include "auNebula.h"
#include "dagalaxy.h"
#include "dhdna.h"
#include "dpconsole.h"
#include "dpconsolehelm.h"
#include "dpconsoleengineer.h"
#include "dasc.h"
#include "aucommpacket.h"
#include "auquad.h"
#include "audice.h"
#include "auid.h"
#include "psenginetrail.h"
#include "dgdata.h"

#include <QByteArray>
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QImage>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QTimer>
#include <QThread>
#include <QKeyEvent>
#include <QMap>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPainter>
#include <QPixmap>
#include <QSet>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector3D>
#include <QWheelEvent>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <map>
#include <mutex>
#include <vector>

namespace
{
const float Pi = 3.14159265358979323846f;
using namespace AuLegacyShip::Rendering;
const qint64 DoorTickMilliseconds = qint64(DoorAnimationTickSeconds * 1000);

enum SystemAction { PowerToggle, DriveToggle, DriveCycle, DriveHold };

bool IsStudioSurface(const Batch& batch)
{
	// The original Spacecraft shader renders every procedural atlas surface
	// with alpha testing and depth writes, including equipment and doors.
	return batch.textureFile.isEmpty() && int(batch.material) >= 0;
}

struct SystemBindings
{
	DiControlInputArray controls;
	QHash<int, int> actions;
};

template<class Station>
SystemBindings LoadSystemBindings(const Station& station)
{
	SystemBindings result;
	result.actions.insert(Station::CI_PowerPlantOnOff, PowerToggle);
	result.actions.insert(Station::CI_GravityDriveOnOff, DriveToggle);
	result.actions.insert(Station::CI_GravityDriveOnOffHold, DriveCycle);
	result.actions.insert(Station::CI_GravityDriveHold, DriveHold);
	DiControlInputArray configured;
	station.EnumControlInputs(configured);
	for(const DiControlInput& control : configured)
		if(result.actions.contains(control.GetId())) result.controls.append(control);
	return result;
}

struct SkyJob
{
	std::atomic<bool> cancelled{false};
	std::mutex mutex;
	GLuint texture = 0;
	bool done = false;
	QString error;

	void Cancel(AuGLEffects& re)
	{
		std::lock_guard<std::mutex> lock(mutex);
		cancelled = true;
		if(texture) re.glDeleteTextures(1, &texture);
		texture = 0;
	}
};

// Only the completed texture crosses contexts. VAOs, shader uniforms and
// galaxy data remain exclusively on the worker, which never accesses a widget.
class SkyThread : public QThread
{
public:
	std::shared_ptr<SkyJob> job;
	QOpenGLContext* glContext;
	QOffscreenSurface* surface;
	unsigned int paneSize;
	bool mipMap;

	SkyThread(const std::shared_ptr<SkyJob>& work, QOpenGLContext* context,
		QOffscreenSurface* target, unsigned int size, bool mip)
		: job(work), glContext(context), surface(target), paneSize(size), mipMap(mip)
	{
		glContext->moveToThread(this);
		connect(this, &QThread::finished, surface, &QObject::deleteLater);
		connect(this, &QThread::finished, this, &QObject::deleteLater);
	}

	void run() override
	{
		QString error;
		if(!job->cancelled && glContext->makeCurrent(surface))
		{
			{
				AuGLEffects re;
				if(re.initializeOpenGLFunctions())
				{
					if(re.GLSL().InitializeSkyBox(re)) Generate(re, error);
					else error = QStringLiteral("Unable to initialize preview skybox shaders.");
					re.GLSL().DeleteSkyBox();
				}
				else error = QStringLiteral("Unable to initialize preview skybox OpenGL functions.");
			}
			glContext->doneCurrent();
		}
		else if(!job->cancelled) error = QStringLiteral("Unable to activate the preview skybox context.");
		delete glContext;
		std::lock_guard<std::mutex> lock(job->mutex);
		job->error = error;
		job->done = true;
	}

	void Generate(AuGLEffects& re, QString& error)
	{
		DaGalaxy galaxy(ShoresOfHazeron);
		AuDice dice;
		dice.Seed(2016);
		AuPoint sector;
		for(int attempt = 0; attempt < 128; ++attempt)
		{
			const int radius = dice.Roll(1, int(galaxy.Radius() / 10));
			sector = AuPoint(radius, 0).RotateZ(dice.RandomMinMax(0, 2 * Pi));
			sector.Round(1);
			if(galaxy.StarDensity(sector * 10) >= .2) break;
		}
		auNebula sky(0, 0, -2.9924924f, .38151205f, -1.8401746f, -1.8524430f,
			30, 0, false, AuPoint(), 2016, 0, galaxy.Color());
		const int radius = int(galaxy.Radius() / 10);
		for(int x = -radius; x <= radius && !job->cancelled;)
			x = sky.AppendGalacticStarFieldSlice(galaxy, sector, dice, x);
		re.glDisable(GL_DEPTH_TEST);
		re.glDisable(GL_CULL_FACE);
		re.glDisable(GL_SCISSOR_TEST);
		re.glActiveTexture(GL_TEXTURE0);
		int face = 0;
		for(; face < 6 && !job->cancelled; ++face)
		{
			if(!sky.CreateSkyBoxFace(re, sector, paneSize, mipMap, face))
			{
				error = QStringLiteral("Unable to create the spacecraft preview skybox.");
				break;
			}
		}
		if(face == 6 && !job->cancelled)
		{
			// Finish here before publishing; painting never waits for the worker.
			re.glFinish();
			std::lock_guard<std::mutex> lock(job->mutex);
			if(!job->cancelled)
			{
				job->texture = sky.m_img;
				sky.m_img = 0;
			}
		}
		sky.DeleteDeviceObjects(re);
		// AuGLTexture's destructor intentionally does not delete GL names.
		// These caches belong only to this worker, even in the shared group.
		const QString textureIds[] = {QStringLiteral("StarGlow"), QStringLiteral("StarSpriteDot")};
		for(const QString& textureId : textureIds)
		{
			if(auto* texture = re.FindTexture(textureId))
			{
				const GLuint name = texture->GlName();
				re.glDeleteTextures(1, &name);
				re.DeleteTexture(STATIC_TEXTURE_ID, textureId);
			}
		}
	}
};

struct TextureRegion
{
	int x, y, width, height;
};

// Original SHIP_TEXTURE order and bottom-left atlas coordinates, also retained
// by AuBodyRenderWidget::ShipTexture. Coordinates use the 128 x 4096 atlas.
const TextureRegion ShipTextures[] = {
	{ 0, 480, 128, 32 }, { 0, 480, 24, 12 },
	{ 0, 2176, 128, 128 }, { 0, 2048, 128, 128 }, { 0, 1920, 128, 128 },
	{ 0, 2560, 128, 128 }, { 0, 2432, 128, 128 }, { 0, 2304, 128, 128 },
	{ 0, 2944, 128, 128 }, { 0, 2816, 128, 128 }, { 0, 2688, 128, 128 },
	{ 0, 1536, 64, 128 }, { 64, 1536, 64, 128 }, { 0, 1408, 64, 128 },
	{ 0, 896, 64, 64 }, { 64, 960, 64, 64 }, { 0, 960, 64, 64 },
	{ 32, 576, 32, 32 }, { 0, 3456, 128, 128 },
	{ 0, 1792, 64, 128 }, { 64, 1792, 64, 128 }, { 0, 1664, 64, 128 },
	{ 0, 1024, 64, 64 }, { 64, 1088, 64, 64 }, { 0, 1088, 64, 64 },
	{ 0, 3584, 128, 128 }, { 0, 3200, 128, 128 },
	{ 0, 3712, 128, 128 }, { 0, 3328, 128, 128 }, { 0, 0, 128, 128 },
	{ 0, 1280, 64, 128 }, { 64, 1280, 64, 128 }, { 0, 1152, 64, 128 },
	{ 0, 3840, 128, 128 },
	{ 0, 768, 64, 64 }, { 64, 832, 64, 64 }, { 0, 832, 64, 64 },
	{ 96, 1152, 32, 128 }, { 0, 3968, 128, 128 }, { 64, 1152, 32, 128 },
	{ 0, 256, 128, 128 }, { 96, 1408, 32, 128 },
	{ 0, 128, 128, 128 }, { 64, 1408, 32, 128 },
	{ 0, 640, 64, 128 }, { 0, 640, 11, 128 },
	{ 64, 544, 32, 32 }, { 32, 608, 32, 32 },
	{ 96, 624, 32, 16 }, { 96, 608, 16, 16 }, { 64, 576, 32, 32 },
	{ 0, 512, 32, 128 }, { 0, 512, 2, 2 }, { 32, 512, 32, 32 },
	{ 64, 640, 64, 128 }, { 64, 608, 32, 32 }, { 0, 3072, 128, 128 },
	{ 64, 512, 32, 32 }, { 32, 544, 32, 32 },
	{ 0, 416, 128, 32 }, { 0, 412, 128, 36 }, { 0, 384, 128, 32 }
};

const TextureRegion EffectTextures[] = {
	{ 352, 0, 128, 32 },
	{ 352, 32, 128, 32 },
	{ 352, 64, 128, 32 },
	{ 352, 96, 128, 32 },
	{ 96, 96, 32, 32 },
	{ 96, 64, 32, 32 },
	{ 96, 32, 32, 32 },
	{ 96, 0, 32, 32 },
	{ 224, 96, 32, 32 },
	{ 224, 64, 32, 32 },
	{ 224, 32, 32, 32 },
	{ 224, 0, 32, 32 },
	{ 160, 96, 32, 32 },
	{ 160, 64, 32, 32 },
	{ 160, 32, 32, 32 },
	{ 160, 0, 32, 32 },
	{ 32, 96, 32, 32 },
	{ 32, 64, 32, 32 },
	{ 32, 32, 32, 32 },
	{ 32, 0, 32, 32 },
	{ 128, 96, 32, 32 },
	{ 128, 64, 32, 32 },
	{ 128, 32, 32, 32 },
	{ 128, 0, 32, 32 },
	{ 256, 0, 32, 128 },
	{ 288, 0, 32, 128 },
	{ 320, 0, 32, 128 },
	{ 0, 96, 32, 32 },
	{ 0, 64, 32, 32 },
	{ 0, 32, 32, 32 },
	{ 0, 0, 32, 32 },
	{ 768, 96, 32, 32 },
	{ 768, 64, 32, 32 },
	{ 768, 32, 32, 32 },
	{ 768, 0, 32, 32 },
	{ 0, 0, 32, 32 },
	{ 64, 96, 32, 32 },
	{ 64, 64, 32, 32 },
	{ 64, 32, 32, 32 },
	{ 64, 0, 32, 32 },
	{ 480, 0, 64, 128 },
	{ 544, 0, 64, 128 },
	{ 608, 0, 64, 128 },
	{ 736, 96, 32, 32 },
	{ 736, 64, 32, 32 },
	{ 736, 32, 32, 32 },
	{ 736, 0, 32, 32 },
	{ 704, 96, 32, 32 },
	{ 704, 64, 32, 32 },
	{ 704, 32, 32, 32 },
	{ 704, 0, 32, 32 },
	{ 672, 96, 32, 32 },
	{ 672, 64, 32, 32 },
	{ 672, 32, 32, 32 },
	{ 672, 0, 32, 32 },
	{ 192, 96, 32, 32 },
	{ 192, 64, 32, 32 },
	{ 192, 32, 32, 32 },
	{ 192, 0, 32, 32 },
	{ 832, 96, 32, 32 },
	{ 832, 64, 32, 32 },
	{ 832, 32, 32, 32 },
	{ 832, 0, 32, 32 }
};

struct RenderVertex
{
	float position[3];
	float normal[3];
	float uv[2];
};

struct DrawBatch
{
	int source;
	int first;
	int count;
	int deck;
};

struct TransparentTriangle
{
	int source;
	int first;
	int chunk;
	QVector3D center;
};

struct DrawChunk
{
	DrawBatch draw;
	QVector3D center;
	float radius;
};

struct BatchTexture
{
	QOpenGLTexture* texture = nullptr;
	int material = -1;
	int variant = -1;
	bool translucent = false;
};

QVector3D Vector3(const AuPoint& point)
{
	return QVector3D(float(point.x), float(point.y), float(point.z));
}

AuPoint Point3(const QVector3D& point)
{
	return AuPoint(point.x(), point.y(), point.z());
}

class LegacyRenderPerson : public DaPerson
{
public:
	using DaPerson::DaPerson;

	AuPoint Acceleration() const override
	{
		const unsigned int actions = Actions();
		if(IsInGravity() || IsInSpacecraft() || IsLadderPose() || IsLockedPose() || IsRider()
			|| (actions & ACT_Brake) || !(actions & (ACT_MoveUp | ACT_MoveDown)))
			return DaPerson::Acceleration();

		// 2016 solid-render movement (18031be50), including normalization:
		// a lone C or Z gets the full movement force, despite its .5/-1 weight.
		AuPoint direction(0, 0, 0);
		if(actions & ACT_MoveForward) direction.y = 1;
		else if(actions & ACT_MoveBackward) direction.y = -1;
		direction = direction.RotateX(CurrentHeadTilt());
		if(actions & ACT_MoveLeft) direction.x -= .75;
		else if(actions & ACT_MoveRight) direction.x += .75;
		direction.z += (actions & ACT_MoveDown) ? -1.0 : .5;
		if(!direction.IsZero()) direction.Unit(MovementSpeed(m_RenderMovePower / 100.f));
		return direction;
	}

	void TickMovement(const AuElapsedTimer& last) override
	{
		const unsigned int movement = Actions() & ACT_MoveMask;
		if(!movement) m_RenderMovePower = 0;
		DaPerson::TickMovement(last);
		// Original DecayMoveActions (18032c740) adds 10%, not the modern
		// controller's 1/15 step, and resets power when any move axis expires.
		if(movement & ~Actions()) m_RenderMovePower = 0;
		else if(movement) m_RenderMovePower = std::min(100, m_RenderMovePower + 10);
	}

	void ClearMovementInput()
	{
		DaPerson::ClearMovementInput();
		m_RenderMovePower = 0;
	}

private:
	int m_RenderMovePower = 0;
};

// The native designer interface supplies environment and movement queries.
// Only the geometry behind those queries is specific to the legacy ship.
class LegacyDesignEnvironment : public DaDesigner
{
public:
	LegacyDesignEnvironment() : DaDesigner(0) {}
	std::function<void(AuQueryEnvironment&, const DaAtom*)> query;
	std::function<bool(DaAtom*, const AuPoint&, int)> move;
	void GetAtomEnvironment(AuQueryEnvironment& result, const DaAtom* atom) const override { query(result, atom); }
	bool MoveSubatom(DaAtom* atom, const AuPoint& displacement, int milliseconds) override
	{
		return move(atom, displacement, milliseconds);
	}
	bool IsRenderSolid(const AuId&) const override { return true; }
};
}

class AuLegacyShipRender::Impl
{
public:
	QWidget* floorPlan = nullptr;
	bool Walkthrough() const { return !floorPlan || floorPlan->isHidden(); }

	AuLegacyShip::Rendering::Mesh mesh;
	AuLegacyShip::Design design;
	WalkWorld world;
	WalkState character;
	AuEther characterEther{"Legacy designer character"};
	LegacyRenderPerson* person = nullptr;
	LegacyDesignEnvironment* environment = nullptr;
	AuElapsedTimer personClock;
	DpConsole console;
	QHash<quint32, int> consoleScreens;
	SoundSession sound;
	QList<AuGLSun> studioSuns;
	std::vector<std::unique_ptr<PsEngineTrail>> contrails;
	GLuint contrailStreak = 0, contrailGlow = 0;
	std::unique_ptr<auNebula> sky;
	std::shared_ptr<SkyJob> skyJob;
	QString skyError;
	unsigned int skyPaneSize = 0;
	bool skyMipMap = false;
	QVector<DoorDefinition> doors;
	QHash<quint32, DoorAnimation> doorStates;
	std::unordered_map<Qt::Key, AuElapsedTimer, AuKey_Hash> keys;
	QSet<int> releasedKeys, handKeys;
	DiControlInputArray controls, pendingControls, sceneControls, pendingSceneControls;
	SystemBindings systemBindings[3]; // No station, helm, engineering.
	bool shipNew = true;
	AuId shipId;
	QString shipName;
	QString targetName;
	bool powerPlantOn = false;
	int driveState = 0; // 2016 GD_Off, GD_On, GD_Hold.
	DiControlInput handControl, toggleControl;
	Qt::MouseButtons mouseButtons = Qt::NoButton;
	Qt::MouseButtons pendingMouseButtons = Qt::NoButton;
	QVector<DrawChunk> chunks;
	QVector<quint8> visibleChunks;
	QVector<DrawBatch> visibleBatches;
	QVector<TransparentTriangle> transparentTriangles;
	QVector<DrawBatch> transparentBatches;
	QVector<BatchTexture> batchTextures;
	QVector<QMatrix4x4> transforms;
	QMatrix4x4 cachedViewProjection;
	quint32 cachedConsoleId = 0;
	bool visibilityValid = false;
	bool transparentOrderDirty = true;
	bool hasAnimatedGeometry = false;
	bool transparentBufferReady = false;
	const bool profileRender = qEnvironmentVariableIsSet("HAZERON_LEGACY_RENDER_STATS");
	qint64 profileLastReport = 0, profileNanoseconds = 0;
	quint64 profileFrames = 0, profileDraws = 0, profileVertices = 0, profileSorts = 0;
	QOpenGLBuffer buffer;
	QOpenGLBuffer transparentBuffer{QOpenGLBuffer::IndexBuffer};
	QOpenGLVertexArrayObject vao;
	QOpenGLShaderProgram* program = nullptr;
	QMap<QString, QOpenGLTexture*> textures;
	QSet<QString> transparentTextures;
	QOpenGLTexture* effectTexture = nullptr;
	QImage atlas;
	QElapsedTimer animationClock;
	QTimer* animationTimer = nullptr;
	QMetaObject::Connection contextConnection;
	QPoint mouseAnchor;
	QPoint mousePressed;
	bool handClick = false;
	bool mouseCaptured = false;
	bool freeMouse = false;
	bool temporaryHand = false;
	bool running = false;
	QString glError;
	QStringList textureWarnings;
	float radius = 10.0f;
	float azimuth = 90.0f;
	float elevation = 0.0f;
	int deck = 0;
	bool dirty = true;
	bool ready = false;
	bool gravity = false;
	bool haveDesign = false;
	qint64 lastTick = 0;
	qint64 lastDoorTick = 0;
	qint64 lastTargetTick = 0;
	bool hasTarget = false;
	int targetLevel = 0;
	quint32 targetDetail = 0;
	AuLegacyShip::Stats designStats;
	AuLegacyShip::Performance performance;
	AuLegacyShip::TargetHealth targetHealth;
	QVector3D targetPoint;
	QString interaction;
	QString reportedTips;
	qint64 interactionUntil = 0;

	bool HandInput(Qt::KeyboardModifiers modifiers)
	{
		bool down = handKeys.contains(handControl.Key());
		switch(handControl.Key())
		{
		case Qt::Key_Control: down = modifiers.testFlag(Qt::ControlModifier); break;
		case Qt::Key_Shift: down = modifiers.testFlag(Qt::ShiftModifier); break;
		case Qt::Key_Alt: down = modifiers.testFlag(Qt::AltModifier); break;
		case Qt::Key_Meta: down = modifiers.testFlag(Qt::MetaModifier); break;
		default: break;
		}
		return (down && modifiers == handControl.KeyModifier())
			|| handControl.TestMouseTriggered(QApplication::mouseButtons(), modifiers, QPoint());
	}

	bool MatchesKey(const DiControlInput& control, const QKeyEvent* event) const
	{
		return control.Key() == event->key() && control.KeyModifier() == event->modifiers();
	}

	bool HasKeyBinding(const QKeyEvent* event) const
	{
		for(const DiControlInput& control : controls)
			if(MatchesKey(control, event)) return true;
		for(const DiControlInput& control : sceneControls)
			if(MatchesKey(control, event)) return true;
		for(const DiControlInput& control : systemBindings[SystemBindingIndex()].controls)
			if(MatchesKey(control, event)) return true;
		return MatchesKey(handControl, event) || MatchesKey(toggleControl, event);
	}

	int SystemBindingIndex() const
	{
		return character.consoleType == 12 ? 1 : character.consoleType == 6 ? 2 : 0;
	}

	QVector3D CameraDirection() const
	{
		const float yaw = azimuth * Pi / 180.0f;
		const float pitch = elevation * Pi / 180.0f;
		return QVector3D(std::cos(pitch) * std::cos(yaw),
			std::cos(pitch) * std::sin(yaw), std::sin(pitch));
	}

	QVector3D Eye() const
	{
		return person ? Vector3(person->Transform().Project(person->CameraOffset()))
			: character.feet + QVector3D(0, 0, world.Settings().eyeHeight);
	}

	struct CameraView { QVector3D eye, forward, up; };
	CameraView Camera(AuAtomRenderInfo& ari) const
	{
		AuMatrix4x4 camera;
		double nearClip = .1, farClip = 1000000;
		person->SetupCamera(camera, nearClip, farClip, ari);
		camera = person->Transform() * camera;
		return {Vector3(camera.Project(AuPoint(0, 0, 0))),
			Vector3(camera.ProjectVector(AuPoint(0, 0, -1))),
			Vector3(camera.ProjectVector(AuPoint(0, 1, 0)))};
	}

	void InitializeCharacter()
	{
		if(person) return;
		environment = new LegacyDesignEnvironment;
		environment->Create();
		environment->SetNormal();
		environment->query = [this](AuQueryEnvironment& result, const DaAtom* atom) {
			const auto* creature = static_cast<const DaPerson*>(atom);
			const QVector3D feet = Vector3(creature->Location()) - QVector3D(0, 0, creature->CGHeight());
			const WalkEnvironment sample = world.Environment(feet, gravity, creature->CGHeight());
			const ON_GROUND ground = sample.gravity && !sample.gravityWell && sample.fallDistance < .25f && !character.ladderEngaged
				? (sample.inShip ? OG_Metal : OG_HardFloor) : OG_None;
			result.SetDesignScene(sample.gravity, sample.inShip, ROOMTYPE(sample.roomType), sample.gravityWell, character.ladderEngaged,
				sample.fallDistance, atom->Transform().UnProjectVector(AuPoint(0, 0, 1)), ground);
		};
		environment->move = [this](DaAtom* atom, const AuPoint& delta, int milliseconds) {
			auto* creature = static_cast<DaPerson*>(atom);
			atom->SetMoveResult(DaAtom::CT_HitParent);
			if(character.consoleType) { atom->SetVector(AuPoint(0, 0, 0)); return false; }
			const QVector3D start = Vector3(atom->Location()) - QVector3D(0, 0, creature->CGHeight());
			QVector3D displacement = Vector3(delta);
			// Entering Jump halves the native CG height. Its zero-time stance
			// adjustment would lower the entire body to keep the feet planted,
			// cancelling the visible rise of a walking jump. Tuck the legs up
			// around the existing origin instead; retain the native jump velocity.
			if(milliseconds == 0 && creature->IsJumping() && displacement.z() < 0)
				displacement.setZ(0);
			if(character.ladderEngaged) { displacement.setX(0); displacement.setY(0); }
			const QVector3D destination = start + displacement;
			const float eyeHeight = creature->CGHeight() + float(creature->CameraOffset().z);
			if(!world.TrySetBodySize(destination, creature->PhysicalHeight(), eyeHeight, milliseconds == 0)) return false;
			if(milliseconds == 0)
			{
				// Commit the checked pose position without replacing velocity.
				character.feet = destination;
			}
			else
			{
				character.feet = start;
				character.velocity = displacement * (1000.f / milliseconds);
				world.MoveDisplacement(character, displacement, gravity);
			}
			AuAtomPos position(atom->ParentId(), Point3(character.feet + QVector3D(0, 0, creature->CGHeight())), atom->Rotation());
			atom->ChangePos(position);
			// ChangePos invalidates transforms, but not cached floor/contact data.
			// Native TickMovement checks landing again before returning.
			atom->InvalidateEnvironment();
			const bool complete = (character.feet - destination).lengthSquared() < 1e-8f;
			atom->SetMoveResult(complete ? DaAtom::CT_Miss : DaAtom::CT_HitParent);
			if(!complete) atom->SetVector(atom->Transform().UnProjectVector(Point3(character.velocity)));
			return complete;
		};
		characterEther.AddAtom(environment);
		DhDNA dna;
		dna.InitDefaultHuman();
		person = new LegacyRenderPerson(dna, AuDNABase::Alpha, RightHanded, QStringLiteral("Designer"), Mature, 0);
		person->Create();
		person->Stand();
		person->ResetHitPoints();
		person->ResetHunger();
		person->ResetStamina();
		person->ClearSpawnCity();
		person->SetPosAndOrient(AuAtomPos(environment->Id(), AuPoint(0, 0, person->CGHeight()), AuPoint(0, 0, 0)));
		characterEther.AddAtom(person);
		person->SetNormal();
		QList<AuGLLight> lights;
		environment->GetRenderLightsRecursive(*person, AuMatrix4x4(), studioSuns, lights);
		personClock.start();
		SyncCharacter();
	}

	void PlaceCharacter()
	{
		person->ChangePosAndOrient(AuAtomPos(environment->Id(),
			Point3(character.feet + QVector3D(0, 0, person->CGHeight())),
			AuPoint(0, 0, (azimuth - 90.f) * Pi / 180.f)));
		person->TiltHeadTo(elevation * Pi / 180.f);
		person->SetVector(person->Transform().UnProjectVector(Point3(character.velocity)));
		person->InvalidateEnvironment();
		person->SetInMotion();
		personClock.restart();
		SyncCharacter();
	}

	void SyncCharacter()
	{
		if(!person) return;
		character.feet = Vector3(person->Location()) - QVector3D(0, 0, person->CGHeight());
		character.velocity = Vector3(person->Transform().ProjectVector(person->Vector()));
		const WalkEnvironment sample = world.Environment(character.feet, gravity, person->CGHeight());
		character.inShip = sample.inShip;
		character.gravityActive = sample.gravity && !sample.gravityWell;
		character.gravitySuspended = !character.gravityActive;
		character.grounded = character.gravityActive && !character.ladderEngaged && sample.fallDistance < .25f;
		character.onLadder = character.ladderEngaged;
		azimuth = float(person->Rotation().z) * 180.f / Pi + 90.f;
		elevation = person->CurrentHeadTilt() * 180.f / Pi;
		running = person->CurrentGeneralPose() == AuPose::StandRun;
		WalkSettings settings = world.Settings();
		settings.height = person->PhysicalHeight();
		settings.eyeHeight = person->CGHeight() + float(person->CameraOffset().z);
		world.SetSettings(settings);
		world.SetConsoleRotation(character.consoleId, character.feet, azimuth);
	}

	void ExitConsole()
	{
		character.consoleType = 0;
		character.consoleId = 0;
		person->ClearMovementInput();
		person->ExitConsole();
		SyncCharacter();
	}

	void SetInteractionPose()
	{
		AuPose pose = character.ladderEngaged ? AuPose::Ladder : AuPose::Stand;
		switch(character.consoleType)
		{
		case 1: pose = AuPose::ConsoleNav; break;
		case 4: pose = AuPose::ConsoleSurgery; break;
		case 6: pose = AuPose::ConsoleEngine; break;
		case 7: pose = AuPose::ConsoleFireControl; break;
		case 12: pose = AuPose::ConsolePilot; break;
		case 13: pose = AuPose::ConsolePower; break;
		case 15: pose = AuPose::ConsoleSensor; break;
		case 17: pose = AuPose::ConsoleShield; break;
		case 19: pose = AuPose::ConsoleTransporter; break;
		case 73: pose = AuPose::ConsoleCommand; break;
		default: break;
		}
		person->ClearMovementInput();
		person->SetGeneralPose(pose);
		PlaceCharacter();
	}

	QMatrix4x4 ConsoleTransform() const
	{
		QMatrix4x4 transform;
		transform.translate(character.feet);
		transform.rotate(azimuth - 90.f, 0, 0, 1);
		return transform;
	}

	void RenderConsole(AuAtomRenderInfo& ari, const QMatrix4x4& projection, const QMatrix4x4& view)
	{
		if(!character.consoleType) return;
		AuGLEffects& re = *ari.re;
		AuMatrix4x4 p, model;
		for(int i = 0; i < 16; ++i)
		{
			p[i] = projection.constData()[i];
			model[i] = view.constData()[i];
		}
		re.glEnable(GL_DEPTH_TEST);
		re.glDepthMask(GL_TRUE);
		re.glEnable(GL_CULL_FACE);
		re.glCullFace(GL_BACK);
		re.glFrontFace(GL_CW);
		re.glDisable(GL_BLEND);
		re.DisableSuns();
		re.DisableLights();
		re.SetAmbientLight(0.014352942f, 0.015f, 0.014705883f, 1);
		for(int i = 0; i < std::min(3, studioSuns.size()); ++i)
		{
			AuGLSun sun = studioSuns[i];
			const QVector3D position = view.map(QVector3D(sun.m_LocX, sun.m_LocY, sun.m_LocZ));
			sun.m_LocX = position.x();
			sun.m_LocY = position.y();
			sun.m_LocZ = position.z();
			re.SetSun(i, sun);
		}
		if(character.consoleType)
		{
			// DpConsole is also used by the modern designer. Supply the
			// operator's floor transform; it owns the chair/panel placement
			// and the user's screen distance, height and tilt adjustments.
			const QMatrix4x4 floorTransform = view * ConsoleTransform();
			for(int i = 0; i < 16; ++i) model[i] = floorTransform.constData()[i];
			if(character.consoleType == 73)
				console.RenderChair(p, model, ari);
			else
			{
				const int screen = consoleScreens.value(character.consoleId, -1);
				console.RenderConsole(p, model, ari, screen >= 0 ? Texture(screen)->textureId() : 0);
			}
		}
		re.glFrontFace(GL_CCW);
		re.glDepthMask(GL_TRUE);
		re.glDisable(GL_BLEND);
	}

	void RebuildContrails()
	{
		contrails.clear();
		// RenderLast's studio branch ignores fuel and drive switches, but
		// requires a classified spacecraft rather than an incomplete hull.
		if(!performance.structure || !designStats.hasHull) return;
		const bool station = performance.structure == 3 || performance.structure == 4;
		QVector<AuPoint> positions;
		for(const auto& detail : design.details)
		{
			if(detail.detailType != 98) continue;
			const QPointF xy = AuLegacyShip::DecodePoint(detail.packedXY);
			positions.append(AuPoint(xy.x(), xy.y() - (station ? 0 : designStats.width / 10),
				detail.deck * AuLegacyShip::DeckHeight + (station ? -4 : 4)));
			if(positions.size() == 10) break;
		}
		if(positions.isEmpty() && !station)
		{
			// Original fallback: center, port and starboard aft emitters.
			const double y = designStats.hullCenter.y() - designStats.length / 2 - designStats.width / 10;
			const double z = (designStats.minimumDeck + designStats.maximumDeck) * AuLegacyShip::DeckHeight / 2;
			for(double x : {0.0, -designStats.width / 3, designStats.width / 3})
				positions.append(AuPoint(designStats.hullCenter.x() + x, y, z));
		}
		const DgData& data = g_Au.LockDataForRead();
		const QColor color = data.EmpireContrailColor(0);
		g_Au.UnlockData();
		for(const AuPoint& position : positions)
		{
			std::unique_ptr<PsEngineTrail> trail(new PsEngineTrail);
			trail->SetMaxParticles(35);
			trail->SetLifeCycle(1.f);
			trail->SetReleaseInterval(0.f);
			trail->SetNumToRelease(1);
			trail->SetColor(color);
			trail->SetPosition(position);
			trail->SetSize(station ? 5000.f : float(designStats.width * 100));
			trail->SetVelocity(station ? AuPoint(0, 0, -25) : AuPoint(0, -500, 0));
			trail->SetParent(environment->Id(), AuMatrix4x4());
			contrails.push_back(std::move(trail));
		}
	}

	void RenderContrails(AuGLEffects& re, const QMatrix4x4& projection, const QMatrix4x4& view)
	{
		if(contrails.empty()) return;
		if(!contrailStreak)
			contrailStreak = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, QStringLiteral("LegacyEngineStreak"),
				QStringLiteral(":/data/EngineStreak.png"));
		if(!contrailGlow)
			contrailGlow = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, QStringLiteral("LegacyEngineGlow"),
				QStringLiteral(":/data/particle.bmp"));
		AuMatrix4x4 p, model;
		for(int i = 0; i < 16; ++i)
		{
			p[i] = projection.constData()[i];
			model[i] = view.constData()[i];
		}
		re.glDepthMask(GL_FALSE);
		re.glEnable(GL_BLEND);
		re.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		for(const auto& trail : contrails)
		{
			trail->SetTextureID(contrailStreak);
			trail->SetTextureID2(contrailGlow);
			if(trail->Update() && trail->GetParticleCount()) trail->Render(p, model, re);
		}
		re.glDisable(GL_BLEND);
		re.glDepthMask(GL_TRUE);
	}

	int FrameMaterial(int source) const
	{
		const Batch& batch = mesh.batches[source];
		return int(batch.doorId ? DoorMaterialFrame(batch.material, doorStates.value(batch.doorId).frame)
			: batch.material);
	}

	void StopMovement(bool preserveVelocity = false)
	{
		keys.clear();
		releasedKeys.clear();
		pendingControls.clear();
		pendingSceneControls.clear();
		mouseButtons = Qt::NoButton;
		pendingMouseButtons = Qt::NoButton;
		running = false;
		if(person)
		{
			person->ClearMovementInput();
			if(!preserveVelocity) person->SetVector(AuPoint(0, 0, 0));
			personClock.restart();
		}
		if(!preserveVelocity) character.velocity = QVector3D();
		handClick = false;
	}

	bool Upload()
	{
		chunks.clear();
		transparentTriangles.clear();
		transparentBatches.clear();
		batchTextures.clear();
		batchTextures.resize(mesh.batches.size());
		transforms = QVector<QMatrix4x4>(mesh.batches.size());
		visibilityValid = false;
		transparentOrderDirty = true;
		transparentBufferReady = false;
		hasAnimatedGeometry = false;
		qint64 vertexCount = 0;
		for (const auto& batch : mesh.batches)
			vertexCount += batch.vertices.size();
		if (vertexCount > std::numeric_limits<int>::max() / qint64(sizeof(RenderVertex)))
		{
			glError = QStringLiteral("This design exceeds the rendering buffer limit.");
			return false;
		}
		QVector<RenderVertex> vertices;
		vertices.reserve(int(vertexCount));
		for (int source = 0; source < mesh.batches.size(); ++source)
		{
			const auto& batch = mesh.batches[source];
			if (batch.vertices.isEmpty())
				continue;
			Texture(source);
			hasAnimatedGeometry |= batch.degreesPerSecond != 0;
			const bool transparent = !IsStudioSurface(batch)
				&& (batch.doorId || batchTextures[source].translucent);
			// Material batches span entire decks. Group their triangles into
			// small spatial chunks once, retaining the original CPU mesh for
			// collision/targeting. Bounds enclose whole triangles, not just the
			// cell, so long walls and triangles crossing cells cannot disappear.
			std::map<std::array<int, 3>, QVector<int>> cells;
			for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
			{
				const QVector3D center = (batch.vertices[i].position
					+ batch.vertices[i + 1].position + batch.vertices[i + 2].position) / 3.f;
				cells[{{int(std::floor(center.x() / 32.f)), int(std::floor(center.y() / 32.f)),
					int(std::floor(center.z() / 32.f))}}].append(i);
			}
			for(const auto& cell : cells)
			{
				DrawChunk chunk;
				chunk.draw = {source, vertices.size(), cell.second.size() * 3, batch.deck};
				QVector3D minimum = batch.vertices[cell.second.first()].position, maximum = minimum;
				for(int first : cell.second)
				{
					if(transparent)
						transparentTriangles.append(TransparentTriangle{source, vertices.size(), chunks.size(),
							(batch.vertices[first].position + batch.vertices[first + 1].position
								+ batch.vertices[first + 2].position) / 3.f});
					for(int i = first; i < first + 3; ++i)
					{
						const Vertex& vertex = batch.vertices[i];
						for(int axis = 0; axis < 3; ++axis)
						{
							minimum[axis] = std::min(minimum[axis], vertex.position[axis]);
							maximum[axis] = std::max(maximum[axis], vertex.position[axis]);
						}
						vertices.append(RenderVertex{{vertex.position.x(), vertex.position.y(), vertex.position.z()},
							{vertex.normal.x(), vertex.normal.y(), vertex.normal.z()},
							{vertex.uv.x(), vertex.uv.y()}});
					}
				}
				chunk.center = (minimum + maximum) * .5f;
				chunk.radius = (maximum - minimum).length() * .5f + .01f;
				chunks.append(chunk);
			}
		}
		if (!buffer.bind())
		{
			glError = QStringLiteral("Unable to bind the spacecraft rendering buffer.");
			return false;
		}
		buffer.allocate(vertices.constData(), vertices.size() * int(sizeof(RenderVertex)));
		const bool allocated = buffer.size() == vertices.size() * int(sizeof(RenderVertex));
		buffer.release();
		if (!allocated)
		{
			glError = QStringLiteral("Unable to upload spacecraft geometry to the graphics device.");
			return false;
		}
		dirty = false;
		glError.clear();
		return true;
	}

	void UpdateVisibility(const QMatrix4x4& viewProjection, float seconds)
	{
		if(visibilityValid && cachedViewProjection == viewProjection
			&& cachedConsoleId == character.consoleId && !hasAnimatedGeometry) return;
		cachedViewProjection = viewProjection;
		cachedConsoleId = character.consoleId;
		visibilityValid = true;
		transparentOrderDirty = true;
		if(hasAnimatedGeometry)
			for(int i = 0; i < transforms.size(); ++i)
			{
				const Batch& batch = mesh.batches[i];
				if(batch.degreesPerSecond == 0) continue;
				QMatrix4x4& transform = transforms[i];
				transform.setToIdentity();
				transform.translate(batch.rotationPivot);
				transform.rotate(std::fmod(seconds * batch.degreesPerSecond, 360.f), batch.rotationAxis);
				transform.translate(-batch.rotationPivot);
			}
		QVector4D planes[6];
		for(int axis = 0; axis < 3; ++axis)
			for(int side = 0; side < 2; ++side)
			{
				QVector4D& plane = planes[axis * 2 + side];
				plane = viewProjection.row(3) + viewProjection.row(axis) * (side ? -1.f : 1.f);
				const float length = plane.toVector3D().length();
				if(length > 0) plane /= length;
			}
		visibleChunks.fill(0, chunks.size());
		visibleBatches.clear();
		for(int i = 0; i < chunks.size(); ++i)
		{
			const DrawChunk& chunk = chunks[i];
			const int source = chunk.draw.source;
			if(character.consoleId && mesh.batches[source].consoleId == character.consoleId) continue;
			const QVector3D center = transforms[source].map(chunk.center);
			bool visible = true;
			for(const QVector4D& plane : planes)
				if(QVector3D::dotProduct(plane.toVector3D(), center) + plane.w() < -chunk.radius)
				{ visible = false; break; }
			if(!visible) continue;
			visibleChunks[i] = 1;
			// A fully visible material still uses one draw call. Only skipped
			// chunks break a contiguous range into additional draws.
			if(!visibleBatches.isEmpty() && visibleBatches.last().source == source
				&& visibleBatches.last().first + visibleBatches.last().count == chunk.draw.first)
				visibleBatches.last().count += chunk.draw.count;
			else visibleBatches.append(chunk.draw);
		}
	}

	void UpdateTransparentOrder(const QVector3D& direction)
	{
		if(!transparentOrderDirty) return;
		struct SortedTriangle { const TransparentTriangle* triangle; float depth; };
		QVector<SortedTriangle> sorted;
		sorted.reserve(transparentTriangles.size());
		for(const TransparentTriangle& triangle : transparentTriangles)
			if(visibleChunks[triangle.chunk] && batchTextures[triangle.source].translucent)
				sorted.append(SortedTriangle{&triangle, QVector3D::dotProduct(
					transforms[triangle.source].map(triangle.center), direction)});
		// Compute each transformed depth once, instead of twice per comparator.
		std::sort(sorted.begin(), sorted.end(), [](const SortedTriangle& a, const SortedTriangle& b) {
			return a.depth > b.depth;
		});
		QVector<quint32> indices;
		indices.reserve(sorted.size() * 3);
		transparentBatches.clear();
		for(const SortedTriangle& entry : sorted)
		{
			const TransparentTriangle& triangle = *entry.triangle;
			if(transparentBatches.isEmpty() || transparentBatches.last().source != triangle.source)
				transparentBatches.append(DrawBatch{triangle.source, indices.size(), 0, mesh.batches[triangle.source].deck});
			transparentBatches.last().count += 3;
			for(int i = 0; i < 3; ++i) indices.append(quint32(triangle.first + i));
		}
		transparentBufferReady = indices.isEmpty();
		if(!indices.isEmpty() && transparentBuffer.bind())
		{
			const int bytes = indices.size() * int(sizeof(quint32));
			transparentBuffer.allocate(indices.constData(), bytes);
			transparentBufferReady = transparentBuffer.size() == bytes;
			transparentBuffer.release();
		}
		transparentOrderDirty = !transparentBufferReady;
		if(!transparentBufferReady)
			glError = QStringLiteral("Unable to upload spacecraft translucent geometry to the graphics device.");
		if(profileRender && !sorted.isEmpty()) ++profileSorts;
	}

	QString TextureKey(int source) const
	{
		const auto& batch = mesh.batches[source];
		if(batch.textureFile == QStringLiteral("legacy:pc_Console.bmp")
			|| batch.textureFile == QStringLiteral(":/data/ec_Console.bmp"))
			return QStringLiteral("%1:power%2:drive%3").arg(batch.textureFile).arg(powerPlantOn).arg(driveState);
		return batch.textureFile.isEmpty() ? QStringLiteral("atlas:%1").arg(FrameMaterial(source)) : batch.textureFile;
	}

	QOpenGLTexture* Texture(int source)
	{
		const auto& batch = mesh.batches[source];
		const int material = FrameMaterial(source);
		if(batchTextures.size() != mesh.batches.size()) batchTextures.resize(mesh.batches.size());
		BatchTexture& cached = batchTextures[source];
		const bool consoleTexture = batch.textureFile == QStringLiteral("legacy:pc_Console.bmp")
			|| batch.textureFile == QStringLiteral(":/data/ec_Console.bmp");
		const int variant = consoleTexture ? int(powerPlantOn) * 3 + driveState : 0;
		if(cached.texture && cached.material == material && cached.variant == variant) return cached.texture;
		const QString key = TextureKey(source);
		const auto remember = [&](QOpenGLTexture* texture) {
			const bool translucent = !IsStudioSurface(batch)
				&& (batch.tint.w() < .999f || transparentTextures.contains(key));
			if(cached.translucent != translucent) transparentOrderDirty = true;
			cached.texture = texture;
			cached.material = material;
			cached.variant = variant;
			cached.translucent = translucent;
			return texture;
		};
		if (textures.contains(key))
			return remember(textures.value(key));
		QImage tile;
		if (batch.textureFile == QStringLiteral("legacy:pc_Console.bmp"))
			tile = QImage::fromData(qUncompress(QByteArray::fromBase64(LegacyShipHelmBmp)), "BMP").mirrored();
		else if (!batch.textureFile.isEmpty())
			tile = QImage(batch.textureFile).mirrored();
		else if (material < 0)
		{
			tile = QImage(1, 1, QImage::Format_RGBA8888);
			tile.fill(Qt::white);
		}
		else if (material >= 0 && material < int(sizeof(ShipTextures) / sizeof(ShipTextures[0])))
		{
			if (atlas.isNull())
			{
				QByteArray encoded;
				encoded.reserve(3502520);
				for (const char* part : LegacyShipAtlasPng)
					encoded.append(part);
				atlas.loadFromData(QByteArray::fromBase64(encoded), "PNG");
				if (atlas.isNull())
				{
					textureWarnings.append(QStringLiteral("The original spacecraft atlas could not be decoded; using the current source atlas."));
					atlas.load(QStringLiteral(":/data/txSpacecraft0Lit.png"));
				}
			}
			if (!atlas.isNull())
			{
				const TextureRegion& region = ShipTextures[material];
				const int x = region.x * atlas.width() / 128;
				const int y = (4096 - region.y - region.height) * atlas.height() / 4096;
				const int width = region.width * atlas.width() / 128;
				const int height = region.height * atlas.height() / 4096;
				tile = atlas.copy(x, y, width, height).mirrored();
			}
		}
		if (tile.isNull())
		{
			const QString warning = QStringLiteral("The original spacecraft texture could not be loaded (%1).")
				.arg(key);
			textureWarnings.append(warning);
			tile = QImage(1, 1, QImage::Format_RGBA8888);
			tile.fill(QColor(175, 185, 195));
		}
		tile = tile.convertToFormat(QImage::Format_ARGB32);
		if(batch.textureFile == QStringLiteral("legacy:pc_Console.bmp")
			|| batch.textureFile == QStringLiteral(":/data/ec_Console.bmp"))
		{
			// Use the native station button artwork and bottom-origin positions.
			// The image is already mirrored for GL, as are these button images.
			QPainter painter(&tile);
			const auto button = [&painter](const char* resource, int x, int y) {
				painter.drawImage(x, y, QImage(QString::fromLatin1(resource)).mirrored());
			};
			if(batch.textureFile == QStringLiteral("legacy:pc_Console.bmp"))
			{
				button(powerPlantOn ? ":/data/pc_PowerPlantOn.bmp" : ":/data/pc_PowerPlantOff.bmp", 218, 90);
				button(driveState == 1 ? ":/data/pc_ManeuverOn.bmp" : driveState == 2
					? ":/data/pc_ManeuverPark.bmp" : ":/data/pc_ManeuverOff.bmp", 236, 90);
			}
			else
			{
				button(powerPlantOn ? ":/data/ec_PowerOn.bmp" : ":/data/ec_PowerOff.bmp", 156, 13);
				button(powerPlantOn ? ":/data/ec_StatusLightGreen.bmp" : ":/data/ec_StatusLightOff.bmp", 157, 32);
				button(driveState == 1 ? ":/data/ec_PowerOn.bmp" : driveState == 2
					? ":/data/ec_PowerPark.bmp" : ":/data/ec_PowerOff.bmp", 156, 90);
				button(driveState == 1 ? ":/data/ec_StatusLightGreen.bmp" : driveState == 2
					? ":/data/ec_StatusLightPurple.bmp" : ":/data/ec_StatusLightOff.bmp", 157, 109);
			}
		}
		for (int y = 0; y < tile.height() && !transparentTextures.contains(key); ++y)
		{
			const QRgb* pixels = reinterpret_cast<const QRgb*>(tile.constScanLine(y));
			for (int x = 0; x < tile.width(); ++x)
			{
				const int alpha = qAlpha(pixels[x]);
				if (alpha < 255)
				{
					transparentTextures.insert(key);
					break;
				}
			}
		}
		QOpenGLTexture* texture = new QOpenGLTexture(tile);
		texture->setWrapMode(QOpenGLTexture::Repeat);
		texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
		texture->setMagnificationFilter(QOpenGLTexture::Linear);
		textures.insert(key, texture);
		return remember(texture);
	}

	void BindEffect(const AuLegacyShip::Rendering::Batch& batch, qint64 elapsed)
	{
		bool enabled = batch.effectFirstFrame >= 0 && batch.effectFrameCount > 0;
		switch(batch.material)
		{
		case Material::PowerPlant:
		case Material::GravityDrive:
		case Material::Sensor:
		case Material::Shield:
		case Material::FTLDrive:
		case Material::WeaponBay:
		case Material::TransporterPad:
			// AuAtom13:1803c9950 hides the overlay for non-working systems.
			// Offline designs have no fuel/charge, weapon fire or transport.
			// Turning a switch on does not supply either resource.
			enabled = false;
			break;
		case Material::Berth:
		case Material::LifeSupport:
		case Material::Surgery:
			// The original checks life-support allocation, not capacitor charge.
			enabled = enabled && designStats.lifeSupportUnits > 0;
			break;
		default: break;
		}
		program->setUniformValue("u_hasEffect", enabled);
		if (!enabled)
			return;
		if (!effectTexture)
		{
			QByteArray encoded;
			for (const char* part : LegacyShipEffectsPng)
				encoded.append(part);
			const QImage effects = QImage::fromData(QByteArray::fromBase64(encoded), "PNG").mirrored();
			effectTexture = new QOpenGLTexture(effects);
			effectTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
			effectTexture->setMinificationFilter(QOpenGLTexture::Linear);
			effectTexture->setMagnificationFilter(QOpenGLTexture::Linear);
		}
		const qint64 duration = std::max<qint64>(1, qint64(batch.effectFrameSeconds * 1000));
		const int frame = batch.effectFirstFrame + int((elapsed / duration) % batch.effectFrameCount);
		if (frame < 0 || frame >= int(sizeof(EffectTextures) / sizeof(EffectTextures[0])))
		{
			program->setUniformValue("u_hasEffect", false);
			return;
		}
		const TextureRegion& region = EffectTextures[frame];
		program->setUniformValue("u_effectRect", QVector4D((region.x + 0.5f) / 1024,
			(region.y + 0.5f) / 128, (region.width - 1.0f) / 1024, (region.height - 1.0f) / 128));
		effectTexture->bind(1);
	}
};

AuLegacyShipRender::AuLegacyShipRender(QWidget* parent)
	: AuDesignerView(parent), m_Impl(new Impl)
{
	QSurfaceFormat requested = format();
	requested.setDepthBufferSize(24);
	requested.setSamples(4);
	setFormat(requested);
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
	qApp->installEventFilter(this);
	ControlsChanged();
	setAccessibleName(QStringLiteral("Legacy spacecraft walkthrough"));
	setMinimumSize(160, 120);
	m_Impl->animationClock.start();
	m_Impl->InitializeCharacter();
	m_Impl->animationTimer = new QTimer(this);
	m_Impl->animationTimer->setInterval(16);
	connect(m_Impl->animationTimer, &QTimer::timeout, this, [this]() { Tick(); });
	m_Impl->animationTimer->start();
}

AuLegacyShipRender::~AuLegacyShipRender()
{
	qApp->removeEventFilter(this);
	ReleaseMouseCapture();
	m_Impl->sound.SetAudio(nullptr);
	CleanupGL();
	// The base view also owns GL resources for the shared scene interface.
	if(context()) makeCurrent();
	delete m_Impl;
}

void AuLegacyShipRender::ControlsChanged()
{
	AuDesignerView::ControlsChanged();
	m_Impl->StopMovement();
	m_Impl->handKeys.clear();
	m_Impl->controls.clear();
	m_Impl->sceneControls.clear();
	m_Impl->pendingSceneControls.clear();
	m_Impl->systemBindings[1] = LoadSystemBindings(DpConsoleHelm());
	m_Impl->systemBindings[2] = LoadSystemBindings(DpConsoleEngineer(0));
	DaPerson person;
	DiControlInputArray configured;
	person.EnumControlInputs(configured);
	for(const DiControlInput& control : configured)
		switch(control.GetId())
		{
		case DaPerson::CI_MoveForward: case DaPerson::CI_MoveBackward:
		case DaPerson::CI_MoveLeft: case DaPerson::CI_MoveRight:
		case DaPerson::CI_MoveUp: case DaPerson::CI_MoveDown:
		case DaPerson::CI_LookUp: case DaPerson::CI_LookDown:
		case DaPerson::CI_TurnLeft: case DaPerson::CI_TurnRight:
		case DaPerson::CI_MoveForwardContinuously: case DaPerson::CI_Run:
		case DaPerson::CI_BrakeOrJump: case DaPerson::CI_Brake: case DaPerson::CI_Jump:
		case DaPerson::CI_EnterExit: case DaPerson::CI_DoorOpen:
		case DaPerson::CI_Crouch: case DaPerson::CI_ProneStandToggle: case DaPerson::CI_Stand:
			m_Impl->controls.append(control);
			break;
		default: break;
		}
	configured.clear();
	EnumControlInputs(configured);
	for(const DiControlInput& control : configured)
	{
		if(control.GetId() == CI_InputModeToggle) m_Impl->toggleControl = control;
		if(control.GetId() == CI_InputModeHandTemporary) m_Impl->handControl = control;
		switch(control.GetId())
		{
		case CI_TopDownViewToggle: case CI_FirstThirdPersonViewToggle:
		case CI_ViewDistanceDecrease: case CI_ViewDistanceIncrease:
		case CI_ViewAngleDecrease: case CI_ViewAngleIncrease:
			m_Impl->sceneControls.append(control);
			break;
		default: break;
		}
	}
	m_Impl->temporaryHand = m_Impl->HandInput(QApplication::queryKeyboardModifiers());
	UpdateMouseCapture();
}

void AuLegacyShipRender::SetFloorPlan(QWidget* floorPlan)
{
	m_Impl->floorPlan = floorPlan;
	floorPlan->setParent(this);
	SetWalkthrough(false);
}

void AuLegacyShipRender::SetInterfaceVisible(bool visible)
{
	// Cancel a pending sidebar click before its button disappears.
	SceneInterfaceMouseRelease(QPoint(-width(), -height()));
	SceneRenderInfo().m_ShowHUD = visible;
	m_Impl->handClick = false;
	update();
}

void AuLegacyShipRender::SetWalkthrough(bool enabled)
{
	m_Impl->profileLastReport = m_Impl->animationClock.elapsed();
	m_Impl->profileFrames = m_Impl->profileNanoseconds = m_Impl->profileDraws
		= m_Impl->profileVertices = m_Impl->profileSorts = 0;
	if(m_Impl->floorPlan) m_Impl->floorPlan->setVisible(!enabled);
	if(!enabled)
		for(const auto& trail : m_Impl->contrails) trail->RestartParticleSystem();
	m_Impl->StopMovement();
	m_Impl->sound.Stop();
	m_Impl->freeMouse = false;
	UpdateMouseCapture();
	update();
}

void AuLegacyShipRender::SetDesign(const AuLegacyShip::Design& design)
{
	const bool reset = !m_Impl->haveDesign || design.header.designId != m_Impl->design.header.designId;
	const QVector<DoorDefinition> previousDoors = m_Impl->doors;
	const QHash<quint32, DoorAnimation> previousStates = m_Impl->doorStates;
	m_Impl->design = design;
	m_Impl->designStats = AuLegacyShip::CalculateStats(design);
	m_Impl->performance = AuLegacyShip::CalculatePerformance(design, m_Impl->designStats, design.header.minimumTechLevel);
	m_Impl->RebuildContrails();
	if(reset)
	{
		// The preview has its own registration, independent of the saved design.
		// Reuse the native ID spelling used by the original unnamed spacecraft.
		do { m_Impl->shipId = AuId(g_Au.Dice().Random()); }
		while(m_Impl->shipId < AuId::FirstUnreserved());
		m_Impl->shipName = m_Impl->shipId.StringId();
		m_Impl->shipNew = true;
		m_Impl->powerPlantOn = false;
		m_Impl->driveState = 0;
	}
	m_Impl->targetName = QStringLiteral("%1 %2")
		.arg(AuLegacyShip::LegacyStructureName(m_Impl->performance.structure), m_Impl->shipName);
	if(!m_Impl->designStats.powerPlantUnits) m_Impl->powerPlantOn = false;
	if(!m_Impl->designStats.maneuverUnits) m_Impl->driveState = 0;
	m_Impl->sound.SetDesign(design);
	m_Impl->haveDesign = true;
	m_Impl->mesh = AuLegacyShip::Rendering::BuildMesh(design);
	m_Impl->consoleScreens.clear();
	for(int i = 0; i < m_Impl->mesh.batches.size(); ++i)
	{
		const Batch& batch = m_Impl->mesh.batches[i];
		if(batch.consoleId && batch.selfIllumination > 0
			&& batch.textureFile != QStringLiteral(":/data/txConsole_Lower.bmp"))
			m_Impl->consoleScreens.insert(batch.consoleId, i);
	}
	if(m_Impl->character.ladderEngaged)
		m_Impl->world.ToggleLadder(m_Impl->character);
	if(m_Impl->character.consoleType)
		m_Impl->ExitConsole();
	m_Impl->world.SetMesh(m_Impl->mesh, design);
	m_Impl->hasTarget = false;
	m_Impl->targetLevel = 0;
	m_Impl->targetDetail = 0;
	m_Impl->targetHealth = AuLegacyShip::TargetHealth();
	m_Impl->world.SetDesignPlane(float(m_Impl->deck * AuLegacyShip::DeckHeight));
	m_Impl->doors = DoorDefinitions(design, m_Impl->mesh);
	m_Impl->doorStates.clear();
	for (const DoorDefinition& door : m_Impl->doors)
	{
		DoorAnimation state;
		if (!reset)
			for (const DoorDefinition& previous : previousDoors)
				if (previous.id == door.id && previous.type == door.type && previous.deck == door.deck
					&& previous.center == door.center && previous.a == door.a && previous.b == door.b)
				{
					state = previousStates.value(door.id);
					break;
				}
		m_Impl->doorStates.insert(door.id, state);
		m_Impl->world.SetDoorOpen(door.id, state.open);
	}
	if (reset)
	{
		// The original studio creates its standing avatar at the ship origin;
		// switching line/solid mode never resets this character transform.
		m_Impl->character = WalkState();
		m_Impl->person->Stand();
		m_Impl->azimuth = 90;
		m_Impl->elevation = 0;
		m_Impl->StopMovement();
		m_Impl->interaction.clear();
	}
	m_Impl->radius = std::max(1.0f, (m_Impl->mesh.maximum - m_Impl->mesh.minimum).length() * 0.5f);
	m_Impl->dirty = true;
	if (m_Impl->ready)
		m_Impl->glError.clear();
	m_Impl->lastTick = m_Impl->animationClock.elapsed();
	m_Impl->lastDoorTick = m_Impl->lastTick / DoorTickMilliseconds;
	m_Impl->person->Stand();
	m_Impl->PlaceCharacter();
	update();
}

void AuLegacyShipRender::SetDeck(int deck)
{
	m_Impl->deck = deck;
	m_Impl->world.SetDesignPlane(float(deck * AuLegacyShip::DeckHeight));
	m_Impl->person->InvalidateEnvironment();
	update();
}

void AuLegacyShipRender::SetGravity(bool enabled)
{
	m_Impl->gravity = enabled;
	m_Impl->person->InvalidateEnvironment();
	update();
}

void AuLegacyShipRender::ResetCharacter()
{
	m_Impl->StopMovement();
	const AuPose previousPose = m_Impl->person->CurrentGeneralPose();
	const WalkSettings previous = m_Impl->world.Settings();
	m_Impl->person->Stand();
	WalkSettings standing = previous;
	standing.height = m_Impl->person->PhysicalHeight();
	standing.eyeHeight = m_Impl->person->CGHeight() + float(m_Impl->person->CameraOffset().z);
	m_Impl->world.SetSettings(standing);
	WalkState recovered;
	if(m_Impl->world.FindSpawn(m_Impl->design, m_Impl->deck, recovered))
	{
		m_Impl->character = recovered;
		m_Impl->azimuth = 90;
		m_Impl->elevation = 0;
		m_Impl->PlaceCharacter();
		m_Impl->interaction = QStringLiteral("Returned to the design.");
	}
	else
	{
		m_Impl->person->SetGeneralPose(previousPose);
		m_Impl->world.SetSettings(previous);
		m_Impl->interaction = QStringLiteral("No clear standing position on this deck. Select another deck and press Home.");
	}
	m_Impl->interactionUntil = m_Impl->animationClock.elapsed() + 2500;
	update();
}

void AuLegacyShipRender::Tick()
{
	const qint64 now = m_Impl->animationClock.elapsed();
	const float seconds = std::min(0.05f, float(now - m_Impl->lastTick) / 1000.0f);
	m_Impl->lastTick = now;
	if (!isVisible()) return;
	if(!m_Impl->Walkthrough())
	{
		const int sidebarWidth = SceneRenderInfo().m_ShowHUD ? SceneInterfaceWidth() : 0;
		m_Impl->floorPlan->setGeometry(sidebarWidth, 0, std::max(0, width() - sidebarWidth), height());
		UpdateMouseCapture();
		update();
		return;
	}
	AuApp* app = qobject_cast<AuApp*>(qApp);
	m_Impl->sound.SetAudio(app ? app->Sound() : nullptr);
	m_Impl->InitializeCharacter();
	const Qt::KeyboardModifiers modifiers = QApplication::queryKeyboardModifiers();
	m_Impl->temporaryHand = m_Impl->HandInput(modifiers);
	AuDesignerFrame* frame = app ? qobject_cast<AuDesignerFrame*>(app->Frame()) : nullptr;
	AuJoystick* joystick = frame ? frame->Joystick() : nullptr;
	const bool sampleJoystick = hasFocus() && isActiveWindow() && !QApplication::activeModalWidget()
		&& !QApplication::activePopupWidget() && joystick;
	if(sampleJoystick)
	{
		joystick->Poll();
		m_Impl->temporaryHand |= m_Impl->handControl.TestJoystickTriggered(joystick);
		if(m_Impl->toggleControl.TestJoystickTriggered(joystick))
			m_Impl->freeMouse = !m_Impl->freeMouse;
	}
	UpdateMouseCapture();
	DiControlInputArray active;
	const Qt::KeyboardModifiers keyModifiers = m_Impl->controls.FilterModState(m_Impl->keys, modifiers);
	m_Impl->controls.SampleKeyInput(active, m_Impl->keys, keyModifiers, AuId(0), 0, false, AuPoint(0, 0), 0);
	DiControlInputArray sceneActive;
	m_Impl->sceneControls.SampleKeyInput(sceneActive, m_Impl->keys,
		m_Impl->sceneControls.FilterModState(m_Impl->keys, modifiers), AuId(0), 0, false, AuPoint(0, 0), 0);
	sceneActive += m_Impl->pendingSceneControls;
	m_Impl->pendingSceneControls.clear();
	if(sampleJoystick)
		m_Impl->sceneControls.SampleJoystickInput(sceneActive, joystick, AuId(0), 0, false, AuPoint(0, 0), 0);
	ProcessControlInput(sceneActive);
	m_Impl->sceneControls.ClearRanges();
	SystemBindings& systems = m_Impl->systemBindings[m_Impl->SystemBindingIndex()];
	DiControlInputArray systemActive;
	if(m_Impl->mouseCaptured)
	{
		systems.controls.SampleKeyInput(systemActive, m_Impl->keys,
			systems.controls.FilterModState(m_Impl->keys, modifiers), AuId(0), 0, false, AuPoint(0, 0), 0);
		if(sampleJoystick)
			systems.controls.SampleJoystickInput(systemActive, joystick, AuId(0), 0, false, AuPoint(0, 0), 0);
	}
	active += m_Impl->pendingControls;
	m_Impl->pendingControls.clear();
	for(auto& key : m_Impl->keys)
		if(!key.second.isValid()) key.second.start();
	for(int key : m_Impl->releasedKeys) m_Impl->keys.erase(Qt::Key(key));
	m_Impl->releasedKeys.clear();
	if(m_Impl->mouseCaptured && sampleJoystick)
		m_Impl->controls.SampleJoystickInput(active, joystick, AuId(0), 0, false, AuPoint(0, 0), 0);
	m_Impl->controls.ClearRanges();
	if (m_Impl->mouseCaptured)
	{
		// Sample the real cursor, rather than queued synthetic warp events.
		// The original input loop also recentres it while controlling the avatar.
		const QPoint delta = QCursor::pos() - m_Impl->mouseAnchor;
		const float unitsPerPixel = float(g_AuClient.MouseLookSensitivity()) * 300.f / std::max(1, width());
		DiControlInputArray mouse;
		DiControlInputArray sceneMouse;
		const bool sceneStreaming = m_Impl->sceneControls.SampleMouseInput(sceneMouse, m_Impl->mouseButtons | m_Impl->pendingMouseButtons,
			modifiers, QPoint(), AuId(0), 0, false, AuPoint(0, 0), 0);
		m_Impl->sceneControls.SampleMouseInput(sceneMouse, Qt::NoButton, modifiers,
			QPoint(qRound(delta.x() * unitsPerPixel), qRound(delta.y() * unitsPerPixel)),
			AuId(0), 0, false, AuPoint(0, 0), 0);
		ProcessControlInput(sceneMouse);
		m_Impl->sceneControls.ClearRanges();
		const bool streaming = m_Impl->controls.SampleMouseInput(mouse, m_Impl->mouseButtons | m_Impl->pendingMouseButtons, keyModifiers,
			QPoint(), AuId(0), 0, false, AuPoint(0, 0), 0);
		const bool systemStreaming = systems.controls.SampleMouseInput(systemActive,
			m_Impl->mouseButtons | m_Impl->pendingMouseButtons, modifiers,
			QPoint(), AuId(0), 0, false, AuPoint(0, 0), 0);
		if(!streaming && !sceneStreaming && !systemStreaming) m_Impl->mouseButtons = Qt::NoButton;
		m_Impl->pendingMouseButtons = Qt::NoButton;
		m_Impl->controls.SampleMouseInput(mouse, Qt::NoButton, keyModifiers,
			QPoint(qRound(delta.x() * unitsPerPixel), qRound(delta.y() * unitsPerPixel)),
			AuId(0), 0, false, AuPoint(0, 0), 0);
		active += mouse;
		CenterMouse();
	}
	m_Impl->controls.ClearRanges();
	for(const DiControlInput& control : systemActive)
		OperateSystem(systems.actions.value(control.GetId(), -1));
	systems.controls.ClearRanges();
	if(m_Impl->mouseCaptured && active.HasId(DaPerson::CI_EnterExit))
	{
		bool changed = false;
		if(m_Impl->character.consoleType)
		{
			m_Impl->ExitConsole();
			m_Impl->interaction = QStringLiteral("Left console.");
		}
		else if(!m_Impl->character.ladderEngaged && m_Impl->world.EnterConsole(m_Impl->design,
			m_Impl->character, m_Impl->person->CGHeight()))
		{
			m_Impl->interaction = QStringLiteral("Seated at console.");
			changed = true;
		}
		else if(m_Impl->world.ToggleLadder(m_Impl->character))
		{
			m_Impl->interaction = m_Impl->character.ladderEngaged
				? QStringLiteral("On ladder.") : QStringLiteral("Released ladder.");
			if(m_Impl->character.ladderEngaged)
			{
				m_Impl->azimuth = m_Impl->character.ladderFacing;
				m_Impl->elevation = 0;
			}
			changed = true;
		}
		else m_Impl->interaction = QStringLiteral("Move within 3 feet of a console or 6 feet of a ladder.");
		if(changed) m_Impl->SetInteractionPose();
		m_Impl->interactionUntil = now + 2500;
	}
	if(m_Impl->mouseCaptured && m_Impl->character.ladderEngaged
		&& (active.HasId(DaPerson::CI_Jump) || active.HasId(DaPerson::CI_BrakeOrJump)))
	{
		m_Impl->world.ToggleLadder(m_Impl->character);
		m_Impl->SetInteractionPose();
	}
	if(hasFocus() && active.HasId(DaPerson::CI_DoorOpen))
	{
		const quint32 id = ClosestDoor(m_Impl->doors, m_Impl->doorStates, m_Impl->character.feet, false);
		if(id) OperateDoor(id, true);
	}
	const qint64 tick = now / DoorTickMilliseconds;
	for (qint64 step = m_Impl->lastDoorTick; step < tick && step < m_Impl->lastDoorTick + 2; ++step)
		for (auto door = m_Impl->doorStates.begin(); door != m_Impl->doorStates.end(); ++door)
			if (AdvanceDoorAnimation(door.value()) && door.value().frame == DoorFrame::HalfOpen)
				for (const DoorDefinition& definition : m_Impl->doors)
					if (definition.id == door.key())
					{
						m_Impl->sound.DoorTransition(definition, door.value().open);
						break;
					}
	m_Impl->lastDoorTick = tick;
	if(hasFocus() && seconds > 0)
	{
		DiControlInputArray nativeControls;
		if(m_Impl->mouseCaptured)
			for(const DiControlInput& control : active)
			{
				const int id = control.GetId();
				if(id == DaPerson::CI_EnterExit || id == DaPerson::CI_DoorOpen) continue;
				if(m_Impl->character.consoleType && id != DaPerson::CI_LookUp && id != DaPerson::CI_LookDown
					&& id != DaPerson::CI_TurnLeft && id != DaPerson::CI_TurnRight) continue;
				if(m_Impl->character.ladderEngaged && (id == DaPerson::CI_Crouch || id == DaPerson::CI_ProneStandToggle
					|| id == DaPerson::CI_Stand)) continue;
				// Space streams while held. In gravity, a rejected repeat jump
				// must not turn into the designer's 10000 ft/s^2 brake and
				// erase takeoff velocity. Keep native braking for flight/zero-G.
				if(id == DaPerson::CI_BrakeOrJump && m_Impl->person->IsInGravity()
					&& !m_Impl->person->IsFlying())
				{
					nativeControls.append(DiControlInput(-1, DaPerson::CI_Jump));
					continue;
				}
				// Keep forward/backward input intact: native Acceleration tilts it
				// with the head, then FilterFinalMoveVector keeps only Z on ladders.
				// The 2016 controller used this same path (AuAtom13:18031be50).
				nativeControls.append(control);
			}
		// The native Scene input loop requests standing when held crouch ends.
		if(m_Impl->person->IsCrouched() && !nativeControls.HasId(DaPerson::CI_Crouch))
			nativeControls.append(DiControlInput(-1, DaPerson::CI_Stand));
		m_Impl->character.blockedDoors.clear();
		// Match AuModel's offline designer upkeep. Jump consumes stamina in
		// the native controller; this transient person has no AuModel loop.
		m_Impl->person->ResetHunger();
		m_Impl->person->ResetStamina();
		m_Impl->person->InvalidateEnvironment();
		m_Impl->person->ProcessControlInput(nativeControls, m_Impl->person, AuGearPos(), 0, CONTROL_Client,
			int(std::min<qint64>(150, m_Impl->personClock.elapsed())));
		const bool onLadder = m_Impl->character.ladderEngaged;
		// Keep the start of this physics step, as AuModel does. Restarting
		// after movement/animation discards their cost from simulated time.
		const AuElapsedTimer movementStart = AuElapsedTimer::Now();
		m_Impl->person->TickMovement(m_Impl->personClock);
		m_Impl->personClock = movementStart;
		m_Impl->person->AnimatePose(false, m_Impl->person->MovementSpeedMod());
		m_Impl->person->InvalidateEnvironment();
		m_Impl->SyncCharacter();
		if(onLadder && !m_Impl->character.ladderEngaged) m_Impl->SetInteractionPose();
		// The 2016 path query requests opening only for automatic small doors
		// and hatches. Designer Tock deliberately skips world auto-closing.
		for (quint32 id : m_Impl->character.blockedDoors)
			for (const DoorDefinition& door : m_Impl->doors)
				if (door.id == id && IsAutomaticDoorDetail(door.type))
				{
					OperateDoor(id, true);
					break;
				}
	}
	else m_Impl->SyncCharacter();
	UpdateTarget(now);
	if (isActiveWindow())
	{
		SoundListener listener;
		listener.feet = m_Impl->character.feet;
		const auto camera = m_Impl->Camera(SceneRenderInfo());
		listener.eye = camera.eye;
		listener.forward = camera.forward;
		listener.up = camera.up;
		listener.inShip = m_Impl->character.inShip;
		listener.grounded = m_Impl->character.grounded;
		listener.climbing = m_Impl->character.onLadder;
		listener.running = m_Impl->running;
		listener.crouching = m_Impl->person->IsCrouched();
		listener.prone = m_Impl->person->IsProne();
		const QVector3D velocity = m_Impl->character.velocity;
		listener.moving = hasFocus() && !m_Impl->character.consoleType && (m_Impl->person->IsMoving()
			|| (listener.climbing && std::abs(velocity.z()) > .01f));
		m_Impl->sound.Update(listener);
	}
	else
		m_Impl->sound.Stop();
	update();
}

void AuLegacyShipRender::UpdateTarget(qint64 now)
{
	if (now - m_Impl->lastTargetTick < 200) return;
	m_Impl->lastTargetTick = now;
	WalkHit hit;
	if (m_Impl->world.Raycast(m_Impl->Eye(), m_Impl->CameraDirection(), 256, hit, false))
	{
		// EXE:14032f800 distinguishes changing detail from retaining the same
		// detail, even when both belong to the same ship or subsystem.
		m_Impl->targetLevel = m_Impl->hasTarget
			? (m_Impl->targetDetail != hit.detailId ? std::min(15, m_Impl->targetLevel + 1)
				: m_Impl->targetLevel ? std::min(15, m_Impl->targetLevel * 2) : 1)
			: m_Impl->targetLevel / 3;
		m_Impl->hasTarget = true;
		m_Impl->targetDetail = hit.detailId;
		m_Impl->targetHealth = AuLegacyShip::DesignTargetHealth(m_Impl->design, m_Impl->designStats,
			m_Impl->performance, hit.detailId);
		m_Impl->targetPoint = hit.point;
	}
	else
	{
		m_Impl->targetLevel /= 2;
		if (!m_Impl->targetLevel)
		{
			m_Impl->hasTarget = false;
			m_Impl->targetDetail = 0;
			m_Impl->targetHealth = AuLegacyShip::TargetHealth();
		}
	}
}

void AuLegacyShipRender::OperateSystem(int action)
{
	if(!m_Impl->SystemBindingIndex()) return;
	if(action == PowerToggle)
	{
		if(!m_Impl->designStats.powerPlantUnits) return;
		m_Impl->powerPlantOn = !m_Impl->powerPlantOn;
		m_Impl->interaction = m_Impl->powerPlantOn ? tr("Power plant on.") : tr("Power plant off.");
	}
	else
	{
		if(!m_Impl->designStats.maneuverUnits || (m_Impl->shipNew && !m_Impl->powerPlantOn)) return;
		const bool station = m_Impl->performance.structure == 3 || m_Impl->performance.structure == 4;
		int state = m_Impl->driveState;
		switch(action)
		{
		case DriveToggle: state = state ? 0 : 1; break;
		case DriveCycle: state = !state ? 1 : state == 1 && !station ? 2 : 0; break;
		case DriveHold:
			if(m_Impl->shipNew || !state || (state == 1 && station)) return;
			state = state == 1 ? 2 : 1;
			break;
		default: return;
		}
		if(state == m_Impl->driveState) return;
		m_Impl->driveState = state;
		// The original SetGravityDriveState calls SetNotNew once, regardless
		// of fuel or capacitor charge. Use the surviving announcement text
		// and native local message path; no world/sector or network is needed.
		if(m_Impl->shipNew)
		{
			m_Impl->shipNew = false;
			AuCommChat message(DaSc::SystemInitializationMessage(), AuCommStdChannel::IntercomChannelName(),
				m_Impl->shipId, m_Impl->shipName, AuId(0), ShoresOfHazeron, AuCommStdChannel::Close, AuCommStdChannel::Intercom);
			ChatReceived(&message);
		}
		m_Impl->interaction = !state ? tr("Maneuver drive off.")
			: state == 1 ? tr("Maneuver drive on.") : tr("Maneuver drive holding.");
	}
	m_Impl->interactionUntil = m_Impl->animationClock.elapsed() + 2500;
	update();
}

void AuLegacyShipRender::OperateDoor(quint32 id, bool open)
{
	if (!id || !m_Impl->doorStates.contains(id)) return;
	const QVector<quint32> group = DoorOperationGroup(m_Impl->doors, id);
	for (quint32 member : group)
	{
		m_Impl->doorStates[member].open = open;
		m_Impl->world.SetDoorOpen(member, open);
	}
	m_Impl->person->InvalidateEnvironment();
	m_Impl->interaction = open ? QStringLiteral("Opening door / hatch.") : QStringLiteral("Closing door / hatch.");
	m_Impl->interactionUntil = m_Impl->animationClock.elapsed() + 1800;
	update();
}

bool AuLegacyShipRender::UseConsole(const QPoint& screenPosition)
{
	if(!m_Impl->ready || !m_Impl->person || !m_Impl->SystemBindingIndex()) return false;
	const auto camera = m_Impl->Camera(SceneRenderInfo());
	const QVector3D right = QVector3D::crossProduct(camera.forward, camera.up).normalized();
	const QVector3D up = QVector3D::crossProduct(right, camera.forward).normalized();
	const float tangent = std::tan(FieldOfView() * Pi / 360.0f);
	const float aspect = float(std::max(1, width())) / std::max(1, height());
	const float x = 2.0f * screenPosition.x() / std::max(1, width()) - 1;
	const float y = 1 - 2.0f * screenPosition.y() / std::max(1, height());
	const QVector3D direction = (camera.forward + right * (x * tangent * aspect) + up * (y * tangent)).normalized();
	const float reach = (camera.eye - m_Impl->Eye()).length() + 16;
	const AuPoint start = Point3(camera.eye), end = Point3(camera.eye + direction * reach);
	const QMatrix4x4 transform = m_Impl->ConsoleTransform();
	AuMatrix4x4 model;
	for(int i = 0; i < 16; ++i) model[i] = transform.constData()[i];
	AuQuadArray panels;
	QVector<qint16> ids;
	// The same native panel geometry/adjustments are used to draw the screen.
	m_Impl->console.EnumControlPanels(panels, model, ids);
	int nearest = -1;
	double distance = reach;
	AuPoint panelHit;
	for(int i = 0; i < panels.size(); ++i)
	{
		AuPoint hit, normal;
		if(panels[i].TestSegmentIntersection(hit, normal, start, end) && start.Distance(hit) < distance)
		{
			nearest = i;
			distance = start.Distance(hit);
			panelHit = hit;
		}
	}
	if(nearest < 0) return false;
	WalkHit obstruction;
	if(m_Impl->world.Raycast(camera.eye, direction, float(distance), obstruction)
		&& obstruction.detailId != m_Impl->character.consoleId) return false;
	if(ids[nearest] != 0) return true;
	const AuQuad& panel = panels[nearest];
	AuPoint rightAxis = panel.vlr - panel.vll, upAxis = panel.vul - panel.vll;
	rightAxis.Unit();
	upAxis.Unit();
	const AuPoint offset = panelHit - panel.vll;
	const AuPoint panelLocation(offset.Dot(rightAxis), offset.Dot(upAxis), 0);
	DiControlInputArray active;
	if(m_Impl->character.consoleType == 12)
	{
		DpConsoleHelm station;
		station.SampleControlPanelInput(0, panelLocation, active, false);
	}
	else
	{
		DpConsoleEngineer station(0);
		station.SampleControlPanelInput(0, panelLocation, active, false);
	}
	const SystemBindings& systems = m_Impl->systemBindings[m_Impl->SystemBindingIndex()];
	for(const DiControlInput& control : active)
		OperateSystem(systems.actions.value(control.GetId(), -1));
	return true;
}

void AuLegacyShipRender::UseDoor(const QPoint& screenPosition)
{
	const auto camera = m_Impl->Camera(SceneRenderInfo());
	const QVector3D forward = camera.forward;
	const QVector3D right = QVector3D::crossProduct(forward, camera.up).normalized();
	const QVector3D up = camera.up;
	const float aspect = float(std::max(1, width())) / std::max(1, height());
	const float tangent = std::tan(FieldOfView() * Pi / 360.0f);
	const float x = 2.0f * screenPosition.x() / std::max(1, width()) - 1;
	const float y = 1 - 2.0f * screenPosition.y() / std::max(1, height());
	const QVector3D direction = (forward + right * (x * tangent * aspect) + up * (y * tangent)).normalized();
	WalkHit hit;
	if (m_Impl->world.Raycast(camera.eye, direction, (camera.eye - m_Impl->Eye()).length() + 16, hit) && hit.doorId)
		for (const DoorDefinition& door : m_Impl->doors)
			if (door.id == hit.doorId)
			{
				const int deck = int(std::floor((m_Impl->character.feet.z() + .05) / AuLegacyShip::DeckHeight));
				if ((door.deck == deck || door.deck == deck + 1)
					&& DoorDistanceSquared(door, m_Impl->character.feet.toPointF()) <= DoorInteractionDistance * DoorInteractionDistance)
				{
					OperateDoor(door.id, !m_Impl->doorStates.value(door.id).open);
					return;
				}
			}
	m_Impl->interaction = QStringLiteral("Move within 6 feet of a door or hatch, then click it.");
	m_Impl->interactionUntil = m_Impl->animationClock.elapsed() + 2500;
	update();
}

QStringList AuLegacyShipRender::Warnings() const
{
	QStringList warnings = m_Impl->mesh.warnings;
	warnings.append(m_Impl->textureWarnings);
	if (!m_Impl->glError.isEmpty())
		warnings.append(m_Impl->glError);
	if (!m_Impl->skyError.isEmpty())
		warnings.append(m_Impl->skyError);
	return warnings;
}

void AuLegacyShipRender::initializeGL()
{
	InitializeSceneInterface(true);
	QObject::disconnect(m_Impl->contextConnection);
	m_Impl->contextConnection = connect(context(), &QOpenGLContext::aboutToBeDestroyed,
		this, [this]() { CleanupGL(); }, Qt::DirectConnection);
	m_Impl->glError.clear();
	m_Impl->textureWarnings.clear();
	m_Impl->program = new QOpenGLShaderProgram;
	const bool es = context()->isOpenGLES();
	const QByteArray version = es ? QByteArray("#version 100\nprecision highp float;\n")
		: QByteArray("#version 150\n");
	const QByteArray vertex = version + (es
		? "#define IN attribute\n#define OUT varying\n"
		: "#define IN in\n#define OUT out\n") + R"(
IN vec3 a_position;
IN vec3 a_normal;
IN vec2 a_uv;
uniform mat4 u_mvp;
uniform mat4 u_model;
uniform vec2 u_uvOffset;
struct StudioSun
{
	vec3 position;
	vec3 color;
	float attenuation;
	float radius;
};
uniform StudioSun u_suns[3];
uniform vec3 u_ambient;
uniform bool u_twoSided;
OUT vec3 v_light;
OUT vec3 v_backLight;
OUT vec2 v_uv;
vec3 lighting(vec3 position, vec3 normal)
{
	// Original Spacecraft.v.glsl: spherical ambient plus three studio suns.
	vec3 color = u_ambient * mix(0.15, 1.0, (normal.z + 1.0) * 0.5);
	for (int i = 0; i < 3; ++i)
	{
		vec3 toSun = u_suns[i].position - position;
		float distanceSquared = max(dot(toSun, toSun), 0.0001);
		float distanceToSun = sqrt(distanceSquared);
		float cosine = clamp(dot(normal, toSun / distanceToSun), -1.0, 1.0);
		// cos(acos(c) +/- atan(r/d)), without inverse trig for every vertex.
		float radius = u_suns[i].radius;
		float diskCosine = distanceToSun / sqrt(distanceSquared + radius * radius);
		float diskSine = radius / sqrt(distanceSquared + radius * radius);
		float diffuse = max(0.0, max(cosine,
			cosine * diskCosine + sqrt(max(0.0, 1.0 - cosine * cosine)) * diskSine));
		color += u_suns[i].color * diffuse / max(u_suns[i].attenuation * distanceSquared, 0.0001);
	}
	return clamp(color, 0.0, 1.0);
}
void main()
{
	vec3 position = (u_model * vec4(a_position, 1.0)).xyz;
	vec3 normal = normalize(mat3(u_model) * a_normal);
	v_light = lighting(position, normal);
	v_backLight = u_twoSided ? lighting(position, -normal) : v_light;
	v_uv = a_uv + u_uvOffset;
	gl_Position = u_mvp * vec4(position, 1.0);
}
)";
	const QByteArray fragment = version + (es
		? "#define IN varying\n#define SAMPLE texture2D\n#define COLOR gl_FragColor\n"
		: "#define IN in\n#define SAMPLE texture\nout vec4 fragColor;\n#define COLOR fragColor\n") + R"(
IN vec3 v_light;
IN vec3 v_backLight;
IN vec2 v_uv;
uniform sampler2D u_texture;
uniform sampler2D u_effect;
uniform bool u_hasEffect;
uniform vec4 u_effectRect;
uniform vec4 u_tint;
uniform float u_selfIllumination;
uniform bool u_atlas;
uniform bool u_twoSided;
uniform vec2 u_inset;
uniform bool u_translucentPass;
uniform int u_studioSide;
void main()
{
	vec2 uv = u_atlas ? mix(u_inset, vec2(1.0) - u_inset, fract(v_uv)) : v_uv;
	vec4 texel = SAMPLE(u_texture, uv) * u_tint;
	if (u_studioSide != 0)
	{
		// Original procedural surfaces use alpha testing, not blending:
		// exterior cutoff 0, interior cutoff 0.1, with opaque depth writes.
		if (texel.a <= (u_studioSide == 2 ? 0.1 : 0.0))
			discard;
		texel.a = 1.0;
	}
	if (texel.a < 0.01 || u_translucentPass != (texel.a < 0.999))
		discard;
	vec3 light = u_twoSided && !gl_FrontFacing ? v_backLight : v_light;
	vec3 color = texel.rgb * mix(light, vec3(1.0), u_selfIllumination);
	if (u_hasEffect)
	{
		vec4 effect = SAMPLE(u_effect, u_effectRect.xy + fract(v_uv) * u_effectRect.zw);
		color = mix(color, effect.rgb, effect.a);
	}
	COLOR = vec4(color, texel.a);
}
)";
	if (!m_Impl->program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex)
		|| !m_Impl->program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment)
		|| !m_Impl->program->link())
	{
		m_Impl->glError = QStringLiteral("Unable to initialize 3D rendering: %1").arg(m_Impl->program->log());
		return;
	}
	if (!m_Impl->buffer.create() || !m_Impl->transparentBuffer.create() || !m_Impl->vao.create())
	{
		m_Impl->glError = QStringLiteral("Unable to create OpenGL spacecraft rendering resources.");
		return;
	}
	m_Impl->buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
	m_Impl->transparentBuffer.setUsagePattern(QOpenGLBuffer::StreamDraw);
	m_Impl->vao.bind();
	m_Impl->buffer.bind();
	m_Impl->program->bind();
	const char* names[] = { "a_position", "a_normal", "a_uv" };
	const int offsets[] = { int(offsetof(RenderVertex, position)), int(offsetof(RenderVertex, normal)),
		int(offsetof(RenderVertex, uv)) };
	for (int i = 0; i < 3; ++i)
	{
		const int location = m_Impl->program->attributeLocation(names[i]);
		m_Impl->program->enableAttributeArray(location);
		m_Impl->program->setAttributeBuffer(location, GL_FLOAT, offsets[i], i == 2 ? 2 : 3,
			int(sizeof(RenderVertex)));
	}
	m_Impl->vao.release();
	m_Impl->buffer.release();
	m_Impl->program->release();
	m_Impl->ready = true;
	m_Impl->dirty = true;
}

void AuLegacyShipRender::CleanupGL()
{
	QObject::disconnect(m_Impl->contextConnection);
	if (context())
		makeCurrent();
	m_Impl->console.DeleteGL();
	for(const QString& id : {QStringLiteral("LegacyEngineStreak"), QStringLiteral("LegacyEngineGlow")})
	{
		AuGLEffects& re = *SceneRenderInfo().re;
		if(auto* texture = re.FindTexture(id))
		{
			const GLuint name = texture->GlName();
			re.glDeleteTextures(1, &name);
			re.DeleteTexture(STATIC_TEXTURE_ID, id);
		}
	}
	m_Impl->contrailStreak = m_Impl->contrailGlow = 0;
	for(const auto& trail : m_Impl->contrails) trail->RestartParticleSystem();
	if(m_Impl->ready) DpConsole::ReleaseSharedDeviceObjects(SceneRenderInfo());
	if(m_Impl->skyJob)
	{
		m_Impl->skyJob->Cancel(*SceneRenderInfo().re);
		m_Impl->skyJob.reset();
	}
	if(m_Impl->sky)
	{
		m_Impl->sky->DeleteDeviceObjects(*SceneRenderInfo().re);
		m_Impl->sky.reset();
		m_Impl->skyError.clear();
	}
	qDeleteAll(m_Impl->textures);
	m_Impl->textures.clear();
	m_Impl->batchTextures.clear();
	m_Impl->transparentTextures.clear();
	delete m_Impl->effectTexture;
	m_Impl->effectTexture = nullptr;
	m_Impl->vao.destroy();
	m_Impl->buffer.destroy();
	m_Impl->transparentBuffer.destroy();
	delete m_Impl->program;
	m_Impl->program = nullptr;
	m_Impl->ready = false;
	m_Impl->dirty = true;
	if (context())
		doneCurrent();
}

float AuLegacyShipRender::FieldOfView() const
{
	return float(AuGLEffects::WindowFieldOfViewY(rect(), g_Au.FieldOfViewY()) * 180.0 / Pi);
}

void AuLegacyShipRender::PrepareSky()
{
	if(!g_Au.DrawSkyBox() || !g_AuClient.DesignerShowSkyBox()) return;
	AuGLEffects& re = *SceneRenderInfo().re;
	const unsigned int paneSize = g_Au.SkyBoxPaneSize();
	const bool mipMap = g_Au.SkyBoxMipMap();
	if(m_Impl->sky && (m_Impl->skyPaneSize != paneSize || m_Impl->skyMipMap != mipMap))
	{
		if(m_Impl->skyJob) m_Impl->skyJob->Cancel(re);
		m_Impl->skyJob.reset();
		m_Impl->sky->DeleteDeviceObjects(re);
		m_Impl->sky.reset();
		m_Impl->skyError.clear();
	}
	m_Impl->skyPaneSize = paneSize;
	m_Impl->skyMipMap = mipMap;
	if(!m_Impl->sky)
	{
		m_Impl->sky.reset(new auNebula(0, 0, -2.9924924f, .38151205f, -1.8401746f, -1.8524430f,
			30, 0, false, AuPoint(), 2016, 0, QColor()));
		// A complete, tiny navy cube is immediately usable by the normal sky shader.
		const GLubyte navy[] = {0, 0, 128, 255};
		re.glActiveTexture(GL_TEXTURE0);
		re.glGenTextures(1, &m_Impl->sky->m_img);
		re.glBindTexture(GL_TEXTURE_CUBE_MAP, m_Impl->sky->m_img);
		for(int face = 0; face < 6; ++face)
			re.glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8,
				1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, navy);
		re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// Qt requires offscreen surface creation/destruction on the GUI thread.
		// Establish sharing before the widget's context can be destroyed, then
		// transfer the worker context; all generation runs on SkyThread.
		auto* workerContext = new QOpenGLContext;
		workerContext->setFormat(context()->format());
		workerContext->setShareContext(context());
		if(!QOpenGLContext::supportsThreadedOpenGL() || !workerContext->create()
			|| !QOpenGLContext::areSharing(context(), workerContext))
		{
			delete workerContext;
			m_Impl->skyError = QStringLiteral("Unable to create a shared preview skybox context.");
			return;
		}
		auto* surface = new QOffscreenSurface;
		surface->setFormat(workerContext->format());
		surface->create();
		if(!surface->isValid())
		{
			delete surface;
			delete workerContext;
			m_Impl->skyError = QStringLiteral("Unable to create the preview skybox surface.");
			return;
		}
		m_Impl->skyJob = std::make_shared<SkyJob>();
		auto* worker = new SkyThread(m_Impl->skyJob, workerContext, surface, paneSize, mipMap);
		worker->start(QThread::LowPriority);
	}
	if(m_Impl->skyJob)
	{
		// Never wait for publication, even briefly, in a paint callback.
		std::unique_lock<std::mutex> lock(m_Impl->skyJob->mutex, std::try_to_lock);
		if(!lock.owns_lock() || !m_Impl->skyJob->done) return;
		if(m_Impl->skyJob->texture)
		{
			re.glDeleteTextures(1, &m_Impl->sky->m_img);
			m_Impl->sky->m_img = m_Impl->skyJob->texture;
			m_Impl->skyJob->texture = 0;
		}
		m_Impl->skyError = m_Impl->skyJob->error;
		lock.unlock();
		m_Impl->skyJob.reset();
	}
}

void AuLegacyShipRender::RenderSky()
{
	if(!g_Au.DrawSkyBox() || !g_AuClient.DesignerShowSkyBox()
		|| !m_Impl->sky || !m_Impl->sky->m_img) return;
	AuGLEffects& re = *SceneRenderInfo().re;
	QMatrix4x4 rotation;
	const auto camera = m_Impl->Camera(SceneRenderInfo());
	rotation.lookAt(QVector3D(), camera.forward, camera.up);
	AuMatrix4x4 model, projection;
	for(int i = 0; i < 16; ++i) model[i] = rotation.constData()[i];
	projection.Perspective(FieldOfView() * Pi / 180., double(std::max(1, height())) / std::max(1, width()),
		1, FeetPerParsec() * 2);
	re.glDepthMask(GL_FALSE);
	re.glDisable(GL_DEPTH_TEST);
	re.glDisable(GL_CULL_FACE);
	re.glDisable(GL_SCISSOR_TEST);
	re.glDisable(GL_BLEND);
	m_Impl->sky->RenderSkyBox(re, projection, model, 1);
	re.glDepthMask(GL_TRUE);
}

void AuLegacyShipRender::resizeGL(int, int)
{
	if (m_Impl->mouseCaptured) CenterMouse();
}

void AuLegacyShipRender::resizeEvent(QResizeEvent* event)
{
	// The legacy geometry is animated by Tick, without the world targeting thread.
	QOpenGLWidget::resizeEvent(event);
}

bool AuLegacyShipRender::event(QEvent* event)
{
	if(event->type() == QEvent::ShortcutOverride && m_Impl->Walkthrough())
	{
		const auto* key = static_cast<QKeyEvent*>(event);
		// Escape belongs to the viewer's Back/visibility actions in both modes.
		if(key->key() == Qt::Key_Escape
			&& (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::ShiftModifier))
			return QOpenGLWidget::event(event);
		if(m_Impl->HasKeyBinding(key))
		{
			event->accept();
			return true;
		}
	}
	if(event->type() == QEvent::KeyPress && m_Impl->Walkthrough())
	{
		QKeyEvent* key = static_cast<QKeyEvent*>(event);
		if(key->key() == Qt::Key_Tab && m_Impl->HasKeyBinding(key))
		{
			keyPressEvent(key);
			return key->isAccepted();
		}
	}
	return QOpenGLWidget::event(event);
}

void AuLegacyShipRender::paintGL()
{
	QElapsedTimer frameClock;
	if(m_Impl->profileRender) frameClock.start();
	quint64 frameDraws = 0, frameVertices = 0;
	QOpenGLFunctions* gl = context()->functions();
	gl->glClearColor(0, 0, 0, 1);
	gl->glDepthMask(GL_TRUE);
	gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if(m_Impl->ready) PrepareSky();
	if(m_Impl->ready)
	{
		m_Impl->console.CreateDeviceObjects(SceneRenderInfo(), AuId(STATIC_TEXTURE_ID), false);
		m_Impl->InitializeCharacter();
		m_Impl->SyncCharacter();
	}
	if(!m_Impl->Walkthrough())
	{
		RenderSceneInterface(false);
		return;
	}
	if (m_Impl->ready && (!m_Impl->dirty || m_Impl->Upload()))
	{
		RenderSky();
		const auto camera = m_Impl->Camera(SceneRenderInfo());
		const QVector3D direction = camera.forward;
		const QVector3D eye = camera.eye;
		QMatrix4x4 view;
		view.lookAt(eye, eye + direction, camera.up);
		QMatrix4x4 projection;
		const float nearPlane = 0.1f;
		float farPlane = std::max(nearPlane + 1.0f,
			(eye - (m_Impl->mesh.minimum + m_Impl->mesh.maximum) * 0.5f).length() + m_Impl->radius * 2.0f);
		for(const auto& trail : m_Impl->contrails)
			farPlane = std::max(farPlane, float((Point3(eye) - trail->GetPosition()).Length()
				+ trail->GetVelocity().Length() * trail->GetLifeCycle() + trail->GetSize() / 900.f));
		projection.perspective(FieldOfView(), float(std::max(1, width())) / std::max(1, height()), nearPlane, farPlane);
		gl->glEnable(GL_DEPTH_TEST);
		gl->glDepthFunc(GL_LEQUAL);
		gl->glEnable(GL_CULL_FACE);
		gl->glCullFace(GL_BACK);
		gl->glFrontFace(GL_CCW);
		gl->glDisable(GL_BLEND);
		m_Impl->program->bind();
		m_Impl->vao.bind();
		m_Impl->program->setUniformValue("u_mvp", projection * view);
		m_Impl->program->setUniformValue("u_texture", 0);
		m_Impl->program->setUniformValue("u_effect", 1);
		for(int i = 0; i < std::min(3, m_Impl->studioSuns.size()); ++i)
		{
			const AuGLSun& sun = m_Impl->studioSuns[i];
			const QByteArray name = "u_suns[" + QByteArray::number(i) + "].";
			m_Impl->program->setUniformValue((name + "position").constData(), QVector3D(sun.m_LocX, sun.m_LocY, sun.m_LocZ));
			m_Impl->program->setUniformValue((name + "color").constData(), QVector3D(sun.m_ColorR, sun.m_ColorG, sun.m_ColorB));
			m_Impl->program->setUniformValue((name + "attenuation").constData(), sun.m_Attenuation);
			m_Impl->program->setUniformValue((name + "radius").constData(), sun.m_DiskRadius);
		}
		m_Impl->program->setUniformValue("u_translucentPass", false);
		const qint64 elapsed = m_Impl->animationClock.elapsed();
		const float seconds = float(elapsed) / 1000;
		m_Impl->UpdateVisibility(projection * view, seconds);
		int boundSource = -1;
		const auto bindBatch = [this, gl, elapsed, seconds, &boundSource](int source)
		{
			if(source == boundSource) return;
			boundSource = source;
			const auto& batch = m_Impl->mesh.batches[source];
			m_Impl->Texture(source)->bind(0);
			const int material = m_Impl->batchTextures[source].material;
			const bool atlas = batch.textureFile.isEmpty() && material >= 0
				&& material < int(sizeof(ShipTextures) / sizeof(ShipTextures[0]));
			m_Impl->program->setUniformValue("u_atlas", atlas);
			m_Impl->program->setUniformValue("u_inset", atlas
				? QVector2D(0.5f / ShipTextures[material].width, 0.5f / ShipTextures[material].height)
				: QVector2D());
			m_Impl->program->setUniformValue("u_model", m_Impl->transforms[source]);
			m_Impl->program->setUniformValue("u_uvOffset", batch.uvVelocity * seconds);
			m_Impl->program->setUniformValue("u_tint", batch.tint);
			m_Impl->program->setUniformValue("u_ambient", batch.interior
				? QVector3D(0.014352942f, 0.015f, 0.014705883f) : QVector3D(0.125f, 0.125f, 0.125f));
			m_Impl->program->setUniformValue("u_selfIllumination", batch.selfIllumination);
			m_Impl->program->setUniformValue("u_twoSided", batch.twoSided);
			m_Impl->program->setUniformValue("u_studioSide", IsStudioSurface(batch) ? (batch.interior ? 2 : 1) : 0);
			if (batch.twoSided)
				gl->glDisable(GL_CULL_FACE);
			else
				gl->glEnable(GL_CULL_FACE);
			m_Impl->BindEffect(batch, elapsed);
		};
		for (const DrawBatch& batch : m_Impl->visibleBatches)
		{
			bindBatch(batch.source);
			gl->glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
			++frameDraws;
			frameVertices += batch.count;
		}
		m_Impl->vao.release();
		m_Impl->program->release();
		m_Impl->RenderConsole(SceneRenderInfo(), projection, view);
		m_Impl->RenderContrails(*SceneRenderInfo().re, projection, view);
		m_Impl->program->bind();
		m_Impl->vao.bind();
		boundSource = -1; // Native console/particle renderers changed GL state.
		m_Impl->UpdateTransparentOrder(direction);
		if(m_Impl->transparentBufferReady && !m_Impl->transparentBatches.isEmpty())
		{
			const bool bound = m_Impl->transparentBuffer.bind();
			if(bound)
			{
				gl->glEnable(GL_BLEND);
				gl->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				gl->glDepthMask(GL_FALSE);
				m_Impl->program->setUniformValue("u_translucentPass", true);
				for (const DrawBatch& batch : m_Impl->transparentBatches)
				{
					bindBatch(batch.source);
					gl->glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
						reinterpret_cast<const void*>(quintptr(batch.first) * sizeof(quint32)));
					++frameDraws;
					frameVertices += batch.count;
				}
			}
			else
				m_Impl->glError = QStringLiteral("Unable to upload spacecraft window geometry to the graphics device.");
			if (bound)
				m_Impl->transparentBuffer.release();
		}
		gl->glDepthMask(GL_TRUE);
		gl->glDisable(GL_BLEND);
		m_Impl->vao.release();
		m_Impl->program->release();
		gl->glActiveTexture(GL_TEXTURE1);
		gl->glBindTexture(GL_TEXTURE_2D, 0);
		gl->glActiveTexture(GL_TEXTURE0);
		gl->glBindTexture(GL_TEXTURE_2D, 0);
		gl->glDisable(GL_CULL_FACE);
		gl->glDisable(GL_DEPTH_TEST);
	}
	QPainter painter(this);
	painter.setPen(QColor(195, 205, 215));
	// Draw HUD after the scene, independently of whether any ship mesh exists.
	// The native crosshair is a yellow textured dot with four spreading arms.
	if(SceneRenderInfo().m_ShowHUD && !SceneRenderInfo().IsViewTopDown()) PaintCrosshair(painter, size(), m_Impl->targetLevel);
	if (SceneRenderInfo().m_ShowHUD && !SceneRenderInfo().IsViewTopDown() && m_Impl->hasTarget && m_Impl->targetLevel > 0
		&& (m_Impl->targetPoint - m_Impl->Eye()).length() <= m_Impl->targetLevel * 10)
		PaintShipTarget(painter, size(), m_Impl->targetName, m_Impl->targetHealth);
	QString tips = m_Impl->glError;
	if(tips.isEmpty() && m_Impl->mesh.batches.isEmpty())
		tips = QStringLiteral("Paint cells in the floor plan to create a spacecraft.");
	if(tips.isEmpty() && m_Impl->animationClock.elapsed() < m_Impl->interactionUntil)
		tips = m_Impl->interaction;
	if(tips != m_Impl->reportedTips)
	{
		m_Impl->reportedTips = tips;
		if(tipsChanged) tipsChanged(tips);
	}
	painter.beginNativePainting();
	RenderSceneInterface(m_Impl->mouseCaptured);
	RenderSceneCommunications();
	painter.endNativePainting();
	if(m_Impl->profileRender)
	{
		++m_Impl->profileFrames;
		m_Impl->profileNanoseconds += frameClock.nsecsElapsed();
		m_Impl->profileDraws += frameDraws;
		m_Impl->profileVertices += frameVertices;
		const qint64 now = m_Impl->animationClock.elapsed();
		const qint64 interval = now - m_Impl->profileLastReport;
		if(interval >= 1000)
		{
			const double frames = double(m_Impl->profileFrames);
			qInfo().nospace() << "Legacy render: " << frames * 1000.0 / interval
				<< " fps, CPU paint " << m_Impl->profileNanoseconds / frames / 1000000.0
				<< " ms, draws " << m_Impl->profileDraws / frames
				<< ", triangles " << m_Impl->profileVertices / frames / 3.0
				<< ", visible ranges " << m_Impl->visibleBatches.size()
				<< ", chunks " << m_Impl->chunks.size()
				<< ", transparency rebuilds " << m_Impl->profileSorts;
			m_Impl->profileLastReport = now;
			m_Impl->profileFrames = m_Impl->profileNanoseconds = m_Impl->profileDraws
				= m_Impl->profileVertices = m_Impl->profileSorts = 0;
		}
	}
}

void AuLegacyShipRender::CenterMouse()
{
	const QPoint center = mapToGlobal(rect().center());
	if (QCursor::pos() != center) QCursor::setPos(center);
	// Some platforms cannot warp the cursor. Sample the resulting position so
	// a rejected warp cannot apply the same look delta repeatedly while idle.
	m_Impl->mouseAnchor = QCursor::pos();
}

void AuLegacyShipRender::ReleaseMouseCapture()
{
	if (QWidget::mouseGrabber() == this) releaseMouse();
	m_Impl->mouseCaptured = false;
	SetSceneInterfaceInput(false);
}

void AuLegacyShipRender::UpdateMouseCapture()
{
	if (m_Impl->mouseCaptured && g_AuClient.MouseGrab() && QWidget::mouseGrabber() != this)
	{
		m_Impl->StopMovement(true);
		ReleaseMouseCapture();
	}
	const bool wanted = m_Impl->Walkthrough() && isVisible() && hasFocus() && isActiveWindow()
		&& !m_Impl->freeMouse && !m_Impl->temporaryHand
		&& !QApplication::activeModalWidget() && !QApplication::activePopupWidget()
		&& (!QWidget::mouseGrabber() || QWidget::mouseGrabber() == this);
	if (wanted == m_Impl->mouseCaptured)
	{
		if(!g_AuClient.MouseGrab() && QWidget::mouseGrabber() == this) releaseMouse();
		return;
	}
	if (wanted)
	{
		m_Impl->handClick = false;
		SetSceneInterfaceInput(true);
		if(g_AuClient.MouseGrab()) grabMouse(Qt::BlankCursor);
		m_Impl->mouseCaptured = !g_AuClient.MouseGrab() || QWidget::mouseGrabber() == this;
		if (m_Impl->mouseCaptured)
		{
			setCursor(Qt::BlankCursor);
			CenterMouse();
		}
	}
	else
	{
		m_Impl->StopMovement(true);
		ReleaseMouseCapture();
	}
	update();
}

void AuLegacyShipRender::mousePressEvent(QMouseEvent* event)
{
	if(SceneRenderInfo().m_ShowHUD && SceneInterfaceMousePress(event->pos()))
	{
		event->accept();
		return;
	}
	if(!m_Impl->Walkthrough()) { event->ignore(); return; }
	if(m_Impl->toggleControl.TestMouseTriggered(event->button(), event->modifiers(), QPoint()))
	{
		m_Impl->freeMouse = !m_Impl->freeMouse;
		setFocus(Qt::MouseFocusReason);
		UpdateMouseCapture();
		event->accept();
		return;
	}
	setFocus(Qt::MouseFocusReason);
	m_Impl->temporaryHand = m_Impl->HandInput(event->modifiers());
	UpdateMouseCapture();
	if(m_Impl->mouseCaptured)
	{
		m_Impl->mouseButtons |= event->button();
		m_Impl->pendingMouseButtons |= event->button();
	}
	else if(event->button() == Qt::LeftButton)
	{
		m_Impl->handClick = true;
		m_Impl->mousePressed = event->pos();
	}
	event->accept();
}

void AuLegacyShipRender::mouseMoveEvent(QMouseEvent* event)
{
	if(m_Impl->handClick && (event->pos() - m_Impl->mousePressed).manhattanLength() > 3)
		m_Impl->handClick = false;
	event->accept();
}

void AuLegacyShipRender::mouseReleaseEvent(QMouseEvent* event)
{
	if(SceneRenderInfo().m_ShowHUD && SceneInterfaceMouseRelease(event->pos()))
	{
		event->accept();
		return;
	}
	m_Impl->mouseButtons &= ~event->button();
	if(event->button() == Qt::LeftButton)
	{
		if(m_Impl->handClick && !m_Impl->mouseCaptured && !UseConsole(event->pos())) UseDoor(event->pos());
		m_Impl->handClick = false;
	}
	event->accept();
}

void AuLegacyShipRender::mouseDoubleClickEvent(QMouseEvent* event)
{
	mousePressEvent(event);
}

void AuLegacyShipRender::wheelEvent(QWheelEvent* event)
{
	if(!m_Impl->Walkthrough()) { event->ignore(); return; }
	m_Impl->sceneControls.SampleWheelInput(m_Impl->pendingSceneControls, event, AuId(0), 0, false, AuPoint(0, 0), 0);
	if(m_Impl->toggleControl.TestWheelTriggered(event))
	{
		m_Impl->freeMouse = !m_Impl->freeMouse;
		UpdateMouseCapture();
	}
	else if(m_Impl->mouseCaptured)
	{
		m_Impl->controls.SampleWheelInput(m_Impl->pendingControls, event, AuId(0), 0, false, AuPoint(0, 0), 0);
		SystemBindings& systems = m_Impl->systemBindings[m_Impl->SystemBindingIndex()];
		DiControlInputArray active;
		systems.controls.SampleWheelInput(active, event, AuId(0), 0, false, AuPoint(0, 0), 0);
		for(const DiControlInput& control : active)
			OperateSystem(systems.actions.value(control.GetId(), -1));
		systems.controls.ClearRanges();
	}
	event->accept();
}

void AuLegacyShipRender::keyPressEvent(QKeyEvent* event)
{
	if(!m_Impl->Walkthrough()) { QOpenGLWidget::keyPressEvent(event); return; }
	const Qt::Key key = Qt::Key(event->key());
	for(const DiControlInput& control : m_Impl->sceneControls)
		if(m_Impl->MatchesKey(control, event))
		{
			if(!event->isAutoRepeat())
			{
				m_Impl->keys.emplace(key, AuElapsedTimer());
				m_Impl->releasedKeys.remove(key);
			}
			event->accept();
			return;
		}
	if(m_Impl->MatchesKey(m_Impl->toggleControl, event))
	{
		if(!event->isAutoRepeat())
		{
			m_Impl->freeMouse = !m_Impl->freeMouse;
			UpdateMouseCapture();
		}
		event->accept();
		return;
	}
	if(m_Impl->mouseCaptured)
		for(const DiControlInput& control : m_Impl->systemBindings[m_Impl->SystemBindingIndex()].controls)
			if(m_Impl->MatchesKey(control, event))
			{
				if(!event->isAutoRepeat())
				{
					m_Impl->keys.emplace(key, AuElapsedTimer());
					m_Impl->releasedKeys.remove(key);
				}
				event->accept();
				return;
			}
	// Retain the recovered legacy door shortcuts only when the user has not
	// assigned the same combination to a supported game control.
	if(!m_Impl->HasKeyBinding(event) && event->modifiers() == Qt::ControlModifier
		&& (key == Qt::Key_P || key == Qt::Key_C))
	{
		if(!event->isAutoRepeat())
		{
			const bool open = key == Qt::Key_P;
			const quint32 id = ClosestDoor(m_Impl->doors, m_Impl->doorStates, m_Impl->character.feet, !open);
			if(id) OperateDoor(id, open);
			else
			{
				m_Impl->interaction = QStringLiteral("No %1 door or hatch within 6 feet.")
					.arg(open ? QStringLiteral("closed") : QStringLiteral("open"));
				m_Impl->interactionUntil = m_Impl->animationClock.elapsed() + 2500;
			}
		}
		event->accept();
		return;
	}
	if(key == Qt::Key_Home && event->modifiers() == Qt::NoModifier && !m_Impl->HasKeyBinding(event))
	{
		ResetCharacter();
		event->accept();
		return;
	}
	for(const DiControlInput& control : m_Impl->controls)
		if(control.Key() == key && (m_Impl->mouseCaptured || control.GetId() == DaPerson::CI_DoorOpen))
		{
			if(!event->isAutoRepeat())
			{
				m_Impl->keys.emplace(key, AuElapsedTimer());
				m_Impl->releasedKeys.remove(key);
			}
			event->accept();
			return;
		}
	QOpenGLWidget::keyPressEvent(event);
}

void AuLegacyShipRender::keyReleaseEvent(QKeyEvent* event)
{
	auto key = m_Impl->keys.find(Qt::Key(event->key()));
	if(key != m_Impl->keys.end())
	{
		if(!event->isAutoRepeat())
		{
			// Keep a very short tap until the next input sample has seen it.
			if(!key->second.isValid()) m_Impl->releasedKeys.insert(event->key());
			else m_Impl->keys.erase(key);
		}
		event->accept();
		return;
	}
	QOpenGLWidget::keyReleaseEvent(event);
}

bool AuLegacyShipRender::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == this && event->type() == QEvent::ToolTip && m_Impl->mouseCaptured) return true;
	if (isVisible() && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease))
	{
		const QKeyEvent* key = static_cast<QKeyEvent*>(event);
		if (key->key() == m_Impl->handControl.Key() && !key->isAutoRepeat())
		{
			// Observe releases over tool windows too, without consuming their keys.
			if(event->type() == QEvent::KeyPress) m_Impl->handKeys.insert(key->key());
			else m_Impl->handKeys.remove(key->key());
			m_Impl->temporaryHand = m_Impl->HandInput(QApplication::queryKeyboardModifiers());
			UpdateMouseCapture();
		}
	}
	if (event->type() == QEvent::ApplicationDeactivate
		|| (event->type() == QEvent::WindowDeactivate && watched == window()))
	{
		m_Impl->StopMovement();
		m_Impl->handKeys.clear();
		ReleaseMouseCapture();
		m_Impl->sound.Stop();
	}
	return QOpenGLWidget::eventFilter(watched, event);
}

void AuLegacyShipRender::focusInEvent(QFocusEvent* event)
{
	QOpenGLWidget::focusInEvent(event);
	m_Impl->StopMovement();
	m_Impl->temporaryHand = m_Impl->HandInput(QApplication::queryKeyboardModifiers());
	UpdateMouseCapture();
}

void AuLegacyShipRender::focusOutEvent(QFocusEvent* event)
{
	m_Impl->StopMovement();
	ReleaseMouseCapture();
	QOpenGLWidget::focusOutEvent(event);
}

void AuLegacyShipRender::showEvent(QShowEvent* event)
{
	QOpenGLWidget::showEvent(event);
	m_Impl->freeMouse = false;
	m_Impl->temporaryHand = m_Impl->HandInput(QApplication::queryKeyboardModifiers());
	QTimer::singleShot(0, this, [this]() { UpdateMouseCapture(); });
}

void AuLegacyShipRender::hideEvent(QHideEvent* event)
{
	m_Impl->StopMovement();
	ReleaseMouseCapture();
	m_Impl->sound.Stop();
	QOpenGLWidget::hideEvent(event);
}
