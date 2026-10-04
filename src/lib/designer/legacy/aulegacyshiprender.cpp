#include "aulegacyshiprender.h"
#include "aulegacyship.h"
#include "aulegacyshipmesh.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipwalk.h"
#include "aulegacyshiphud.h"
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
#include "dicontrolinput.h"
#include "aujoystick.h"
#include "auNebula.h"
#include "dagalaxy.h"

#include <QByteArray>
#include <QApplication>
#include <QCursor>
#include <QImage>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QTimer>
#include <QKeyEvent>
#include <QMap>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
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
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>

namespace
{
const float Pi = 3.14159265358979323846f;
using namespace AuLegacyShip::Rendering;
const qint64 DoorTickMilliseconds = qint64(DoorAnimationTickSeconds * 1000);

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
	int deck;
	QVector3D center;
};

float Bounded(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}
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
	SoundSession sound;
	std::unique_ptr<auNebula> sky;
	unsigned int skyPaneSize = 0;
	bool skyMipMap = false;
	AuPoint skyPosition;
	QVector<DoorDefinition> doors;
	QHash<quint32, DoorAnimation> doorStates;
	std::unordered_map<Qt::Key, AuElapsedTimer, AuKey_Hash> keys;
	QSet<int> releasedKeys, handKeys;
	DiControlInputArray controls, pendingControls;
	DiControlInput handControl, toggleControl;
	Qt::MouseButtons mouseButtons = Qt::NoButton;
	Qt::MouseButtons pendingMouseButtons = Qt::NoButton;
	QVector<DrawBatch> batches;
	QVector<TransparentTriangle> transparentTriangles;
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
	bool continuousForward = false;
	bool jumpRequested = false;
	bool haveDesign = false;
	qint64 lastTick = 0;
	qint64 lastDoorTick = 0;
	qint64 lastTargetTick = 0;
	bool hasTarget = false;
	int targetLevel = 0;
	QVector3D targetPoint;
	QString interaction;
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
		return MatchesKey(handControl, event) || MatchesKey(toggleControl, event);
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
		return character.feet + QVector3D(0, 0, world.Settings().eyeHeight);
	}

	int FrameMaterial(int source) const
	{
		const Batch& batch = mesh.batches[source];
		return int(batch.doorId ? DoorMaterialFrame(batch.material, doorStates.value(batch.doorId).frame)
			: batch.material);
	}

	void StopMovement(bool preserveFall = false)
	{
		keys.clear();
		releasedKeys.clear();
		pendingControls.clear();
		mouseButtons = Qt::NoButton;
		pendingMouseButtons = Qt::NoButton;
		running = false;
		continuousForward = false;
		jumpRequested = false;
		character.velocity = preserveFall && character.gravityActive
			? QVector3D(0, 0, character.velocity.z()) : QVector3D();
		handClick = false;
	}

	bool Upload()
	{
		batches.clear();
		transparentTriangles.clear();
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
			DrawBatch draw = { source, vertices.size(), batch.vertices.size(), batch.deck };
			batches.append(draw);
			Texture(source);
			if (batch.doorId || batch.tint.w() < 0.999f || transparentTextures.contains(TextureKey(source)))
			{
				for (int i = 0; i + 2 < batch.vertices.size(); i += 3)
				{
					TransparentTriangle triangle = { source, draw.first + i, draw.deck,
						(batch.vertices[i].position + batch.vertices[i + 1].position + batch.vertices[i + 2].position) / 3.0f };
					transparentTriangles.append(triangle);
				}
			}
			for (const auto& vertex : batch.vertices)
			{
				RenderVertex value = {
					{ vertex.position.x(), vertex.position.y(), vertex.position.z() },
					{ vertex.normal.x(), vertex.normal.y(), vertex.normal.z() },
					{ vertex.uv.x(), vertex.uv.y() }
				};
				vertices.append(value);
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

	QString TextureKey(int source) const
	{
		const auto& batch = mesh.batches[source];
		return batch.textureFile.isEmpty() ? QStringLiteral("atlas:%1").arg(FrameMaterial(source)) : batch.textureFile;
	}

	QOpenGLTexture* Texture(int source)
	{
		const auto& batch = mesh.batches[source];
		const int material = FrameMaterial(source);
		const QString key = TextureKey(source);
		if (textures.contains(key))
			return textures.value(key);
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
		return texture;
	}

	void BindEffect(const AuLegacyShip::Rendering::Batch& batch, qint64 elapsed)
	{
		const bool enabled = batch.effectFirstFrame >= 0 && batch.effectFrameCount > 0;
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

void AuLegacyShipRender::SetWalkthrough(bool enabled)
{
	if(m_Impl->floorPlan) m_Impl->floorPlan->setVisible(!enabled);
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
	m_Impl->sound.SetDesign(design);
	m_Impl->haveDesign = true;
	m_Impl->mesh = AuLegacyShip::Rendering::BuildMesh(design);
	if(m_Impl->character.ladderEngaged)
		m_Impl->world.ToggleLadder(m_Impl->character);
	m_Impl->world.SetMesh(m_Impl->mesh, design);
	m_Impl->hasTarget = false;
	m_Impl->targetLevel = 0;
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
	update();
}

void AuLegacyShipRender::SetDeck(int deck)
{
	m_Impl->deck = deck;
	m_Impl->world.SetDesignPlane(float(deck * AuLegacyShip::DeckHeight));
	update();
}

void AuLegacyShipRender::SetGravity(bool enabled)
{
	m_Impl->gravity = enabled;
	m_Impl->character.velocity = QVector3D();
	update();
}

void AuLegacyShipRender::ResetCharacter()
{
	m_Impl->StopMovement();
	if (m_Impl->world.FindSpawn(m_Impl->design, m_Impl->deck, m_Impl->character))
	{
		m_Impl->azimuth = 90;
		m_Impl->elevation = 0;
		m_Impl->interaction = QStringLiteral("Returned to the design.");
	}
	else
		m_Impl->interaction = QStringLiteral("No clear standing position on this deck. Select another deck and press Home.");
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
		const int sidebarWidth = SceneInterfaceWidth();
		m_Impl->floorPlan->setGeometry(sidebarWidth, 0, std::max(0, width() - sidebarWidth), height());
		UpdateMouseCapture();
		update();
		return;
	}
	AuApp* app = qobject_cast<AuApp*>(qApp);
	m_Impl->sound.SetAudio(app ? app->Sound() : nullptr);
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
	active += m_Impl->pendingControls;
	m_Impl->pendingControls.clear();
	for(auto& key : m_Impl->keys)
		if(!key.second.isValid()) key.second.start();
	for(int key : m_Impl->releasedKeys) m_Impl->keys.erase(Qt::Key(key));
	m_Impl->releasedKeys.clear();
	if(m_Impl->mouseCaptured && sampleJoystick)
		m_Impl->controls.SampleJoystickInput(active, joystick, AuId(0), 0, false, AuPoint(0, 0), 0);
	float yawChange = 0, pitchChange = 0;
	const auto look = [&yawChange, &pitchChange](const DiControlInput& control, float limit) {
		const float angle = std::min(float(control.GetRangeValue()) / 300.f, limit);
		switch(control.GetId())
		{
		case DaPerson::CI_TurnLeft: yawChange += angle; break;
		case DaPerson::CI_TurnRight: yawChange -= angle; break;
		case DaPerson::CI_LookUp: pitchChange += angle; break;
		case DaPerson::CI_LookDown: pitchChange -= angle; break;
		default: break;
		}
	};
	for(const DiControlInput& control : active) look(control, 1080.f * seconds);
	m_Impl->controls.ClearRanges();
	if (m_Impl->mouseCaptured)
	{
		// Sample the real cursor, rather than queued synthetic warp events.
		// The original input loop also recentres it while controlling the avatar.
		const QPoint delta = QCursor::pos() - m_Impl->mouseAnchor;
		const float unitsPerPixel = float(g_AuClient.MouseLookSensitivity()) * 300.f / std::max(1, width());
		DiControlInputArray mouse;
		const bool streaming = m_Impl->controls.SampleMouseInput(mouse, m_Impl->mouseButtons | m_Impl->pendingMouseButtons, keyModifiers,
			QPoint(), AuId(0), 0, false, AuPoint(0, 0), 0);
		if(!streaming) m_Impl->mouseButtons = Qt::NoButton;
		m_Impl->pendingMouseButtons = Qt::NoButton;
		m_Impl->controls.SampleMouseInput(mouse, Qt::NoButton, keyModifiers,
			QPoint(qRound(delta.x() * unitsPerPixel), qRound(delta.y() * unitsPerPixel)),
			AuId(0), 0, false, AuPoint(0, 0), 0);
		for(const DiControlInput& control : mouse) look(control, 100.f);
		active += mouse;
		m_Impl->azimuth = std::fmod(m_Impl->azimuth + yawChange, 360.0f);
		m_Impl->elevation = Bounded(m_Impl->elevation + pitchChange, -89.0f, 89.0f);
		CenterMouse();
	}
	m_Impl->controls.ClearRanges();
	m_Impl->running = m_Impl->mouseCaptured && active.HasId(DaPerson::CI_Run);
	if(m_Impl->mouseCaptured)
	{
		if(active.HasId(DaPerson::CI_MoveForwardContinuously))
			m_Impl->continuousForward = !m_Impl->continuousForward;
		if(active.HasId(DaPerson::CI_MoveBackward) || active.HasId(DaPerson::CI_Brake)
			|| active.HasId(DaPerson::CI_BrakeOrJump)) m_Impl->continuousForward = false;
		if(active.HasId(DaPerson::CI_Jump) || active.HasId(DaPerson::CI_BrakeOrJump))
			m_Impl->jumpRequested = true;
		if(active.HasId(DaPerson::CI_EnterExit))
		{
			if(m_Impl->world.ToggleLadder(m_Impl->character))
			{
				m_Impl->interaction = m_Impl->character.ladderEngaged
					? QStringLiteral("On ladder.") : QStringLiteral("Released ladder.");
				m_Impl->continuousForward = false;
				if(m_Impl->character.ladderEngaged)
				{
					m_Impl->azimuth = m_Impl->character.ladderFacing;
					m_Impl->elevation = 0;
				}
			}
			else m_Impl->interaction = QStringLiteral("Move closer to a ladder.");
			m_Impl->interactionUntil = now + 2500;
		}
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
	if (hasFocus() && seconds > 0)
	{
		const auto pressed = [this, &active](int id) { return m_Impl->mouseCaptured && active.HasId(id); };
		const float forward = float(pressed(DaPerson::CI_MoveForward) || m_Impl->continuousForward)
			- float(pressed(DaPerson::CI_MoveBackward));
		const float sideways = float(pressed(DaPerson::CI_MoveRight)) - float(pressed(DaPerson::CI_MoveLeft));
		const float vertical = float(pressed(DaPerson::CI_MoveUp)) - float(pressed(DaPerson::CI_MoveDown));
		const float speed = m_Impl->running ? 12.5f : 5.0f;
		const float yaw = m_Impl->azimuth * Pi / 180.0f;
		QVector3D direction = m_Impl->CameraDirection();
		if (m_Impl->character.gravityActive && !m_Impl->character.onLadder)
			direction = QVector3D(std::cos(yaw), std::sin(yaw), 0);
		QVector3D motion = direction * forward + QVector3D(std::sin(yaw), -std::cos(yaw), 0) * sideways
			+ QVector3D(0, 0, vertical);
		if (m_Impl->character.ladderEngaged)
			motion = QVector3D(0, 0, Bounded(forward + vertical, -1.f, 1.f));
		else if (m_Impl->character.gravityActive)
		{
			// Vertical input cannot fly in ship gravity, but Z still requests
			// descent through a supporting automatic hatch. Keep that intent
			// without reducing walking speed when C/Z is held with WASD.
			motion.setZ(0);
			if (motion.lengthSquared() > 1) motion.normalize();
			motion.setZ(vertical);
		}
		else if (motion.lengthSquared() > 1) motion.normalize();
		if (!m_Impl->character.gravityActive && (pressed(DaPerson::CI_BrakeOrJump) || pressed(DaPerson::CI_Brake)))
		{
			motion = QVector3D();
			m_Impl->character.velocity = QVector3D();
		}
		m_Impl->world.Step(m_Impl->character, motion * speed, seconds, m_Impl->gravity, m_Impl->jumpRequested);
		m_Impl->jumpRequested = false;
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
	UpdateTarget(now);
	if (isActiveWindow())
	{
		SoundListener listener;
		listener.feet = m_Impl->character.feet;
		listener.eye = m_Impl->Eye();
		listener.forward = m_Impl->CameraDirection();
		listener.up = QVector3D::crossProduct(QVector3D::crossProduct(listener.forward, QVector3D(0, 0, 1)),
			listener.forward).normalized();
		listener.inShip = m_Impl->character.inShip;
		listener.grounded = m_Impl->character.grounded;
		listener.climbing = m_Impl->character.onLadder;
		listener.running = m_Impl->running;
		const QVector3D velocity = m_Impl->character.velocity;
		listener.moving = hasFocus() && (velocity.x() * velocity.x() + velocity.y() * velocity.y()
			+ (listener.climbing ? velocity.z() * velocity.z() : 0) > .001f);
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
		// The preview has one target atom, the ship. The original sight worker
		// doubles a steady target's lock every 200 ms, starting at level one.
		m_Impl->targetLevel = m_Impl->hasTarget
			? (m_Impl->targetLevel ? std::min(15, m_Impl->targetLevel * 2) : 1)
			: m_Impl->targetLevel / 3;
		m_Impl->hasTarget = true;
		m_Impl->targetPoint = hit.point;
	}
	else
	{
		m_Impl->targetLevel /= 2;
		if (!m_Impl->targetLevel) m_Impl->hasTarget = false;
	}
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
	m_Impl->interaction = open ? QStringLiteral("Opening door / hatch.") : QStringLiteral("Closing door / hatch.");
	m_Impl->interactionUntil = m_Impl->animationClock.elapsed() + 1800;
	update();
}

void AuLegacyShipRender::UseDoor(const QPoint& screenPosition)
{
	const QVector3D forward = m_Impl->CameraDirection();
	const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 0, 1)).normalized();
	const QVector3D up = QVector3D::crossProduct(right, forward).normalized();
	const float aspect = float(std::max(1, width())) / std::max(1, height());
	const float tangent = std::tan(FieldOfView() * Pi / 360.0f);
	const float x = 2.0f * screenPosition.x() / std::max(1, width()) - 1;
	const float y = 1 - 2.0f * screenPosition.y() / std::max(1, height());
	const QVector3D direction = (forward + right * (x * tangent * aspect) + up * (y * tangent)).normalized();
	WalkHit hit;
	if (m_Impl->world.Raycast(m_Impl->Eye(), direction, 16, hit) && hit.doorId)
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
OUT vec3 v_normal;
OUT vec3 v_position;
OUT vec2 v_uv;
void main()
{
	v_position = (u_model * vec4(a_position, 1.0)).xyz;
	v_normal = mat3(u_model) * a_normal;
	v_uv = a_uv + u_uvOffset;
	gl_Position = u_mvp * vec4(v_position, 1.0);
}
)";
	const QByteArray fragment = version + (es
		? "#define IN varying\n#define SAMPLE texture2D\n#define COLOR gl_FragColor\n"
		: "#define IN in\n#define SAMPLE texture\nout vec4 fragColor;\n#define COLOR fragColor\n") + R"(
IN vec3 v_normal;
IN vec3 v_position;
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
uniform vec3 u_light;
uniform bool u_translucentPass;
void main()
{
	vec2 uv = u_atlas ? mix(u_inset, vec2(1.0) - u_inset, fract(v_uv)) : v_uv;
	vec4 texel = SAMPLE(u_texture, uv) * u_tint;
	if (texel.a < 0.01 || u_translucentPass != (texel.a < 0.999))
		discard;
	vec3 normal = normalize(v_normal);
	if (u_twoSided && !gl_FrontFacing)
		normal = -normal;
	float diffuse = max(0.0, dot(normal, u_light));
	vec3 color = texel.rgb * mix(0.48 + 0.52 * diffuse, 1.0, u_selfIllumination);
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
	if(m_Impl->sky)
	{
		m_Impl->sky->DeleteDeviceObjects(*SceneRenderInfo().re);
		m_Impl->sky.reset();
	}
	qDeleteAll(m_Impl->textures);
	m_Impl->textures.clear();
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

void AuLegacyShipRender::RenderSky()
{
	if(!g_Au.DrawSkyBox() || !g_AuClient.DesignerShowSkyBox()) return;
	AuAtomRenderInfo& ari = SceneRenderInfo();
	AuGLEffects& re = *ari.re;
	const unsigned int paneSize = g_Au.SkyBoxPaneSize();
	const bool mipMap = g_Au.SkyBoxMipMap();
	if(m_Impl->sky && (m_Impl->skyPaneSize != paneSize || m_Impl->skyMipMap != mipMap))
	{
		m_Impl->sky->DeleteDeviceObjects(re);
		m_Impl->sky.reset();
	}
	if(!m_Impl->sky)
	{
		// The 2016 studio generated a temporary Shores of Hazeron sector.
		// Use a repeatable preview sector; no environment is stored in .ship.
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
		m_Impl->skyPosition = sector;
		// Deep space uses the game's non-nebula sky. A nearby nebula must not
		// be invented merely because the editor has no surrounding world.
		m_Impl->sky.reset(new auNebula(0, 0, -2.9924924f, .38151205f, -1.8401746f, -1.8524430f,
			30, 0, false, AuPoint(), 2016, 0, galaxy.Color()));
		m_Impl->sky->CreateGalacticStarField(galaxy, sector, dice);
		m_Impl->skyPaneSize = paneSize;
		m_Impl->skyMipMap = mipMap;
		GLint drawFramebuffer, readFramebuffer, viewport[4];
		re.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
		re.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
		re.glGetIntegerv(GL_VIEWPORT, viewport);
		re.glDisable(GL_DEPTH_TEST);
		re.glDisable(GL_CULL_FACE);
		re.glDisable(GL_SCISSOR_TEST);
		re.glActiveTexture(GL_TEXTURE0);
		m_Impl->sky->CreateSkyBox(ari, m_Impl->skyPosition, paneSize, mipMap);
		// QOpenGLWidget renders into its own FBO, with a physical-pixel viewport.
		re.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
		re.glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
		re.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	}
	if(!m_Impl->sky->m_img) return;
	QMatrix4x4 rotation;
	rotation.lookAt(QVector3D(), m_Impl->CameraDirection(), QVector3D(0, 0, 1));
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
	QOpenGLFunctions* gl = context()->functions();
	gl->glClearColor(0.055f, 0.075f, 0.10f, 1.0f);
	gl->glDepthMask(GL_TRUE);
	gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if(!m_Impl->Walkthrough())
	{
		RenderSceneInterface(false);
		return;
	}
	if (m_Impl->ready && (!m_Impl->dirty || m_Impl->Upload()))
	{
		RenderSky();
		const QVector3D direction = m_Impl->CameraDirection();
		const QVector3D eye = m_Impl->Eye();
		QMatrix4x4 view;
		view.lookAt(eye, eye + direction, QVector3D(0.0f, 0.0f, 1.0f));
		QMatrix4x4 projection;
		const float nearPlane = 0.1f;
		const float farPlane = std::max(nearPlane + 1.0f,
			(eye - (m_Impl->mesh.minimum + m_Impl->mesh.maximum) * 0.5f).length() + m_Impl->radius * 2.0f);
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
		m_Impl->program->setUniformValue("u_light", (-direction + QVector3D(0.15f, -0.1f, 0.5f)).normalized());
		m_Impl->program->setUniformValue("u_translucentPass", false);
		const qint64 elapsed = m_Impl->animationClock.elapsed();
		const float seconds = float(elapsed) / 1000;
		QVector<QMatrix4x4> transforms(m_Impl->mesh.batches.size());
		for (int i = 0; i < transforms.size(); ++i)
		{
			const auto& batch = m_Impl->mesh.batches[i];
			if (batch.degreesPerSecond != 0)
			{
				transforms[i].translate(batch.rotationPivot);
				transforms[i].rotate(std::fmod(seconds * batch.degreesPerSecond, 360.0f), batch.rotationAxis);
				transforms[i].translate(-batch.rotationPivot);
			}
		}
		const auto bindBatch = [this, gl, elapsed, seconds, &transforms](int source)
		{
			const auto& batch = m_Impl->mesh.batches[source];
			const int material = m_Impl->FrameMaterial(source);
			m_Impl->Texture(source)->bind(0);
			const bool atlas = batch.textureFile.isEmpty() && material >= 0
				&& material < int(sizeof(ShipTextures) / sizeof(ShipTextures[0]));
			m_Impl->program->setUniformValue("u_atlas", atlas);
			m_Impl->program->setUniformValue("u_inset", atlas
				? QVector2D(0.5f / ShipTextures[material].width, 0.5f / ShipTextures[material].height)
				: QVector2D());
			m_Impl->program->setUniformValue("u_model", transforms[source]);
			m_Impl->program->setUniformValue("u_uvOffset", batch.uvVelocity * seconds);
			m_Impl->program->setUniformValue("u_tint", batch.tint);
			m_Impl->program->setUniformValue("u_selfIllumination", batch.selfIllumination);
			m_Impl->program->setUniformValue("u_twoSided", batch.twoSided);
			if (batch.twoSided)
				gl->glDisable(GL_CULL_FACE);
			else
				gl->glEnable(GL_CULL_FACE);
			m_Impl->BindEffect(batch, elapsed);
		};
		for (const DrawBatch& batch : m_Impl->batches)
		{
			bindBatch(batch.source);
			gl->glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
		}
		QVector<const TransparentTriangle*> transparent;
		transparent.reserve(m_Impl->transparentTriangles.size());
		for (const TransparentTriangle& triangle : m_Impl->transparentTriangles)
			transparent.append(&triangle);
		std::sort(transparent.begin(), transparent.end(), [&direction, &transforms](const TransparentTriangle* a, const TransparentTriangle* b)
		{
			return QVector3D::dotProduct(transforms[a->source].map(a->center), direction)
				> QVector3D::dotProduct(transforms[b->source].map(b->center), direction);
		});
		QVector<quint32> indices;
		QVector<DrawBatch> transparentBatches;
		indices.reserve(transparent.size() * 3);
		for (const TransparentTriangle* triangle : transparent)
		{
			if (transparentBatches.isEmpty() || transparentBatches.last().source != triangle->source)
			{
				DrawBatch batch = { triangle->source, indices.size(), 0, triangle->deck };
				transparentBatches.append(batch);
			}
			transparentBatches.last().count += 3;
			for (int i = 0; i < 3; ++i)
				indices.append(quint32(triangle->first + i));
		}
		if (!indices.isEmpty())
		{
			const int bytes = indices.size() * int(sizeof(quint32));
			const bool bound = m_Impl->transparentBuffer.bind();
			if (bound)
				m_Impl->transparentBuffer.allocate(indices.constData(), bytes);
			if (bound && m_Impl->transparentBuffer.size() == bytes)
			{
				gl->glEnable(GL_BLEND);
				gl->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				gl->glDepthMask(GL_FALSE);
				m_Impl->program->setUniformValue("u_translucentPass", true);
				for (const DrawBatch& batch : transparentBatches)
				{
					bindBatch(batch.source);
					gl->glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
						reinterpret_cast<const void*>(quintptr(batch.first) * sizeof(quint32)));
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
	PaintCrosshair(painter, size(), m_Impl->targetLevel);
	if (m_Impl->hasTarget && m_Impl->targetLevel > 0
		&& (m_Impl->targetPoint - m_Impl->Eye()).length() <= m_Impl->targetLevel * 10)
		PaintShipTarget(painter, size(), m_Impl->design.header.name);
	if (!m_Impl->glError.isEmpty())
		painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap, m_Impl->glError);
	else if (m_Impl->mesh.batches.isEmpty())
		painter.drawText(rect().adjusted(24, int(height() * .72), -24, -12), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
			QStringLiteral("Paint cells in the floor plan to create a spacecraft."));
	else
	{
		painter.drawText(QRect(12, height() - 34, std::max(0, width() - 24), 24), Qt::AlignLeft | Qt::AlignVCenter,
			QStringLiteral("Use your Avatar and Scene control bindings. Switch to hand input for the sidebar and doors."));
		if (m_Impl->animationClock.elapsed() < m_Impl->interactionUntil)
			painter.drawText(QRect(12, height() - 64, std::max(0, width() - 24), 28),
				Qt::AlignLeft | Qt::AlignVCenter, m_Impl->interaction);
	}
	painter.beginNativePainting();
	RenderSceneInterface(m_Impl->mouseCaptured);
	painter.endNativePainting();
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
	if(SceneInterfaceMousePress(event->pos()))
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
	if(SceneInterfaceMouseRelease(event->pos()))
	{
		event->accept();
		return;
	}
	m_Impl->mouseButtons &= ~event->button();
	if(event->button() == Qt::LeftButton)
	{
		if(m_Impl->handClick && !m_Impl->mouseCaptured) UseDoor(event->pos());
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
	if(m_Impl->toggleControl.TestWheelTriggered(event))
	{
		m_Impl->freeMouse = !m_Impl->freeMouse;
		UpdateMouseCapture();
	}
	else if(m_Impl->mouseCaptured)
		m_Impl->controls.SampleWheelInput(m_Impl->pendingControls, event, AuId(0), 0, false, AuPoint(0, 0), 0);
	event->accept();
}

void AuLegacyShipRender::keyPressEvent(QKeyEvent* event)
{
	if(!m_Impl->Walkthrough()) { QOpenGLWidget::keyPressEvent(event); return; }
	const Qt::Key key = Qt::Key(event->key());
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
