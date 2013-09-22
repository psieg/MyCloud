#include "qtShareDialog.h"

QtShareDialog::QtShareDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, int syncID)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	share.sid = syncID;
	ui.sendLabel->setVisible(false);
}

QtShareDialog::~QtShareDialog()
{
}

void QtShareDialog::showEvent(QShowEvent *event){
	
	QDialog::showEvent(event);
}

void QtShareDialog::accept(){
	int rc;
	
	// share.uid = 

	/*
	
	connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	srv_putshare_async(netibuf,netobuf,performer,&share);
	
	ui.okButton->setVisible(false);
	ui.sendLabel->setVisible(true);
	*/
	QDialog::accept();
	//replyReceived does the accept
}

void QtShareDialog::replyReceived(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_putshare_process(netobuf);
	if(rc){
		reject();
		return;
	}

	rc = db_insert_share(&share);
	if(rc){
		reject();
		return;
	}

	//if successful, the server will have increased the Shareversion by one
	sync.id = share.sid;
	rc = db_select_sync(&sync);
	if(rc){
		reject();
		return;
	}
	sync.shareversion += 1;
	rc = db_update_sync(&sync);
	if(rc){
		reject();
		return;
	}

	QDialog::accept();
}