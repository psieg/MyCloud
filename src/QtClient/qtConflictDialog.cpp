#include "qtConflictDialog.h"

QtConflictDialog::QtConflictDialog(QWidget *parent)
	: QDialog(parent)
{
	quit = false;
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	quittimer.setSingleShot(false);
	quittimer.setInterval(2000);
	connect(&quittimer, SIGNAL(timeout()), this, SLOT(checkquit()));
	skiptimer.setSingleShot(false);
	skiptimer.setInterval(1000);
	connect(&skiptimer, SIGNAL(timeout()), this, SLOT(checkskip()));
}

QtConflictDialog::~QtConflictDialog()
{

}

int QtConflictDialog::exec(std::string *fullPath, std::string *descLocal, std::string *descServer, int defaultValue, bool manualSolvePossible)
{
	quittimer.start();
	skiptimer.start();
	skiptime = 60;
	setMouseTracking(true);
	setWindowTitle(tr("Conflict: ") + fullPath->c_str());
	ui.downloadButton->setDescription(descServer->c_str());
	ui.uploadButton->setDescription(descLocal->c_str());
	ui.keepButton->setEnabled(manualSolvePossible);
	ui.dirBox->setChecked(false);
	ui.recursiveBox->setChecked(false);
	ui.recursiveBox->setEnabled(false);
	switch(defaultValue) {
		case 1:
			ui.uploadButton->setDefault(true);
			ui.uploadButton->setFocus();
			break;
		case 2:
			ui.downloadButton->setDefault(true);
			ui.downloadButton->setFocus();
			break;
		default:
			if (manualSolvePossible) {
				ui.keepButton->setDefault(true);
				ui.keepButton->setFocus();
			} else {
				ui.skipButton->setDefault(true);
				ui.skipButton->setFocus();
			}
	}
	//this->setFocus(Qt::FocusReason::ActiveWindowFocusReason);
	return QDialog::exec();
	skiptimer.stop();
	quittimer.stop();
}

void QtConflictDialog::forceSetFocus() {
	//this->setFocus();
	this->activateWindow();
	this->raise();
	skiptimer.stop();
	ui.skipButton->setText(tr("skip this time"));
}

void QtConflictDialog::closeEvent(QCloseEvent *event)
{
	//event->ignore();
	done(Skip);
}

void QtConflictDialog::keyPressEvent(QKeyEvent *e) {
	// Unfortunately does not fire when using the arrow keys to shift focus
	if (skiptimer.isActive()) {
		skiptimer.stop();
		ui.skipButton->setText(tr("skip this time"));
	}

	QDialog::keyPressEvent(e);
}

void QtConflictDialog::mouseMoveEvent(QMouseEvent *e) {
	if (skiptimer.isActive()) {
		skiptimer.stop();
		ui.skipButton->setText(tr("skip this time"));
	}

	QDialog::mouseMoveEvent(e);

}

void QtConflictDialog::reject() {
	done(Skip);
}

void QtConflictDialog::checkquit() {
	if (quit) {
		done(Terminating);
		qApp->quit();
	}
	if (MC_TERMINATING()) {
		done(Terminating);
	}
}

void QtConflictDialog::checkskip() {
	skiptime -= 1;
	if (skiptime <= 0) {
		done(Skip);
	}
	ui.skipButton->setText(tr("skip this time") + " (" + QVariant(skiptime).toString() + ")");
}

void QtConflictDialog::on_downloadButton_clicked()
{
	if (ui.dirBox->isChecked())
		if (ui.recursiveBox->isChecked()) done(DownloadDR);
		else done(DownloadD);
	else 
		done(Download);
}

void QtConflictDialog::on_uploadButton_clicked()
{
	if (ui.dirBox->isChecked())
		if (ui.recursiveBox->isChecked()) done(UploadDR);
		else done(UploadD);
	else 
		done(Upload);
}

void QtConflictDialog::on_keepButton_clicked()
{
	if (ui.dirBox->isChecked()) done(KeepD);
	else done(Keep);
	
}
void QtConflictDialog::on_skipButton_clicked()
{
	if (ui.dirBox->isChecked()) done(SkipD);
	else done(Skip);
}