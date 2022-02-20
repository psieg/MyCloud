#include "qtGeneralFilterDialog.h"

QtGeneralFilterDialog::QtGeneralFilterDialog(QWidget *parent, QtNetworkPerformer *parentperf, mc_buf *parentibuf, mc_buf *parentobuf, bool needrefresh)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
	ui.filterTable->setColumnWidth(0, 23);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
	ui.filterTable->setColumnWidth(1, 23);
	ui.filterTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
	QFontMetrics fm(ui.filterTable->font()); 
	ui.filterTable->setColumnWidth(2, fm.horizontalAdvance(tr("MFull Name (regex)M")));
	ui.filterTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
	myparent = parent;
	performer = parentperf;
	netibuf = parentibuf;
	netobuf = parentobuf;
	refresh = needrefresh;
	file = QIcon(":/Resources/file.png");
	directory = QIcon(":/Resources/directory.png");
}

QtGeneralFilterDialog::~QtGeneralFilterDialog()
{
}

void QtGeneralFilterDialog::showEvent(QShowEvent *event) {
	int rc;
	if (refresh) {
		connect(performer, SIGNAL(finished(int)), this, SLOT(filterListReceived(int)));
		srv_listfilters_async(netibuf, netobuf, performer, 0);
	} else {
		listFilters();
		ui.fetchFilterLabel->setVisible(false);
		ui.filterTable->setEnabled(true);
	}
	QDialog::showEvent(event);
}

void QtGeneralFilterDialog::filterListReceived(int rc) {
	std::list<mc_filter> list;
	mc_sync_db s;
	disconnect(performer, SIGNAL(finished(int)), this, SLOT(filterListReceived(int)));
	if (rc) {
		reject();
		return;
	}

	rc = srv_listfilters_process(netobuf, &list);
	if (rc) {
		reject();
		return;
	}

	//global filters not encrypted

	rc = update_filters(0, &list);
	if (rc) {
		reject();
		return;
	}
	
	ui.fetchFilterLabel->setVisible(false);
	ui.filterTable->setEnabled(true);
	listFilters();
}

void QtGeneralFilterDialog::accept() {
	QDialog::accept();
}

void QtGeneralFilterDialog::listFilters() {
	std::list<mc_filter> fl;
	int rc;
	rc = db_list_filter_sid(&fl, 0);
	if (rc) return;
	filterlist.assign(fl.begin(), fl.end());
	ui.filterTable->clearContents();
	ui.filterTable->setRowCount(0);
	for (mc_filter& f : filterlist) {
		ui.filterTable->insertRow(ui.filterTable->rowCount());
		if (f.files) ui.filterTable->setItem(ui.filterTable->rowCount()-1, 0, new QTableWidgetItem(file, ""));
		if (f.directories) ui.filterTable->setItem(ui.filterTable->rowCount()-1, 1, new QTableWidgetItem(directory, ""));
		switch(f.type) {
			case MC_FILTERT_MATCH_NAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Name")));
				break;
			case MC_FILTERT_MATCH_EXTENSION:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Extension")));
				break;
			case MC_FILTERT_MATCH_FULLNAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Full Name")));
				break;
			case MC_FILTERT_MATCH_PATH:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Path")));
				break;
			case MC_FILTERT_REGEX_NAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Name (regex)")));
				break;
			case MC_FILTERT_REGEX_EXTENSION:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Extension (regex)")));
				break;
			case MC_FILTERT_REGEX_FULLNAME:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Full Name (regex)")));
				break;
			case MC_FILTERT_REGEX_PATH:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Path (regex)")));
				break;
			default:
				ui.filterTable->setItem(ui.filterTable->rowCount()-1, 2, new QTableWidgetItem(tr("Unrecognized")));
		}
					
		ui.filterTable->setItem(ui.filterTable->rowCount()-1, 3, new QTableWidgetItem(f.rule.c_str()));
	}
}