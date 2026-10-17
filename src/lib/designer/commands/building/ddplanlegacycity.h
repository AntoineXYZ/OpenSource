#ifndef DdPlanLegacyCity_H
#define DdPlanLegacyCity_H

#include "ddcommand.h"
#include "aulegacycityprotocol.h"
#include "auglobell.h"
#include "auglsl.h"
#include <QPointer>

class AuLegacyCityConstruction;
class DaWorldLL;

// Reuses only the viewport's input/projection mechanism. Historical city
// placement and requests are independent of DdPlanBdBuilding/modern roads.
class DdPlanLegacyCity : public DdCommand
{
public:
	DdPlanLegacyCity(AuDesignerView* view, AuLegacyCityConstruction* controls,
		const AuLegacyCityProtocol::Request& request);
	~DdPlanLegacyCity() override;
	bool UsesControls(const AuLegacyCityConstruction* controls) const;
	void UpdateSelection(const AuLegacyCityProtocol::Request& request);
	bool OnKeyDown(QKeyEvent* event) override;
	void OnButtonDown(QMouseEvent* event) override;
	void OnMouseWheel(QWheelEvent* event) override;
	void DeleteDeviceObjects(AuGLEffects& effects) override;
	void SetCursor(const AuMatrix4x4& camera, DaPerson* person, const QPoint& pixel) override;
	void RenderCursor(const AuMatrix4x4& projection, const AuMatrix4x4& camera, DaPerson* person,
		const QPoint& pixel, AuAtomRenderInfo& info, bool snapped) override;
private:
	QPointer<AuLegacyCityConstruction> m_Interface;
	AuLegacyCityProtocol::Request m_Request;
	bool m_HasStart = false;
	quint32 m_StartWorld = 0;
	AuPoint m_Start;
	double m_Angle = 90;
	bool m_PreviewValid = false;
	QList<AuPointAlpha> m_Preview;
	AuGLVAO m_PreviewVAO;
	void ChangeMaterial(int direction);
	void ChangeLevels(int direction);
	void ChangeRotation(int direction, bool fine);
	void ConstrainRoad(const DaWorldLL& world, AuPoint& point, bool constrain) const;
};

#endif
