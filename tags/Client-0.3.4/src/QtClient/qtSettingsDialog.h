#ifndef QTSETTINGSDIALOG_H
#define QTSETTINGSDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include "ui_qtSettingsDialog.h"
#include "qtPasswordChangeDialog.h"
#include "mc.h" //wether MC_WATCHMODE is defined
#include "mc_db.h"
#include "mc_types.h"
#ifdef MC_OS_WIN
#include <QtCore/QSettings>
#include <QtCore/QDir>
#define MC_AUTOSTARTNAME	"MyCloud Client"
#define MC_STARTUP_KEY		"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#endif



class QtSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	QtSettingsDialog(QWidget *parent = 0);
	~QtSettingsDialog();

	enum DialogCode {
		Rejected = 0, 
		Accepted = 1, 
		NeedRestart = 2
	};

public slots:
	void accept();

private slots:
	void passchange();
	void acceptActivate();

	void on_passwordButton_clicked();
#ifdef MC_OS_WIN
	void on_startupBox_clicked(bool);
#endif

private:

	Ui::QtSettingsDialog ui;
	mc_status s;
	bool passchanged;
};

#endif // QTSETTINGSDIALOG_H
