#ifndef QTSHAREDIALOG_H
#define QTSHAREDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtGui/QShowEvent>
#include "ui_qtShareDialog.h"
#include "mc_db.h"
#include "mc_srv.h"
#include "mc_types.h"
#include "mc_crypt.h"

class QtShareDialog : public QDialog
{
	Q_OBJECT

public:
	QtShareDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, int syncID, int myUID);
	~QtShareDialog();

public slots:
	void accept();
	
private slots:
	void userListReceived(int);
	void replyReceived(int);

protected:
	void showEvent(QShowEvent *event);

private:
	Ui::QtShareDialog ui;
	QWidget *myparent;
	QtNetworkPerformer *performer;
	mc_buf *netibuf, *netobuf;
	mc_share share;
	mc_sync_db sync;
	QIcon user;
	std::vector<mc_user> userlist;
	int myUID;

};

#endif // QTSHAREDIALOG_H
