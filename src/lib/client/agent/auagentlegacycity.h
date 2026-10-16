#ifndef AuAgentLegacyCity_H
#define AuAgentLegacyCity_H

#include "auclientlib.h"
#include "aulegacycityprotocol.h"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

class AUCLIENT_DLL AuAgentLegacyCity : public QWidget
{
	Q_OBJECT
public:
	explicit AuAgentLegacyCity(QWidget* parent = nullptr);
	void setVisible(bool visible) override;
private slots:
	void Refresh();
	void BlueprintChanged();
	void BuildingChanged();
	void UpdateControls();
	void ReceiveReply(QByteArray payload);
	void ResetTransport();
private:
	QComboBox *m_Blueprints, *m_Materials, *m_Buildings;
	QSpinBox *m_MinimumDeck, *m_MaximumDeck;
	QDoubleSpinBox *m_Distance, *m_Heading, *m_Elevation, *m_TargetX, *m_TargetY, *m_TargetZ;
	QCheckBox* m_Constructed;
	QPushButton *m_Spawn, *m_Refresh, *m_Apply, *m_Remove, *m_Route;
	QLabel *m_Status, *m_DesignInfo;
	QTimer* m_Timeout;
	quint32 m_PendingToken = 0;
	QVector<AuLegacyCityProtocol::Entry> m_Entries;
	bool LoadBlueprints();
	void Send(AuLegacyCityProtocol::Action action);
};
#endif
