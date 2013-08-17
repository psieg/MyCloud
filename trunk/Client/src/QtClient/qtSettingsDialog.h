#ifndef QTSETTINGSDIALOG_H
#define QTSETTINGSDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include "ui_qtSettingsDialog.h"
#include "qtPasswordChangeDialog.h"
#include "mc.h" //wether MC_WATCHMODE is defined
#include "mc_db.h"


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

private:
	Ui::QtSettingsDialog ui;
	mc_status s;
	bool passchanged;
};

#endif // QTSETTINGSDIALOG_H
