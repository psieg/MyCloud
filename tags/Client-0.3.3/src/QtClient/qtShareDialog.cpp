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
	list<mc_user> l;
	list<mc_share> sharelist;

	disconnect(performer,SIGNAL(finished(int)),this,SLOT(userListReceived(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_listusers_process(netobuf,&l);
	if(rc){
		reject();
		return;
	}

	rc = db_list_share_sid(&sharelist,share.sid);
	if(rc){
		reject();
		return;
	}

	if(l.size() == 1){ //only one user on the server
		QMessageBox b(this);
		b.setText(tr("No users"));
		b.setInformativeText(tr("There are no other users on the server to whom you could share.\nCreate more users first."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();

		reject();
		return;
	}
	
	for(mc_user& u : l){
		if(u.id != myUID){
			bool sharedalready = false;
			for(mc_share& s : sharelist){
				if(s.uid == u.id){
					sharedalready = true;
					break;
				}
			}
			if(!sharedalready){
				ui.userBox->addItem(user,u.name.c_str());
				userlist.push_back(u);
			}
		}
	}

	if(userlist.size() == 0){
		QMessageBox b(this);
		b.setText(tr("No other users"));
		b.setInformativeText(tr("There seems to be no user left you could share to."));
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
	
	connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	rc = srv_putshare_async(netibuf,netobuf,performer,&share);
	if(rc){
		reject();
		return;
	}
	
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