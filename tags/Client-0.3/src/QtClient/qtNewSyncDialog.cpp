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
		QMessageBox b(this);
		b.setText(tr("No name set"));
		b.setInformativeText(tr("Please choose a name for the new Sync."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
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

	QDialog::accept();
}
