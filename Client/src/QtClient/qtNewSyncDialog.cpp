#include "QtNewSyncDialog.h"

QtNewSyncDialog::QtNewSyncDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	ui.sendLabel->setVisible(false);
}

QtNewSyncDialog::~QtNewSyncDialog()
{
}

void QtNewSyncDialog::showEvent(QShowEvent *event){
	QDialog::showEvent(event);
}

void QtNewSyncDialog::accept(){
	int rc;
	if(ui.nameEdit->text().length() == 0){
		QMessageBox::warning(this, tr("No name set"), tr("Please choose a name for the new Sync."), QMessageBox::Ok);
		return;
	}
	
	sync.id = MC_SYNCID_NONE;
	sync.crypted = ui.encryptedBox->isChecked();
	sync.filterversion = 0;
	sync.name = qPrintable(ui.nameEdit->text());
	memset(sync.hash,0,16);
	

	connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	srv_createsync_async(netibuf,netobuf,performer,sync.name,sync.crypted);

	
	ui.nameEdit->setEnabled(false);
	ui.encryptedBox->setEnabled(false);
	ui.okButton->setVisible(false);
	ui.sendLabel->setVisible(true);

	//replyReceived does the accept
}

void QtNewSyncDialog::replyReceived(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_createsync_process(netobuf,&sync.id);
	if(rc){
		reject();
		return;
	}

	// donwload keyring
	if(sync.crypted){
		ui.sendLabel->setText(tr("<i>downloading keyring...</i>"));
		connect(performer,SIGNAL(finished(int)),this,SLOT(keyringReceived(int)));
		srv_getkeyring_async(netibuf,netobuf,performer);
	} else {
		QDialog::accept();
	}
}

void QtNewSyncDialog::keyringReceived(int rc){
	string keyringdata;
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(keyringReceived(int)));
	if(rc) {
		reject();
		return;
	}

	rc = srv_getkeyring_process(netobuf,&keyringdata);
	if(rc){
		reject();
		return;
	}

	if(keyringdata.length() > 0){ // don't ask for a password when there is no ring...

		bool ok = false;
		while(!ok){
			QString pass = "";
			while(!ok){
				pass = QInputDialog::getText(this, tr("Keyring Password"), tr("Please enter the password to your keyring"), QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
			}

			// decrypt
			rc = crypt_keyring_fromsrv(keyringdata,pass.toStdString(),&keyring);
			if(rc){
				QMessageBox::critical(this, tr("Keyring Decryption failed"), tr("The keyring could not be decrypted! Re-check your password or enter the key manually."), QMessageBox::Ok);
				ui.okButton->setEnabled(true);
				ok = false;
			}
			keyringpass = pass;
		}
	}

	// generate new key
	ui.sendLabel->setText(tr("<i>generating key...</i>"));

	QByteArray newkey = QByteArray(32,'\0');
	while(crypt_randkey((unsigned char*)newkey.data())){
		QMessageBox::warning(this, tr("Can't generate keys atm"), tr("Please do something else so the system can collect entropy"), QMessageBox::Ok);
		ui.okButton->setEnabled(true);
		return;
	} 

		int result = 0;

		// TODO: add to keyring and send to server
		bool found = false;
		for(mc_keyringentry& entry : keyring){
			if(entry.sid == sync.id){
				entry.sname = sync.name;
				memcpy(entry.key,newkey.constData(),newkey.size());
				found = true;
			}
		}
		if(!found){
			mc_keyringentry newentry;
			newentry.sid = sync.id;
			newentry.sname = sync.name;
			memcpy(newentry.key,newkey.constData(),newkey.size());
			keyring.push_back(newentry);
		}

			
		bool ok = false;
		while(keyringpass == ""){
			QString pass, confirm;
			pass = QInputDialog::getText(this, tr("Keyring Password"), tr("Please choose a password for your keyring.\nIt is used to encrypt the keyring and should not be as secure as possible, especially not related to your account password!\nMake sure you do not forget it!"), QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
			if(ok) confirm = QInputDialog::getText(this, tr("Keyring Password"), tr("Please confirm your keyring password"), QLineEdit::Password, NULL, &ok, windowFlags() & ~Qt::WindowContextHelpButtonHint);
			if(ok && pass != confirm){
				QMessageBox::warning(this, tr("Password Mismatch"), tr("The passwords didn't match. Try again"), QMessageBox::Ok);
				ok = false;
			}
			if(ok && pass.length() < 10){
				QMessageBox::warning(this, tr("Insecure Password"), tr("This is the key to the keys to all your files!\nI can't force you to use a secure password, but..."), QMessageBox::Ok);
				ok = false;
			}
			if(ok)
				keyringpass = pass;
		}

		rc = crypt_keyring_tosrv(&keyring,keyringpass.toStdString(),&keyringdata);
		if(rc){
			reject();
			return;
		}

		ui.sendLabel->setText(tr("<i>uploading keyring...</i>"));
		connect(performer,SIGNAL(finished(int)),this,SLOT(keyringSent(int)));
		srv_setkeyring_async(netibuf,netobuf,performer,&keyringdata);

	return;
	
}

void QtNewSyncDialog::keyringSent(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(keyringSent(int)));
	
	if(rc) {
		reject();
		return;
	}

	rc = srv_setkeyring_process(netobuf);
	if(rc){
		reject();
		return;
	}
	
	QDialog::accept();
}