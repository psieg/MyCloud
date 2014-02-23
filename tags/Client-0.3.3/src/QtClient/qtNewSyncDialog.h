#ifndef QTNEWSYNCDIALOG_H
#define QTNEWSYNCDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtGui/QShowEvent>
#include "ui_qtNewSyncDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_types.h"
#include "mc_crypt.h"

class QtNewSyncDialog : public QDialog
{
	Q_OBJECT

public:
	QtNewSyncDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf);
	~QtNewSyncDialog();

public slots:
	void accept();
	
private slots:
	void userReceived(int);
	void replyReceived(int);
	void keyringReceived(int);
	void keyringSent(int);

protected:
	void showEvent(QShowEvent *event);

private:
	Ui::QtNewSyncDialog ui;
	QWidget *myparent;
	QtNetworkPerformer *performer;
	mc_buf *netibuf,*netobuf;
	mc_sync sync;
	list<mc_keyringentry> keyring;
	QString keyringpass;
	mc_user self;

};

#endif // QTNEWSYNCDIALOG_H
