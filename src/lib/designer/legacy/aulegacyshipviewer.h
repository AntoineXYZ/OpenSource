#ifndef AuLegacyShipViewer_H
#define AuLegacyShipViewer_H

#include <QMainWindow>
#include <QString>
#include <functional>

class QCloseEvent;

// Offline inspection of the recovered 2016 authoring format. Opening a design
// here does not modify the file or convert it to the modern mesh design model.
class AuLegacyShipViewer : public QMainWindow
{
public:
	explicit AuLegacyShipViewer(QWidget* parent = nullptr);
	~AuLegacyShipViewer() override;

	bool OpenFile(const QString& fileName);
	void ChooseFile();
	void SetExitHandler(const std::function<void()>& handler);
	bool ConfirmDiscardChanges();

protected:
	void showEvent(QShowEvent* event) override;
	void closeEvent(QCloseEvent* event) override;

private:
	class Impl;
	Impl* m_Impl;
};

#endif // AuLegacyShipViewer_H
