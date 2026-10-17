//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aubldconstruct.h,v 1.7 2022/02/19 17:51:37 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuBldConstruct_H
#define AuBldConstruct_H

#include "auclientlib.h"
#include "augl.h"
#include "auglobell.h"
#include "auid.h"
#include "dbindustry.h"
#include <QWidget>

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTimer;
class DeConstructionProcess;

class AUCLIENT_DLL AuBldConstruct : public QWidget
{
	Q_OBJECT;

private:
	AuId								m_CapitolOpId;			// Id of building operated when person is in a capitol building.
	INDUSTRYID							m_CapitolOpIndustry;	// Industry of building operated when person is in a capitol.
	AuGlobeLLF							m_CapitolOpLL;			// LL for geo roads
	AuId								m_BdId;					// Id of DaBd the person is physically standing in.
	INDUSTRYID							m_BdIndustry;			// Bd industry the person is physically standing in.
	AuGlobeLLF							m_BdLL;					// LL for geo roads
	bool								m_CitizenOk;			// is citizen worker enabled?
	QLabel*								m_Construct;
	bool								m_DoWork;				// true while auto-working
	QCheckBox*							m_Employ;
	QLabel*								m_Labor;
	QListWidget*						m_Materials;
	QTableWidget*						m_Modifiers;
	QTimer*								m_timer;
	QPushButton*						m_Work;
	bool m_LegacyMode = false, m_LegacyAvailable = false, m_LegacyHalted = false, m_LegacyPending = false;
	void ShowProcessComponents(const DeConstructionProcess& process);

public:
	AuBldConstruct(QWidget* parent=0, bool legacy=false);
	virtual ~AuBldConstruct();
	// Reuse common F10 labor controls with a city-owned historical target.
	// Default construction continues to operate modern Bd targets unchanged.
	void SetLegacyTarget(const QString& name, const DeConstructionProcess& process,
		bool available, bool citizenEnabled, bool halted);
	void SetLegacyRequestPending(bool pending);
	void StopWorking();
	bool IsWorking() const { return m_DoWork; }

signals:
	void LegacyUpdateRequested(bool working);
	void LegacyActionRequested(quint8 action, bool enabled);

public:

	// Clear information needed when this building is operated from a capitol building.
	void ClearCapitolOp()
	{
		m_CapitolOpId = AuId(0);
	}

	// Set information needed when this building is operated from a capitol building.
	void SetCapitolOp(const AuId& capitolId, INDUSTRYID capitolIndustry, const AuGlobeLLF& capitolLL)
	{
		m_CapitolOpId = capitolId;
		m_CapitolOpIndustry = capitolIndustry;
		m_CapitolOpLL = capitolLL;
	}

protected:
	virtual void showEvent(QShowEvent* event);

protected slots:
	void CitizenEnableChanged(int state);
	void FetchPushed();
	void GoPushed();
	void UpdateContents();
};

#endif // AuBldConstruct_H
