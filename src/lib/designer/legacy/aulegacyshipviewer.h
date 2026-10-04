#ifndef AuLegacyShipViewer_H
#define AuLegacyShipViewer_H

#include <QMainWindow>
#include <QString>

// Offline inspection of the recovered 2016 authoring format. Opening a design
// here does not modify the file or convert it to the modern mesh design model.
class AuLegacyShipViewer : public QMainWindow
{
public:
	explicit AuLegacyShipViewer(QWidget* parent = nullptr);
	~AuLegacyShipViewer() override;

	bool OpenFile(const QString& fileName);
	void ChooseFile();

protected:
	void showEvent(QShowEvent* event) override;

private:
	class Impl;
	Impl* m_Impl;
};

#endif // AuLegacyShipViewer_H
