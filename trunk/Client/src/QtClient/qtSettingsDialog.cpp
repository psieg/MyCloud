#include "qtSettingsDialog.h"

qtSettingsDialog::qtSettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	int rc;

	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedSize(sizeHint());
	
	connect(ui.passEdit,SIGNAL(textEdited(const QString &)),this,SLOT(passchange()));
	connect(ui.certBox,SIGNAL(pressed()),this,SLOT(acceptActivate()));
	
	ui.versionLabel->setText(tr("MyCloud Client ")+MC_VERSION);
#ifdef MC_WATCHMODE
	ui.watchmodeBox->setEnabled(true);
#endif
	rc = db_select_status(&s);
	if(!rc){
		ui.urlEdit->setText(QString(s.url.c_str()));
		ui.nameEdit->setText(QString(s.uname.c_str()));
		if(s.passwd != "") ui.passEdit->setText(tr("*****"));
		passchanged = false;
		ui.certBox->setChecked(s.acceptallcerts);
		if(s.watchmode > 0){
			ui.watchmodeBox->setChecked(true);
			ui.sleeptimeSpin->setValue(s.watchmode);
		} else {
			ui.watchmodeBox->setChecked(false);
			ui.sleeptimeSpin->setValue(-s.watchmode);
		}
		if(s.updatecheck >= 0) ui.updateBox->setChecked(true);
		else ui.updateBox->setChecked(false);
	}
}

qtSettingsDialog::~qtSettingsDialog()
{

}

void qtSettingsDialog::passchange(){
	passchanged = true;
}

void qtSettingsDialog::acceptActivate(){
	if(!ui.certBox->isChecked()){
		QMessageBox b(this);
		b.setText("UNSAFE: This option should be used only on safe networks");
		b.setInformativeText("You are about to disable Certificate Verification. This allows for easy man-in-the-middle attacks and is effectively as safe as no ecryption!\nONLY use this option on safe and trusted networks.\nIf you get SSL handshake errors, make sure you provided a trustCA.crt first.");
		QAbstractButton *yes = b.addButton("Yes, I know what I'm doing",QMessageBox::YesRole);
		b.setStandardButtons(QMessageBox::No);
		b.setDefaultButton(QMessageBox::No);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		if(b.clickedButton() == yes){
			ui.certBox->setChecked(true);
		}
	}
}

void qtSettingsDialog::accept(){
	bool restart = false;
	if(passchanged || ui.urlEdit->text() != s.url.c_str() || ui.nameEdit->text() != s.uname.c_str() 
		|| ui.watchmodeBox->isChecked() != (s.watchmode > 0) || abs(ui.sleeptimeSpin->value()) != abs(s.watchmode)){
		//"restart" required
		restart = true;
		//MC_INF("Important info changed, requires restart");
	}
	
	s.url = qPrintable(ui.urlEdit->text());
	s.uname = qPrintable(ui.nameEdit->text());
	if(passchanged) s.passwd = qPrintable(ui.passEdit->text());
	s.acceptallcerts = ui.certBox->isChecked();
	if(ui.watchmodeBox->isChecked()) 
		s.watchmode = ui.sleeptimeSpin->value();
	else
		s.watchmode = -ui.sleeptimeSpin->value();
	if(ui.updateBox->isChecked())
		s.updatecheck = (s.updatecheck>=0?s.updatecheck:0);
	else
		s.updatecheck = -1;

	db_update_status(&s);

	if(restart){
		done(NeedRestart);
	} else {
		done(Accepted);
	}

	//QDialog::accept();
}