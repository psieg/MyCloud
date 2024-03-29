#include "qtFilterDialog.h"

QtFilterDialog::QtFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, int syncID, int editID)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowFlags(windowFlags() | Qt::MSWindowsFixedSizeDialogHint);
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	filter.id = editID;
	filter.sid = syncID;
	ui.sendLabel->setVisible(false);
}

QtFilterDialog::~QtFilterDialog()
{
}

void QtFilterDialog::showEvent(QShowEvent *event) {
	int rc;

	if (filter.id != -1) {
		rc = db_select_filter(&filter);
		if (rc) return;
		ui.filesBox->setChecked(filter.files);
		ui.directoriesBox->setChecked(filter.directories);
		ui.typeBox->setCurrentIndex(typeToIndex(filter.type));
		ui.valueEdit->setText(filter.rule.c_str());
	}

	QDialog::showEvent(event);
}

void QtFilterDialog::accept() {
	mc_sync_ctx ctx;
	std::string cleanrule;
	int rc;
	if (!ui.filesBox->isChecked() && !ui.directoriesBox->isChecked()) {
		QMessageBox b(this);
		b.setText(tr("No match type set"));
		b.setInformativeText(tr("Please choose wether the filter should match files, directories or both."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		return;
	}
	if (ui.valueEdit->text().length() == 0) {
		QMessageBox b(this);
		b.setText(tr("No value set"));
		b.setInformativeText(tr("Please set a value to match against."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		return;
	}
	filter.files = ui.filesBox->isChecked();
	filter.directories = ui.directoriesBox->isChecked();
	filter.type = indexToType(ui.typeBox->currentIndex());
	filter.rule = qPrintable(ui.valueEdit->text());


	sync.id = filter.sid;
	rc = db_select_sync(&sync);
	if (rc) {
		reject();
		return;
	}
	init_sync_ctx(&ctx, &sync, NULL);
	cleanrule = filter.rule;
	rc = crypt_filter_tosrv(&ctx, sync.name, &filter);
	if (rc) {
		reject();
		return;
	}

	connect(performer, SIGNAL(finished(int)), this, SLOT(replyReceived(int)));
	srv_putfilter_async(netibuf, netobuf, performer, &filter);

	filter.rule = cleanrule;

	ui.filesBox->setEnabled(false);
	ui.directoriesBox->setEnabled(false);
	ui.typeBox->setEnabled(false);
	ui.valueEdit->setEnabled(false);
	ui.browseButton->setEnabled(false);
	ui.okButton->setVisible(false);
	ui.sendLabel->setVisible(true);

	//replyReceived does the accept
}

void QtFilterDialog::replyReceived(int rc) {
	int id = filter.id;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(replyReceived(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_putfilter_process(netobuf, &filter.id);
	if (rc) {
		reject();
		return;
	}

	if (id == -1) rc = db_insert_filter(&filter);
	else rc = db_update_filter(&filter);
	if (rc) {
		reject();
		return;
	}

	//if successful, the server will have increased the filterversion by one
	sync.id = filter.sid;
	rc = db_select_sync(&sync);
	if (rc) {
		reject();
		return;
	}
	sync.filterversion += 1;
	rc = db_update_sync(&sync);
	if (rc) {
		reject();
		return;
	}

	QDialog::accept();
}


void QtFilterDialog::on_typeBox_currentIndexChanged(int index) {
	ui.browseButton->setEnabled(indexToType(index) == MC_FILTERT_MATCH_PATH);
}

void QtFilterDialog::on_browseButton_clicked() {
	mc_sync_db s;
	QFileDialog d(this);
	int rc;

	s.id = filter.sid;
	rc = db_select_sync(&s);
	if (rc) {
		reject();
		return;
	}
	if (!ui.filesBox->isChecked() && ui.directoriesBox->isChecked()) d.setFileMode(QFileDialog::Directory);
	d.setDirectory(s.path.c_str());
	if (d.exec()) {
		QString tmp = d.selectedFiles()[0];
		if (tmp.mid(0, s.path.length()) == s.path.c_str()) tmp = tmp.mid(s.path.length());
		ui.valueEdit->setText(tmp);
	};
}

int QtFilterDialog::typeToIndex(MC_FILTERTYPE type) {
	switch(type) {
		case MC_FILTERT_MATCH_NAME:
			return 0;
		case MC_FILTERT_MATCH_EXTENSION:
			return 1;
		case MC_FILTERT_MATCH_FULLNAME:
			return 2;
		case MC_FILTERT_MATCH_PATH:
			return 3;
		case MC_FILTERT_REGEX_NAME:
			return 4;
		case MC_FILTERT_REGEX_EXTENSION:
			return 5;
		case MC_FILTERT_REGEX_FULLNAME:
			return 6;
		case MC_FILTERT_REGEX_PATH:
			return 7;
		default:
			return 0;
	}
}

MC_FILTERTYPE QtFilterDialog::indexToType(int index) {
	switch(index) {
		case 0:
			return MC_FILTERT_MATCH_NAME;
		case 1:
			return MC_FILTERT_MATCH_EXTENSION;
		case 2:
			return MC_FILTERT_MATCH_FULLNAME;
		case 3:
			return MC_FILTERT_MATCH_PATH;
		case 4:
			return MC_FILTERT_REGEX_NAME;
		case 5:
			return MC_FILTERT_REGEX_EXTENSION;
		case 6:
			return MC_FILTERT_REGEX_FULLNAME;
		case 7:
			return MC_FILTERT_REGEX_PATH;
		default:
			return MC_FILTERT_MATCH_NAME;
	}
}