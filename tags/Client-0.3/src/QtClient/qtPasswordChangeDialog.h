#ifndef QTPASSWORDCHANGEIDALOG_H
#define QTPASSWORDCHANGEIDALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtGui/QShowEvent>
#include "ui_qtPasswordChangeDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_filter.h"
#include "mc_types.h"

class QtPasswordChangeDialog : public QDialog
{
	Q_OBJECT

public:
	QtPasswordChangeDialog(QWidget *parent);
	~QtPasswordChangeDialog();

public slots:
	void accept();
	
private slots:
	void authed(int);
	void replyReceived(int);

protected:
	void showEvent(QShowEvent *event);

private:

	Ui::QtPasswordChangeDialog ui;
	QtNetworkPerformer *performer;
	mc_buf netibuf,netobuf;
	mc_status s;
	int64 authtime;

};

#endif // QTPASSWORDCHANGEIDALOG_H
