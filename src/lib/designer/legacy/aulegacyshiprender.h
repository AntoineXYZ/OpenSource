#ifndef AuLegacyShipRender_H
#define AuLegacyShipRender_H

#include "audesignerview.h"
#include <QStringList>
#include <functional>

namespace AuLegacyShip { struct Design; }
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QFocusEvent;
class QHideEvent;
class QShowEvent;

class AuLegacyShipRender : public AuDesignerView
{
public:
	explicit AuLegacyShipRender(QWidget* parent = nullptr);
	~AuLegacyShipRender() override;

	void SetDesign(const AuLegacyShip::Design& design);
	void SetDeck(int deck);
	void SetGravity(bool enabled);
	void ResetCharacter();
	void SetFloorPlan(QWidget* floorPlan);
	void SetWalkthrough(bool enabled);
	void ControlsChanged();
	QStringList Warnings() const;
	std::function<void(const QString&)> tipsChanged;

protected:
	void initializeGL() override;
	void resizeGL(int width, int height) override;
	void resizeEvent(QResizeEvent* event) override;
	bool event(QEvent* event) override;
	void paintGL() override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;

private:
	void CleanupGL();
	void PrepareSky();
	void RenderSky();
	void Tick();
	float FieldOfView() const;
	void UpdateTarget(qint64 now);
	void UpdateMouseCapture();
	void ReleaseMouseCapture();
	void CenterMouse();
	void UseDoor(const QPoint& screenPosition);
	bool UseConsole(const QPoint& screenPosition);
	void OperateDoor(quint32 id, bool open);
	void OperateSystem(int action);
	class Impl;
	Impl* m_Impl;
};

#endif // AuLegacyShipRender_H
