#include "aulegacyshiphud.h"
#include "aulegacyshipperformance.h"

#include <QColor>
#include <QFontMetricsF>
#include <QImage>
#include <QPainter>
#include <QSize>
#include <QString>
#include <QtGlobal>

static const int TargetFontSize = 24; 

namespace AuLegacyShip
{
namespace Rendering
{
namespace
{
QImage Tint(const QImage& source, const QColor& color)
{
	QImage image = source.convertToFormat(QImage::Format_ARGB32);
	for(int y = 0; y < image.height(); ++y)
	{
		QRgb* row = reinterpret_cast<QRgb*>(image.scanLine(y));
		for(int x = 0; x < image.width(); ++x)
			row[x] = qRgba(qRed(row[x]) * color.red() / 255, qGreen(row[x]) * color.green() / 255,
				qBlue(row[x]) * color.blue() / 255, qAlpha(row[x]));
	}
	return image;
}

struct HudImages
{
	QImage health[3], unused[2], hair, dot;
	QImage systems[16];

	HudImages()
	{
		// Reuse the game's shared HUD atlas. The bar and hull icon pixels are
		// unchanged from 2016; the crosshair is also used by the current game.
		const QImage atlas(QStringLiteral(":/data/HudItems.png"));
		const QImage bar = atlas.copy(271, 34, 129, 10);
		health[0] = Tint(bar, QColor(51, 255, 51));
		health[1] = Tint(bar, QColor(255, 179, 51));
		health[2] = Tint(bar, QColor(255, 51, 51));
		unused[0] = Tint(bar, QColor(128, 89, 26));
		unused[1] = Tint(bar, QColor(128, 26, 26));
		hair = Tint(atlas.copy(0, 58, 48, 5), QColor(255, 230, 26));
		dot = Tint(atlas.copy(97, 58, 5, 5), QColor(255, 230, 26));
		// Legacy SYS_Count=15 maps to the door tile, not modern capacitor.
		for(int i = 0; i < 16; ++i)
			systems[i] = atlas.copy(768 + (i % 8) * 16, 32 + (i / 8) * 16, 16, 16);
	}
};

const HudImages& Images()
{
	static const HudImages images;
	return images;
}
}

void PaintShipTarget(QPainter& painter, const QSize& size, const QString& name, const TargetHealth& health)
{
	if(size.width() <= 0 || size.height() <= 0) return;
	const HudImages& images = Images();
	// Native RenderBodyStats uses aspect-correct normalized screen units.
	// DaPerson supplies width .275 and height .05; the ship scales height .75.
	const qreal unit = size.height() * .5;
	const qreal height = .0375 * unit;
	const qreal width = .275 * unit;
	const qreal left = size.width() * .5;
	const qreal top = .01 * unit;
	const qreal iconLeft = left - height * 1.5;
	painter.save();
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	const qreal fraction = health.hits ? qBound(qreal(0), qreal(health.remaining) / qreal(health.hits), qreal(1)) : 0;
	const int color = fraction <= .1 ? 2 : fraction < .5 ? 1 : 0;
	if(fraction < 1)
		painter.drawImage(QRectF(left + width * fraction, top, width * (1 - fraction), height),
			images.unused[color == 2 ? 1 : 0], QRectF(.5 + 128 * fraction, .5, 128 * (1 - fraction), 9));
	if(fraction > 0)
		painter.drawImage(QRectF(left, top, width * fraction, height), images.health[color], QRectF(.5, .5, 128 * fraction, 9));
	if(health.system >= 0 && health.system < 16)
		painter.drawImage(QRectF(iconLeft, top, height, height), images.systems[health.system], QRectF(.5, .5, 15, 15));
	// The local designer's ship has no empire, so the target name is neutral.
	painter.setPen(QColor(192, 192, 192));
	QFont font = painter.font();
	font.setPixelSize(TargetFontSize);
	painter.setFont(font);
	const qreal textTop = top + height + .005 * unit;
	const qreal center = (iconLeft + left + width) * .5;
	const qreal textWidth = QFontMetricsF(painter.font()).boundingRect(name).width() + 4;
	painter.drawText(QRectF(center - textWidth * .5, textTop, textWidth,
		QFontMetricsF(painter.font()).height() + 2), Qt::AlignHCenter | Qt::AlignTop, name);
	painter.restore();
}

void PaintCrosshair(QPainter& painter, const QSize& size, int targetLevel, float opacity)
{
	if(size.width() <= 0 || size.height() <= 0) return;
	const HudImages& images = Images();
	const qreal unit = size.height() * .5;
	const qreal spread = .3 * (15 - qBound(0, targetLevel, 15)) / 15;
	painter.save();
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.setOpacity(qBound(0.f, opacity, 1.f));
	painter.translate(size.width() * .5, size.height() * .5);
	painter.drawImage(QRectF(-.006 * unit, -.006 * unit, .012 * unit, .012 * unit),
		images.dot, QRectF(.5, .5, 4, 4));
	for(int i = 0; i < 4; ++i)
	{
		painter.drawImage(QRectF((spread + .005) * unit, -.005 * unit, .095 * unit, .01 * unit),
			images.hair, QRectF(.5, .5, 47, 4));
		painter.rotate(90);
	}
	painter.restore();
}
}
}
