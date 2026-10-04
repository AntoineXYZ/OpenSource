#include "aulegacyshiprender.h"
#include "aulegacyship.h"
#include "aulegacyshipmesh.h"

#include <QImage>
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
#include <QSet>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector3D>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace
{
const float Pi = 3.14159265358979323846f;
const float FieldOfView = 40.0f;

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

struct RenderVertex
{
	float position[3];
	float normal[3];
	float uv[2];
};

struct DrawBatch
{
	int material;
	int first;
	int count;
	int deck;
};

struct TransparentTriangle
{
	int material;
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
	AuLegacyShip::Rendering::Mesh mesh;
	QVector<DrawBatch> batches;
	QVector<TransparentTriangle> transparentTriangles;
	QOpenGLBuffer buffer;
	QOpenGLBuffer transparentBuffer{QOpenGLBuffer::IndexBuffer};
	QOpenGLVertexArrayObject vao;
	QOpenGLShaderProgram* program = nullptr;
	QMap<int, QOpenGLTexture*> textures;
	QSet<int> transparentMaterials;
	QImage atlas;
	QMetaObject::Connection contextConnection;
	QVector3D target;
	QPoint mouse;
	Qt::MouseButton dragButton = Qt::NoButton;
	QString glError;
	QStringList textureWarnings;
	float radius = 10.0f;
	float distance = 40.0f;
	float azimuth = -55.0f;
	float elevation = 30.0f;
	int deck = 0;
	bool cutaway = false;
	bool dirty = true;
	bool ready = false;
	bool autoFit = true;
	bool panning = false;

	QVector3D CameraDirection() const
	{
		const float yaw = azimuth * Pi / 180.0f;
		const float pitch = elevation * Pi / 180.0f;
		return QVector3D(std::cos(pitch) * std::cos(yaw),
			std::cos(pitch) * std::sin(yaw), std::sin(pitch));
	}

	float FittedDistance(float aspect) const
	{
		const float vertical = FieldOfView * Pi / 360.0f;
		const float horizontal = std::atan(std::tan(vertical) * aspect);
		return radius * 1.12f / std::sin(std::min(vertical, horizontal));
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
		for (const auto& batch : mesh.batches)
		{
			if (batch.vertices.isEmpty())
				continue;
			DrawBatch draw = { int(batch.material), vertices.size(), batch.vertices.size(), batch.deck };
			batches.append(draw);
			Texture(draw.material);
			if (transparentMaterials.contains(draw.material))
			{
				for (int i = 0; i + 2 < batch.vertices.size(); i += 3)
				{
					TransparentTriangle triangle = { draw.material, draw.first + i, draw.deck,
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

	QOpenGLTexture* Texture(int material)
	{
		if (textures.contains(material))
			return textures.value(material);
		QImage tile;
		if (material >= 0 && material < int(sizeof(ShipTextures) / sizeof(ShipTextures[0])))
		{
			if (atlas.isNull())
				atlas.load(QStringLiteral(":/data/txSpacecraft0Lit.png"));
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
			const QString warning = QStringLiteral("The original spacecraft texture could not be loaded (material %1).")
				.arg(material);
			textureWarnings.append(warning);
			tile = QImage(1, 1, QImage::Format_RGBA8888);
			tile.fill(QColor(175, 185, 195));
		}
		tile = tile.convertToFormat(QImage::Format_ARGB32);
		for (int y = 0; y < tile.height() && !transparentMaterials.contains(material); ++y)
		{
			const QRgb* pixels = reinterpret_cast<const QRgb*>(tile.constScanLine(y));
			for (int x = 0; x < tile.width(); ++x)
			{
				const int alpha = qAlpha(pixels[x]);
				if (alpha < 255)
				{
					transparentMaterials.insert(material);
					break;
				}
			}
		}
		QOpenGLTexture* texture = new QOpenGLTexture(tile);
		texture->setWrapMode(QOpenGLTexture::Repeat);
		texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
		texture->setMagnificationFilter(QOpenGLTexture::Linear);
		textures.insert(material, texture);
		return texture;
	}
};

AuLegacyShipRender::AuLegacyShipRender(QWidget* parent)
	: QOpenGLWidget(parent), m_Impl(new Impl)
{
	QSurfaceFormat requested = format();
	requested.setDepthBufferSize(24);
	requested.setSamples(4);
	setFormat(requested);
	setFocusPolicy(Qt::StrongFocus);
	setAccessibleName(QStringLiteral("Legacy spacecraft 3D rendering"));
	setToolTip(QStringLiteral("Drag to orbit. Right, middle, or Shift-drag to pan. "
		"Scroll to zoom. Press F or Home to fit the spacecraft."));
	setMinimumSize(160, 120);
}

AuLegacyShipRender::~AuLegacyShipRender()
{
	CleanupGL();
	delete m_Impl;
}

void AuLegacyShipRender::SetDesign(const AuLegacyShip::Design& design)
{
	const bool wasEmpty = m_Impl->mesh.batches.isEmpty();
	m_Impl->mesh = AuLegacyShip::Rendering::BuildMesh(design);
	m_Impl->radius = std::max(1.0f, (m_Impl->mesh.maximum - m_Impl->mesh.minimum).length() * 0.5f);
	m_Impl->dirty = true;
	if (m_Impl->ready)
		m_Impl->glError.clear();
	if (wasEmpty || m_Impl->autoFit)
		FitDesign();
	update();
}

void AuLegacyShipRender::SetDeck(int deck)
{
	m_Impl->deck = deck;
	update();
}

void AuLegacyShipRender::SetCutaway(bool enabled)
{
	m_Impl->cutaway = enabled;
	update();
}

void AuLegacyShipRender::FitDesign()
{
	m_Impl->target = (m_Impl->mesh.minimum + m_Impl->mesh.maximum) * 0.5f;
	m_Impl->distance = m_Impl->FittedDistance(float(std::max(1, width())) / std::max(1, height()));
	m_Impl->autoFit = true;
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
OUT vec3 v_normal;
OUT vec3 v_position;
OUT vec2 v_uv;
void main()
{
	v_position = a_position;
	v_normal = a_normal;
	v_uv = a_uv;
	gl_Position = u_mvp * vec4(a_position, 1.0);
}
)";
	const QByteArray fragment = version + (es
		? "#define IN varying\n#define SAMPLE texture2D\n#define COLOR gl_FragColor\n"
		: "#define IN in\n#define SAMPLE texture\nout vec4 fragColor;\n#define COLOR fragColor\n") + R"(
IN vec3 v_normal;
IN vec3 v_position;
IN vec2 v_uv;
uniform sampler2D u_texture;
uniform vec2 u_inset;
uniform vec3 u_light;
uniform float u_clipHeight;
uniform bool u_cutaway;
uniform bool u_translucentPass;
void main()
{
	if (u_cutaway && v_position.z > u_clipHeight)
		discard;
	vec2 uv = fract(v_uv);
	uv = mix(u_inset, vec2(1.0) - u_inset, uv);
	vec4 texel = SAMPLE(u_texture, uv);
	if (texel.a < 0.01 || u_translucentPass != (texel.a < 0.999))
		discard;
	float diffuse = max(0.0, dot(normalize(v_normal), u_light));
	COLOR = vec4(texel.rgb * (0.48 + 0.52 * diffuse), texel.a);
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
	qDeleteAll(m_Impl->textures);
	m_Impl->textures.clear();
	m_Impl->transparentMaterials.clear();
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

void AuLegacyShipRender::resizeGL(int, int)
{
	if (m_Impl->autoFit)
		FitDesign();
}

void AuLegacyShipRender::paintGL()
{
	QOpenGLFunctions* gl = context()->functions();
	gl->glClearColor(0.055f, 0.075f, 0.10f, 1.0f);
	gl->glDepthMask(GL_TRUE);
	gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (m_Impl->ready && (!m_Impl->dirty || m_Impl->Upload()))
	{
		const QVector3D direction = m_Impl->CameraDirection();
		const QVector3D eye = m_Impl->target + direction * m_Impl->distance;
		QMatrix4x4 view;
		view.lookAt(eye, m_Impl->target, QVector3D(0.0f, 0.0f, 1.0f));
		QMatrix4x4 projection;
		const float nearPlane = std::max(0.02f, std::min(m_Impl->radius * 0.01f, m_Impl->distance * 0.01f));
		const float farPlane = std::max(nearPlane + 1.0f,
			(eye - (m_Impl->mesh.minimum + m_Impl->mesh.maximum) * 0.5f).length() + m_Impl->radius * 2.0f);
		projection.perspective(FieldOfView, float(std::max(1, width())) / std::max(1, height()), nearPlane, farPlane);
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
		m_Impl->program->setUniformValue("u_cutaway", m_Impl->cutaway);
		m_Impl->program->setUniformValue("u_clipHeight", float(m_Impl->deck * AuLegacyShip::DeckHeight + 4.0));
		m_Impl->program->setUniformValue("u_light", (direction + QVector3D(0.15f, -0.1f, 0.5f)).normalized());
		m_Impl->program->setUniformValue("u_translucentPass", false);
		const auto bindTexture = [this](int material)
		{
			QOpenGLTexture* texture = m_Impl->Texture(material);
			texture->bind(0);
			const bool known = material >= 0 && material < int(sizeof(ShipTextures) / sizeof(ShipTextures[0]));
			m_Impl->program->setUniformValue("u_inset", known
				? QVector2D(0.5f / ShipTextures[material].width, 0.5f / ShipTextures[material].height)
				: QVector2D(0.0f, 0.0f));
		};
		for (const DrawBatch& batch : m_Impl->batches)
		{
			if (m_Impl->cutaway && batch.deck > m_Impl->deck)
				continue;
			bindTexture(batch.material);
			gl->glDrawArrays(GL_TRIANGLES, batch.first, batch.count);
		}
		QVector<const TransparentTriangle*> transparent;
		transparent.reserve(m_Impl->transparentTriangles.size());
		for (const TransparentTriangle& triangle : m_Impl->transparentTriangles)
			if (!m_Impl->cutaway || triangle.deck <= m_Impl->deck)
				transparent.append(&triangle);
		std::sort(transparent.begin(), transparent.end(), [&direction](const TransparentTriangle* a, const TransparentTriangle* b)
		{
			return QVector3D::dotProduct(a->center, direction) < QVector3D::dotProduct(b->center, direction);
		});
		QVector<quint32> indices;
		QVector<DrawBatch> transparentBatches;
		indices.reserve(transparent.size() * 3);
		for (const TransparentTriangle* triangle : transparent)
		{
			if (transparentBatches.isEmpty() || transparentBatches.last().material != triangle->material)
			{
				DrawBatch batch = { triangle->material, indices.size(), 0, triangle->deck };
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
					bindTexture(batch.material);
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
		gl->glBindTexture(GL_TEXTURE_2D, 0);
		gl->glDisable(GL_CULL_FACE);
		gl->glDisable(GL_DEPTH_TEST);
	}
	QPainter painter(this);
	painter.setPen(QColor(195, 205, 215));
	if (!m_Impl->glError.isEmpty())
		painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap, m_Impl->glError);
	else if (m_Impl->mesh.batches.isEmpty())
		painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Paint cells in the floor plan to create a spacecraft."));
	else
	{
		const QString mode = m_Impl->cutaway ? QStringLiteral("Deck %1 cutaway").arg(m_Impl->deck)
			: QStringLiteral("Spacecraft exterior");
		painter.drawText(QRect(12, 10, std::max(0, width() - 24), 24), Qt::AlignLeft | Qt::AlignVCenter, mode);
		painter.drawText(QRect(12, height() - 34, std::max(0, width() - 24), 24), Qt::AlignLeft | Qt::AlignVCenter,
			QStringLiteral("Drag: orbit   Shift / right drag: pan   Scroll: zoom   F: fit"));
		if (!m_Impl->textureWarnings.isEmpty())
			painter.drawText(QRect(12, 38, std::max(0, width() - 24), 40), Qt::AlignLeft | Qt::TextWordWrap,
				QStringLiteral("Original texture unavailable; showing untextured geometry."));
	}
}

void AuLegacyShipRender::mousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton && event->button() != Qt::RightButton)
	{
		QOpenGLWidget::mousePressEvent(event);
		return;
	}
	setFocus(Qt::MouseFocusReason);
	m_Impl->mouse = event->pos();
	m_Impl->dragButton = event->button();
	m_Impl->panning = event->button() != Qt::LeftButton || event->modifiers().testFlag(Qt::ShiftModifier);
	setCursor(Qt::ClosedHandCursor);
	event->accept();
}

void AuLegacyShipRender::mouseMoveEvent(QMouseEvent* event)
{
	if (m_Impl->dragButton == Qt::NoButton || !event->buttons().testFlag(m_Impl->dragButton))
	{
		QOpenGLWidget::mouseMoveEvent(event);
		return;
	}
	const QPoint delta = event->pos() - m_Impl->mouse;
	m_Impl->mouse = event->pos();
	m_Impl->autoFit = false;
	if (m_Impl->panning)
	{
		const QVector3D direction = m_Impl->CameraDirection();
		const QVector3D right = QVector3D::crossProduct(QVector3D(0.0f, 0.0f, 1.0f), direction).normalized();
		const QVector3D up = QVector3D::crossProduct(direction, right).normalized();
		const float scale = 2.0f * m_Impl->distance * std::tan(FieldOfView * Pi / 360.0f) / std::max(1, height());
		m_Impl->target += (-right * float(delta.x()) + up * float(delta.y())) * scale;
	}
	else
	{
		m_Impl->azimuth = std::fmod(m_Impl->azimuth - delta.x() * 0.4f, 360.0f);
		m_Impl->elevation = Bounded(m_Impl->elevation + delta.y() * 0.4f, -89.0f, 89.0f);
	}
	update();
	event->accept();
}

void AuLegacyShipRender::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == m_Impl->dragButton)
	{
		m_Impl->dragButton = Qt::NoButton;
		unsetCursor();
		event->accept();
		return;
	}
	QOpenGLWidget::mouseReleaseEvent(event);
}

void AuLegacyShipRender::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		FitDesign();
		event->accept();
		return;
	}
	QOpenGLWidget::mouseDoubleClickEvent(event);
}

void AuLegacyShipRender::wheelEvent(QWheelEvent* event)
{
	const int delta = event->angleDelta().y();
	if (delta == 0)
	{
		event->ignore();
		return;
	}
	m_Impl->distance = Bounded(m_Impl->distance * std::pow(1.15f, -delta / 120.0f),
		0.2f, std::max(100.0f, m_Impl->radius * 100.0f));
	m_Impl->autoFit = false;
	update();
	event->accept();
}

void AuLegacyShipRender::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F || event->key() == Qt::Key_Home)
	{
		FitDesign();
		event->accept();
		return;
	}
	QOpenGLWidget::keyPressEvent(event);
}
