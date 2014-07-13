#include "qtPasswordChangeDialog.h"

QtPasswordChangeDialog::QtPasswordChangeDialog(QWidget *parent)
	: QDialog(parent)
{
	int rc;
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
	ui.statusLabel->setVisible(false);

	rc = db_select_status(&s);
	if(!rc){
		ui.urlLabel->setText(s.url.c_str());
		ui.nameLabel->setText(s.uname.c_str());
	}
}

QtPasswordChangeDialog::~QtPasswordChangeDialog()
{
}

void QtPasswordChangeDialog::showEvent(QShowEvent *event){
	/*connect(performer,SIGNAL(finished(int)),this,SLOT(filterListReceived(int)));
	srv_listfilters_async(netibuf,netobuf,performer,0);*/
}

void QtPasswordChangeDialog::authed(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	if(rc){
		reject();
		return;
	}

	rc = srv_auth_process(&netobuf,&authtime);
	if(rc){
		if(rc == MC_ERR_LOGIN){
			QMessageBox b(this);
			b.setText(tr("Old login rejected"));
			b.setInformativeText(tr("The server did not accept your old password."));
			b.setStandardButtons(QMessageBox::Ok);
			b.setDefaultButton(QMessageBox::Ok);
			b.setIcon(QMessageBox::Warning);
			b.exec();
			//no reject here
			ui.oldEdit->setEnabled(true);
			ui.new1Edit->setEnabled(true);
			ui.new2Edit->setEnabled(true);
			ui.statusLabel->setVisible(false);
			ui.okButton->setVisible(true);
			return;
		}
		if(rc == MC_ERR_TIMEDIFF){
			QMessageBox b(this);
			b.setText(tr("Time difference too high"));
			b.setInformativeText(tr("Syncronisation only works if the Client and Server clocks are synchronized.\nUse NTP (recommended) or set the time manually."));
			b.setStandardButtons(QMessageBox::Ok);
			b.setDefaultButton(QMessageBox::Ok);
			b.setIcon(QMessageBox::Warning);
			b.exec();
		}
		reject();
		return;
	}
	
	ui.statusLabel->setText(tr("<i>sending request...</i>"));
	connect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	srv_passchange_async(&netibuf,&netobuf,performer,qPrintable(ui.new1Edit->text()));
}
void QtPasswordChangeDialog::replyReceived(int rc){
	disconnect(performer,SIGNAL(finished(int)),this,SLOT(replyReceived(int)));
	if(rc) {
		reject();
		return;
	}

	rc = srv_passchange_process(&netobuf);
	if(rc){
		reject();
		return;
	}

	rc = db_select_status(&s);
	if(rc){
		reject();
		return;
	}

	s.passwd = qPrintable(ui.new1Edit->text());

	rc = db_update_status(&s);
	if(rc){
		reject();
		return;
	}
	
	QMessageBox b(this);
	b.setText(tr("Password change successful"));
	b.setInformativeText(tr("The new password has been saved."));
	b.setStandardButtons(QMessageBox::Ok);
	b.setDefaultButton(QMessageBox::Ok);
	b.setIcon(QMessageBox::Information);
	b.exec();
	QDialog::accept();
}

void QtPasswordChangeDialog::accept(){
	if(ui.new1Edit->text() != ui.new2Edit->text()){
		QMessageBox b(this);
		b.setText(tr("Passwords don't match"));
		b.setInformativeText(tr("Make sure the new passwords match."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Critical);
		b.exec();
		return;
	}
	if(ui.new1Edit->text().length() == 0){
		QMessageBox b(this);
		b.setText(tr("Password can't be empty"));
		b.setInformativeText(tr("Please choss a secure password."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Critical);
		b.exec();
		return;
	}
	if(ui.new1Edit->text().length() < 7){
		QMessageBox b(this);
		b.setText(tr("Insecure Password"));
		b.setInformativeText(tr("Short passwords are insecure! Hit cancel to abort."));
		b.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		if(b.exec() != QMessageBox::Ok)
			return;
	}


	QString _url = "https://";
	_url.append(s.url.c_str());
	_url.append("/bin.php");
	performer = new QtNetworkPerformer(_url,"trustCA.crt",s.acceptallcerts,true);
	SetBuf(&netibuf);
	SetBuf(&netobuf);
		
	connect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	srv_auth_async(&netibuf,&netobuf,performer,s.uname,qPrintable(ui.oldEdit->text()),&authtime);
	ui.oldEdit->setEnabled(false);
	ui.new1Edit->setEnabled(false);
	ui.new2Edit->setEnabled(false);
	ui.okButton->setVisible(false);
	ui.statusLabel->setVisible(true);
	ui.statusLabel->setText(tr("<i>authenticating...</i>"));
	//QDialog::accept();
}
