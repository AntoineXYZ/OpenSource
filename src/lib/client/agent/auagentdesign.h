//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auagentdesign.h,v 1.9 2022/02/19 17:49:08 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuAgentDesign_H
#define AuAgentDesign_H

#include "auclientlib.h"
#include "audialog.h"
#include "auid.h"
#include <QByteArray>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

class AUCLIENT_DLL AuAgentDesign : public QWidget
{
	Q_OBJECT;
private:
	// Widgets
	QLineEdit* m_LegacyFileName;
	QLineEdit* m_LegacyName;
	QComboBox* m_LegacyDesigns;
	QPushButton* m_LegacyBrowse;
	QPushButton* m_LegacyUpload;
	QPushButton* m_LegacyRefresh;
	QPushButton* m_LegacySpawn;
	QLabel* m_LegacyStatus;
	QTimer* m_LegacyTimeout;
	QString m_LegacyFile;
	bool m_LegacyAllowed = false;
	quint32 m_LegacyUploadToken = 0;
	quint32 m_LegacyCatalogToken = 0;
	quint32 m_LegacySpawnToken = 0;
	quint32 m_LegacyAfterId = 0;
	quint32 m_LegacyWantedId = 0;

public:
	AuAgentDesign(QWidget* parent=0);
	virtual ~AuAgentDesign();

public slots:
	virtual void setVisible(bool visible);

protected slots:
	void EnterDesigner();
	void LoadBdDesign();
	void LoadScDesign();
	void BrowseLegacyScDesign();
	void UploadLegacyScDesign();
	void RefreshLegacyScDesigns();
	void SpawnLegacyScDesign();
	void LegacyScReply(QByteArray payload);
	void LegacyScDisconnected();
	void LegacyScTimedOut();
	void UpdateLegacyControls();
	void LegacyScSelectionChanged();
};

#endif // AuAgentDesign_H
