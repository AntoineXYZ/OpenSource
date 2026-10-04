#ifndef AuLegacyShipRender_H
#define AuLegacyShipRender_H

#include <QOpenGLWidget>
#include <QStringList>

namespace AuLegacyShip { struct Design; }
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

class AuLegacyShipRender : public QOpenGLWidget
{
public:
	explicit AuLegacyShipRender(QWidget* parent = nullptr);
	~AuLegacyShipRender() override;

	void SetDesign(const AuLegacyShip::Design& design);
	void SetDeck(int deck);
	void SetCutaway(bool enabled);
	void FitDesign();
	QStringList Warnings() const;

protected:
	void initializeGL() override;
	void resizeGL(int width, int height) override;
	void paintGL() override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	void CleanupGL();
	class Impl;
	Impl* m_Impl;
};

#endif // AuLegacyShipRender_H
