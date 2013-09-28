#include "qtShareDialog.h"

QtShareDialog::QtShareDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, int syncID, int myUID)
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
	this->myUID = myUID;
	user = QIcon(":/Resources/user.png");
	ui.sendLabel->setVisible(false);
}

QtShareDialog::~QtShareDialog()
{
}

void QtShareDialog::showEvent(QShowEvent *event){	
	connect(performer,SIGNAL(finished(int)),this,SLOT(userListReceived(int)));
	srv_listusers_async(netibuf,netobuf,performer);
	QDialog::showEvent(event);
}

void QtShareDialog::userListReceived(int rc){
	list<mc_user> list;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(userListReceived(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_listusers_process(netobuf,&list);
	if(rc){
		reject();
		return;
	}
	
	for(mc_user& u : list){
		if(u.id != myUID){
			ui.userBox->addItem(user,u.name.c_str());
			userlist.push_back(u);
		}
	}

	if(userlist.size() == 0){
		QMessageBox b(this);
		b.setText(tr("No other users"));
		b.setInformativeText(tr("There seems to be no other user you could share to."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();

		reject();
		return;
	}
	ui.fetchUserLabel->setVisible(false);
	ui.userBox->setEnabled(true);
	ui.okButton->setEnabled(true);
}

void QtShareDialog::accept(){
	int rc;
	
	share.uid = userlist[ui.userBox->currentIndex()].id;
	share.uname = userlist[ui.userBox->currentIndex()].name;
	
	connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	srv_putshare_async(netibuf,netobuf,performer,&share);
	
	ui.okButton->setVisible(false);
	ui.sendLabel->setVisible(true);

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