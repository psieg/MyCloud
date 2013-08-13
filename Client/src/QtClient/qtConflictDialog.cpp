#include "qtConflictDialog.h"

QtConflictDialog::QtConflictDialog(QWidget *parent)
	: QDialog(parent)
{
	quit = false;
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	quittimer.setSingleShot(false);
	quittimer.setInterval(2000);
	quittimer.start();
	connect(&quittimer,SIGNAL(timeout()),this,SLOT(checkquit()));
}

QtConflictDialog::~QtConflictDialog()
{

}

int QtConflictDialog::exec(std::string *fullPath, std::string *descLocal, std::string *descServer, int defaultValue, bool manualSolvePossible)
{
	setWindowTitle(tr("Conflict: ") + fullPath->c_str());
	ui.downloadButton->setDescription(descServer->c_str());
	ui.uploadButton->setDescription(descLocal->c_str());
	ui.keepButton->setEnabled(manualSolvePossible);
	ui.dirBox->setChecked(false);
	ui.recursiveBox->setChecked(false);
	ui.recursiveBox->setEnabled(false);
	switch(defaultValue){
		case 1:
			ui.uploadButton->setDefault(true);
			ui.uploadButton->setFocus();
			break;
		case 2:
			ui.downloadButton->setDefault(true);
			ui.downloadButton->setFocus();
			break;
		default:
			if(manualSolvePossible){
				ui.keepButton->setDefault(true);
				ui.keepButton->setFocus();
			} else {
				ui.skipButton->setDefault(true);
				ui.skipButton->setFocus();
			}
	}
	return QDialog::exec();
}

void QtConflictDialog::closeEvent(QCloseEvent *event)
{
	//event->ignore();
	done(Skip);
}

void QtConflictDialog::keyPressEvent(QKeyEvent *e) {
    //if(e->key() != Qt::Key_Escape) //No one may close or ignore us, this choice needs to be made
        QDialog::keyPressEvent(e);

}

void QtConflictDialog::reject(){
	done(Skip);
}

void QtConflictDialog::checkquit(){
	if(quit){
		done(Terminating);
		qApp->quit();
	}
	if(MC_TERMINATING()){
		done(Terminating);
	}
}

void QtConflictDialog::on_downloadButton_clicked()
{
	if(ui.dirBox->isChecked())
		if(ui.recursiveBox->isChecked()) done(DownloadDR);
		else done(DownloadD);
	else 
		done(Download);
}

void QtConflictDialog::on_uploadButton_clicked()
{
	if(ui.dirBox->isChecked())
		if(ui.recursiveBox->isChecked()) done(UploadDR);
		else done(UploadD);
	else 
		done(Upload);
}

void QtConflictDialog::on_keepButton_clicked()
{
	if(ui.dirBox->isChecked()) done(KeepD);
	else done(Keep);
	
}
void QtConflictDialog::on_skipButton_clicked()
{
	if(ui.dirBox->isChecked()) done(SkipD);
	else done(Skip);
}