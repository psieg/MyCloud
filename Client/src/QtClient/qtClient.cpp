#include "qtClient.h"
#include "mc_types.h"
QtClient *QtClient::_instance = NULL;
#ifdef MC_IONOTIFY
#	define RESERVED_STATUSWIDTH				80
#	define RESERVED_STATUSWIDTH_PROGRESS	310
#else
#	define RESERVED_STATUSWIDTH				10
#	define RESERVED_STATUSWIDTH_PROGRESS	240
#endif

QtClient::QtClient(QWidget *parent, int autorun)
	: QMainWindow(parent)
{
	Q_ASSERT_X((QtClient::_instance == NULL), "QtClient", "There should only be one QtClient Instance");
	QtClient::_instance = this;
	uploading = false;
	downloading = false;
	conflictDialog = NULL;

	ui.setupUi(this);

	//QFontMetrics met(ui.statusBar->font());
	//charWidth = (met.width("ABCDEFKHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890")/50.0);

	connect(this,SIGNAL(_logOutput(QString)),this,SLOT(__logOutput(QString)),Qt::QueuedConnection);
	connect(this,SIGNAL(_notify(int, QString)),this,SLOT(__notify(int, QString)),Qt::BlockingQueuedConnection); //Notify presents dialogs to the user, wait for it
	connect(this,SIGNAL(_notifyEnd(int)),this,SLOT(__notifyEnd(int)),Qt::QueuedConnection);
	connect(this,SIGNAL(_notifyStart(int, QString)),this,SLOT(__notifyStart(int, QString)),Qt::QueuedConnection);
#ifdef MC_IONOTIFY
	connect(this,SIGNAL(_notifyIOEnd(int)),this,SLOT(__notifyIOEnd(int)),Qt::QueuedConnection);
	connect(this,SIGNAL(_notifyIOStart(int)),this,SLOT(__notifyIOStart(int)),Qt::QueuedConnection);
#endif
	connect(this,SIGNAL(_notifyProgress(qint64,qint64)),this,SLOT(__notifyProgress(qint64,qint64)),Qt::QueuedConnection);
	connect(this,SIGNAL(_notifySubProgress(double,double)),this,SLOT(__notifySubProgress(double,double)),Qt::QueuedConnection);

	connect(ui.syncTable,SIGNAL(itemActivated(QTableWidgetItem*)),ui.editButton,SLOT(click()));

	//list syncs 
	ui.syncTable->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
	ui.syncTable->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
	ui.syncTable->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
	QFontMetrics fm(ui.syncTable->font()); 
	ui.syncTable->setColumnWidth(3,fm.width(tr("MLast Sync successfulM"))+16);

	//ui.splitter->setStretchFactor(0,2);
	//ui.splitter->setStretchFactor(1,3);


	//load Icon Resources to memory
	icon = QIcon(":/Resources/icon.png");
	icon_conn = QIcon(":/Resources/icon_conn.png");
	icon_sync = QIcon(":/Resources/icon_sync.png");
	icon_ul = QIcon(":/Resources/icon_ul.png");
	icon_dl = QIcon(":/Resources/icon_dl.png");
	icon_cf = QIcon(":/Resources/icon_cf.png");
	icon_err = QIcon(":/Resources/icon_err.png");
	icon_ok = QIcon(":/Resources/icon_ok.png");
	status_sync = QIcon(":/Resources/status_sync.png");
	status_err = QIcon(":/Resources/status_err.png");
	status_done = QIcon(":/Resources/status_done.png");
	status_ok = QIcon(":/Resources/status_ok.png");
	status_new = QIcon(":/Resources/status_new.png");
	status_disabled = QIcon(":/Resources/status_disabled.png");
	status_warn = QIcon(":/Resources/status_warn.png");
	status_unknown = QIcon(":/Resources/status_unknown.png");
	lock = QIcon(":/Resources/lock.png");
	enable = QIcon(":/Resources/enable.png");
	disable = QIcon(":/Resources/disable.png");
#ifdef MC_IONOTIFY
	act_nofs = QIcon(":/Resources/act_nofs.png");
	act_fs = QIcon(":/Resources/act_fs.png");
	act_nodb = QIcon(":/Resources/act_nodb.png");
	act_db = QIcon(":/Resources/act_db.png");
	act_nosrv = QIcon(":/Resources/act_nosrv.png");
	act_srv = QIcon(":/Resources/act_srv.png");
#endif

	statusLabel = new QLabel();
	statusLabel->setText(tr("idle"));
	statusLabel->setToolTip(tr("idle"));
	ui.statusBar->addWidget(statusLabel,100);
	progressBar = new QProgressBar();
	progressBar->setRange(0,100);
	progressBar->setFormat("");
	progressBar->setTextVisible(false);
	progressBar->setMinimumWidth(120);
	progressBar->setMaximumWidth(120);
	progressBar->setMinimumHeight(15);
	progressBar->setMaximumHeight(15);
	ui.statusBar->addPermanentWidget(progressBar,0);
	progressBar->hide();
	progressLabel = new QLabel();
	progressLabel->setText("");
	progressLabel->setMinimumWidth(100);
	progressLabel->setMaximumWidth(100);
	ui.statusBar->addPermanentWidget(progressLabel,0);
	progressLabel->hide();
#ifdef MC_IONOTIFY
	fsLabel = new QLabel();
	fsLabel->setText("");
	fsLabel->setToolTip(tr("Activity on the filesystem"));
	fsLabel->setPixmap(act_nofs.pixmap(16));
	fsLabel->setMinimumWidth(18);
	ui.statusBar->addPermanentWidget(fsLabel,0);
	dbLabel = new QLabel();
	dbLabel->setText("");
	dbLabel->setToolTip(tr("Activity on the database"));
	dbLabel->setPixmap(act_nodb.pixmap(16));
	dbLabel->setMinimumWidth(18);
	ui.statusBar->addPermanentWidget(dbLabel,0);
	srvLabel = new QLabel();
	srvLabel->setText("");
	srvLabel->setToolTip(tr("Activity on the server connection"));
	srvLabel->setPixmap(act_nosrv.pixmap(16));
	srvLabel->setMinimumWidth(18);
	ui.statusBar->addPermanentWidget(srvLabel,0);
#endif

	//TrayIcon stuff
	showAction = new QAction(tr("&Show"), this);
    connect(showAction, SIGNAL(triggered()), this, SLOT(showNormal()));
	startAction = new QAction(tr("&Run"), this);
    connect(startAction, SIGNAL(triggered()), this, SLOT(startNewRun()));
	stopAction = new QAction(tr("&Abort"), this);
    connect(stopAction, SIGNAL(triggered()), this, SLOT(stopCurrentRun()));
	stopAction->setVisible(false);
	quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(quit()));

	trayIconMenu = new QMenu(this);
	trayIconMenu->addAction(showAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(startAction);
	trayIconMenu->addAction(stopAction);
	trayIconMenu->addAction(quitAction);

	trayIcon = new QSystemTrayIcon(this);
	trayIcon->setContextMenu(trayIconMenu);
	trayIcon->setIcon(icon);
	trayIcon->setToolTip(tr("MyCloud"));
	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
	trayIcon->show();

	//Update stuff
	ui.updateButton->setVisible(false);
	connect(&updateChecker,SIGNAL(newVersion(QString)),this,SLOT(newVersion(QString)));

	if(autorun>0){
		setStatus("idle (delay " + QString::number(autorun) + " sec)","",icon);
		delayTimer = new QTimer();
		delayTimer->setSingleShot(true);
		delayTimer->setInterval(autorun*1000);
		connect(delayTimer,SIGNAL(timeout()),this,SLOT(startNewRun()));
		connect(delayTimer,SIGNAL(timeout()),&updateChecker,SLOT(checkForUpdate()));
		delayTimer->start();
	} else {
		delayTimer = new QTimer();
		delayTimer->setSingleShot(true);
		delayTimer->setInterval(5000);
		connect(delayTimer,SIGNAL(timeout()),&updateChecker,SLOT(checkForUpdate()));
		delayTimer->start();
	}
}

QtClient::~QtClient()
{
	if(conflictDialog) delete conflictDialog;
	stopCurrentRun();
	QtClient::_instance = NULL;
}

void QtClient::showEvent(QShowEvent *event)
{
	listSyncs();
	QMainWindow::showEvent(event);
}

void QtClient::hideEvent(QHideEvent *event)
{
	QMainWindow::hideEvent(event);
}

void QtClient::closeEvent(QCloseEvent *event)
{
	if (trayIcon->isVisible()){
		hide();
		event->ignore();
	}
}

void QtClient::startNewRun(){
	//ui.textEdit->clear();
	std::cout << "<i>start</i>" << std::endl;
	worker.start();
}

void QtClient::stopCurrentRun(){
	std::cout << "<i>stop</i>" << std::endl;
	worker.quit();
}

void QtClient::quit(){
	if(conflictDialog) { //we need do go back through the stacked event loops to "unlock" the worker
		worker.terminating = true;
		conflictDialog->quit = true;
	} else {
		stopCurrentRun();
		qApp->quit();
	}
}

void QtClient::logOutput(QString s){
	if(QThread::currentThread() == QtWorkerThread::instance()){
		emit _logOutput(s);
#ifdef MC_DEBUGLOW
		//mc_sleepms(150); //not writing debuglow to cout
#else
#ifdef MC_DEBUG
		//mc_sleepms(50); //not writing debug to cout
#endif
#endif

	} else {
		emit _logOutput(s);
	}
}

void QtClient::__logOutput(QString s){
	if(ui.textEdit->document()->lineCount() > 1024) 
		ui.textEdit->clear();
	ui.textEdit->append(s);
	//ui.textEdit->setHtml(ui.textEdit->toHtml() + s);
}

void QtClient::notify(int evt, std::string object){
	emit _notify(evt,object.c_str());
}
void QtClient::notifyEnd(int evt){
	emit _notifyEnd(evt);
}
void QtClient::notifyStart(int evt, std::string object){
	emit _notifyStart(evt,object.c_str());
}
#ifdef MC_IONOTIFY
void QtClient::notifyIOEnd(int evt){
	if(this->isVisible() && !this->isMinimized())
		emit _notifyIOEnd(evt);
}
void QtClient::notifyIOStart(int evt){
	if(this->isVisible() && !this->isMinimized())
		emit _notifyIOStart(evt);
}
#endif
void QtClient::notifyProgress(qint64 value, qint64 total)
{
	emit _notifyProgress(value,total);
}
void QtClient::notifySubProgress(double value, double total)
{
	if(value != 0)
		emit _notifySubProgress(value,total);
}


void QtClient::__notify(int evt, QString object){
	switch(evt){
		case MC_NT_ERROR:
			progressBar->hide();
			progressLabel->hide();
			setStatus(tr("Error: "),object,icon_err);
			break;
		case MC_NT_FULLSYNC: //A little out of row
			listSyncs(); //refresh listing
			setStatus(tr("Fully synced (") + object + ")","",icon_ok);
			break;
		case MC_NT_NOSYNCWARN: //Even more out of row
			trayIcon->showMessage(tr("No Server Connection"), tr("Last successful server connection: ") + object + tr(".\n"
				"You might want to check the server settings"), QSystemTrayIcon::Critical);
			disconnect(trayIcon,SIGNAL(messageClicked()));
			connect(trayIcon,SIGNAL(messageClicked()),this,SLOT(show()));
			break;
		case MC_NT_CRYPTOFAIL:
			QMessageBox::critical(this, tr("Server untrusted"), tr("The decryption of data from the server failed.\n"
																	"This can only occur if:\n"
																	"* the data was modified on the server\n"
																	"* the owner deleted and re-created the sync with a different key\n"
																	"* in case of a bug in the software\n"
																	"The server is not considered trusted any more and we will not sync any more data.\n"
																	"The  sync has been disabled, log for details where exactly the failure occured.\n"
																	"If you absolutely trust the server, remove the prefix from the server URL."));
			break;
		case MC_NT_SERVERRESET:
			QMessageBox::information(this, tr("Server reset"), tr("The server has been reset, which changed all IDs.\n") +
																	object + tr(" Syncs could be recovered.\n"
																	"We tried to automatically adopt the new IDs, please check the Sync list and restart the worker."));
			listSyncs();
			break;
		default:
			setStatus(tr("Unknown Notify Type received"),"",icon);
	}
}
void QtClient::__notifyEnd(int evt)
{
	uploading = false;
	downloading = false;
	switch((MC_NOTIFYSTATETYPE)evt){
		case MC_NT_CONN:
			setStatus(tr("idle"),"",icon);
			break;
		case MC_NT_SYNC:
			listSyncs(); //refresh listing
			progressBar->hide();
			progressLabel->hide();
			setStatus(tr("Connected to "),currentConnectString,icon_conn);
			break;
		case MC_NT_UL:
			progressBar->setRange(0,0);
			progressLabel->setText(tr("Walking..."));
			progressBar->show();
			progressLabel->show();
			setStatus(tr("Syncing "),currentSyncString,icon_sync);
			break;
		case MC_NT_DL:
			progressBar->setRange(0,0);
			progressLabel->setText(tr("Walking..."));
			progressBar->show();
			progressLabel->show();
			setStatus(tr("Syncing "),currentSyncString,icon_sync);
			break;
		default:
			setStatus(tr("Unknown Notify Type received"),"",icon);
	}
}
void QtClient::__notifyStart(int evt, QString object){
	uploading = false;
	downloading = false;
	switch(evt){
		case MC_NT_CONN:
			currentConnectString = object;
			setStatus(tr("Connected to "),object,icon_conn);
			break;
		case MC_NT_SYNC:
			listSyncs(); //refresh listing
			currentSyncString = object;
			progressBar->setRange(0,0);
			progressLabel->setText(tr("Walking..."));
			progressBar->show();
			progressLabel->show();
			setStatus(tr("Syncing "),object,icon_sync);
			break;
		case MC_NT_UL:
			currentFileString = object;
			progressBar->setRange(0,100);
			progressBar->setValue(0);
			progressLabel->setText(tr("Preparing..."));
			progressBar->show();
			progressLabel->show();
			setStatus(tr("Uploading "),object,icon_ul);
			uploading = true;
			break;
		case MC_NT_DL:
			currentFileString = object;
			progressBar->setRange(0,100);
			progressBar->setValue(0);
			progressLabel->setText(tr("Preparing..."));
			progressBar->show();
			progressLabel->show();
			setStatus(tr("Downloading "),object,icon_dl);
			downloading = true;
			break;
		default:
			setStatus(tr("Unknown Notify Type received"),"",icon);
	}
}

void QtClient::__notifyIOEnd(int evt){
#ifdef MC_IONOTIFY
	switch(evt){
		case MC_NT_FS:
			fsLabel->setPixmap(act_nofs.pixmap(16));
			break;
		case MC_NT_DB:
			dbLabel->setPixmap(act_nodb.pixmap(16));
			break;
		case MC_NT_SRV:
			srvLabel->setPixmap(act_nosrv.pixmap(16));
			break;
	}
#endif
}
void QtClient::__notifyIOStart(int evt){
#ifdef MC_IONOTIFY
	switch(evt){
		case MC_NT_FS:
			fsLabel->setPixmap(act_fs.pixmap(16));
			break;
		case MC_NT_DB:
			dbLabel->setPixmap(act_db.pixmap(16));
			break;
		case MC_NT_SRV:
			srvLabel->setPixmap(act_srv.pixmap(16));
			break;
	}
#endif
}

void QtClient::__notifyProgress(qint64 value, qint64 total)
{
	int percent;
	progressBase = value;
	progressTotal = total;
	if(total == 0) percent = 0;
	else percent = ((double)value/total)*100;
	updateStatus(currentFileString + " (" + QString::number(percent) + "%)");
	progressBar->setValue(percent);
	progressLabel->setText(BytesToSize(value,false,-1/*,1*/).c_str() + tr(" / ") + BytesToSize(total,false,-1).c_str());
}
//										dl			ul
void QtClient::__notifySubProgress(double value, double total)
{
	if(this->isVisible()){
		int64 sum = 0;
		if(uploading){
			sum = progressBase + total;
		} else if(downloading){
			sum = progressBase + value;
		}
		if(sum){
			int percent;
			//qt includes headers, so we might be over file size
			if(sum > progressTotal) { 
				percent = 100; 
				sum = progressTotal;
			} else {
				if(progressTotal == 0) percent = 0;
				else percent = ((double)sum/progressTotal)*100;
			}
			updateStatus(currentFileString + " (" + QString::number(percent) + "%)");
			progressBar->setValue(percent);
			progressLabel->setText(BytesToSize(sum,false,-1).c_str() + tr(" / ") + BytesToSize(progressTotal,false,-1).c_str());
		}
	}
}

int QtClient::execConflictDialog(std::string *fullPath, std::string *descLocal, std::string *descServer, int defaultValue, bool manualSolvePossible)
{
	int rc = -2;
	
	//foreach(QWidget *widget, qApp->topLevelWidgets()) {
	//	if(widget->inherits("QMainWindow")){
			QMetaObject::invokeMethod(/*qobject_cast<QtClient*>(widget)*/QtClient::instance(),"_execConflictDialog",Qt::BlockingQueuedConnection,
				Q_RETURN_ARG(int,rc),Q_ARG(std::string*,fullPath),Q_ARG(std::string*,descLocal),Q_ARG(std::string*,descServer),Q_ARG(int,defaultValue),Q_ARG(bool,manualSolvePossible));
			return rc;
	//	}
	//}
	return -1;
	
}

int  QtClient::_execConflictDialog(std::string *fullPath, std::string *descLocal, std::string *descServer, int defaultValue, bool manualSolvePossible)
{
	int rc;
	QString object = fullPath->c_str();
	setStatus(tr("Conflict detected: "),object,icon_cf);
	progressBar->hide();
	progressLabel->hide();
	if(!conflictDialog){
		QWidget *parent = 0;
		if(this->isVisible()) parent = this;
		conflictDialog = new QtConflictDialog(parent);
	}
	if(!this->isActiveWindow()){
		trayIcon->showMessage(tr("Conflict Detected"), tr("A file conflict has been detected, you need to choose what to do.\nClick to view"), QSystemTrayIcon::Warning);
	}
	disconnect(trayIcon,SIGNAL(messageClicked()));
	connect(trayIcon,SIGNAL(messageClicked()),conflictDialog,SLOT(forceSetFocus()));
	rc = conflictDialog->exec(fullPath,descLocal,descServer,defaultValue,manualSolvePossible);
	disconnect(trayIcon,SIGNAL(messageClicked()));
	progressBar->show();
	progressLabel->show();
	setStatus(tr("Syncing "),currentSyncString,icon_sync);
	return rc;
}

void QtClient::on_quitButton_clicked(){
	std::cout << "<i>terminating</i>" << std::endl;
	qApp->quit();
}

void QtClient::on_pushButton_clicked(){
	startNewRun();
}
void QtClient::on_pushButton2_clicked(){
	stopCurrentRun();
}

void QtClient::on_addButton_clicked(){
	QtSyncDialog d(this);	
	d.exec();

	listSyncs();
	ui.syncTable->selectRow(ui.syncTable->rowCount()-1);
}

void QtClient::on_removeButton_clicked(){
	int rc;
	int sid = synclist[ui.syncTable->selectedItems().at(0)->row()].id;
	bool cando = true;
	QMessageBox b(this);
	b.setText("This will delete the local state information");
	b.setInformativeText("The files on disk will not be modified, but the files will no longer be synced.");
	b.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
	b.setDefaultButton(QMessageBox::Ok);
	b.setIcon(QMessageBox::Warning);

	if(b.exec() == QMessageBox::Ok){
		
		if(worker.isRunning()){
			QMessageBox b2(this);
			b2.setText(tr("Worker stop required"));
			b2.setInformativeText(tr("Deleting the sync requires the worker to be stopped."));
			b2.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
			b2.setDefaultButton(QMessageBox::Ok);
			b2.setIcon(QMessageBox::Warning);
			if(b2.exec() == QMessageBox::Ok){
				if(worker.isRunning()){
					MC_INF("Stopping Worker");
					stopCurrentRun();
				}
			} else {
				cando = false;
			}
		}
		if(cando){	
			this->setCursor(Qt::WaitCursor);
			ui.syncTable->setEnabled(false);
			setStatus(tr("Deleting sync"),"",icon_sync);
			QApplication::processEvents();

			std::list<mc_file> l;
			std::list<mc_file>::iterator lit,lend;
			rc = db_list_file_parent(&l, -sid);
			if(rc) setStatus(tr("Error"),"",icon_err);
			lit = l.begin();
			lend = l.end();
			rc = db_execstr("BEGIN TRANSACTION");
			if(rc) setStatus(tr("Error"),"",icon_err);

			while(lit != lend){
				QApplication::processEvents();
				rc = purge(&*lit,NULL);
				if(rc) setStatus(tr("Error"),"",icon_err);
				++lit;
			}
			rc = db_execstr("COMMIT");
			if(rc) setStatus(tr("Error"),"",icon_err);
			rc = db_delete_filter_sid(sid);
			if(rc) setStatus(tr("Error"),"",icon_err);


			rc = db_delete_filter_sid(sid);
			if(rc) setStatus(tr("Error"),"",icon_err);

			rc = db_delete_sync(sid);
			if(rc) setStatus(tr("Error"),"",icon_err);
			listSyncs();

			if(!rc) setStatus(tr("idle"),"",icon);
			this->setCursor(Qt::ArrowCursor);
			ui.syncTable->setEnabled(true);
		}
	}
}

void QtClient::on_upButton_clicked(){
	int rc;
	int index = ui.syncTable->selectedItems().at(0)->row();
	int tmp = synclist[index].priority;
	synclist[index].priority = synclist[index-1].priority;
	synclist[index-1].priority = tmp;
	ui.syncTable->setEnabled(false);
	QApplication::processEvents();
	rc = db_update_sync(&synclist[index]);
	if(rc) return;
	rc = db_update_sync(&synclist[index-1]);
	if(rc) return;
	listSyncs();
	ui.syncTable->setEnabled(true);
	ui.syncTable->selectRow(index-1);
}

void QtClient::on_downButton_clicked(){
	int rc;
	const int index = ui.syncTable->selectedItems().at(0)->row();
	int tmp = synclist[index].priority;
	synclist[index].priority = synclist[index+1].priority;
	synclist[index+1].priority = tmp;
	ui.syncTable->setEnabled(false);
	QApplication::processEvents();
	rc = db_update_sync(&synclist[index]);
	if(rc) return;
	rc = db_update_sync(&synclist[index+1]);
	if(rc) return;
	listSyncs();
	ui.syncTable->setEnabled(true);
	ui.syncTable->selectRow(index+1);
}

void QtClient::on_editButton_clicked(){
	int index = ui.syncTable->selectedItems().at(0)->row();
	QtSyncDialog d(this,synclist[index].id);
	d.exec();
	listSyncs();
	ui.syncTable->selectRow(index);
}

void QtClient::on_settingsButton_clicked(){
	QtSettingsDialog d(this);
	if (d.exec() == QtSettingsDialog::NeedRestart){
		if(worker.isRunning()){
			QMessageBox b(this);
			b.setText(tr("Changing the settings requires restarting the Sync"));
			b.setInformativeText(tr("Hit OK to restart now or Cancel to apply on next run."));
			b.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
			b.setDefaultButton(QMessageBox::Ok);
			b.setIcon(QMessageBox::Warning);
			if(b.exec() == QMessageBox::Ok){
				if(worker.isRunning()){
					MC_INF("Restarting Worker");
					stopCurrentRun();
					startNewRun();
				}
			}
		}
	}
}

void QtClient::on_disableButton_clicked(){
	int rc;
	int index = ui.syncTable->selectedItems().at(0)->row();
	if(synclist[index].status == MC_SYNCSTAT_DISABLED || synclist[index].status == MC_SYNCSTAT_CRYPTOFAIL)
		synclist[index].status = MC_SYNCSTAT_UNKOWN;
	else
		synclist[index].status = MC_SYNCSTAT_DISABLED;
	ui.syncTable->setEnabled(false);
	QApplication::processEvents();
	rc = db_update_sync(&synclist[index]);
	if(rc) return;
	listSyncs();
	ui.syncTable->setEnabled(true);
	ui.syncTable->selectRow(index);
}

void QtClient::on_syncTable_itemSelectionChanged(){
	if(ui.syncTable->selectedItems().length() == 0){
		ui.removeButton->setEnabled(false);
		ui.upButton->setEnabled(false);
		ui.downButton->setEnabled(false);
		ui.editButton->setEnabled(false);
		ui.disableButton->setEnabled(false);
	} else {
		ui.removeButton->setEnabled(true);
		int r = ui.syncTable->selectedItems().at(0)->row();
		if(r > 0) ui.upButton->setEnabled(true);
		else ui.upButton->setEnabled(false);
		if(r < ui.syncTable->rowCount()-1) ui.downButton->setEnabled(true);
		else ui.downButton->setEnabled(false);
		ui.disableButton->setEnabled(true);
		ui.editButton->setEnabled(true);
		if(synclist[r].status == MC_SYNCSTAT_DISABLED || synclist[r].status == MC_SYNCSTAT_CRYPTOFAIL) ui.disableButton->setIcon(enable);
		else ui.disableButton->setIcon(disable);
	}
}

void QtClient::trayIconActivated(QSystemTrayIcon::ActivationReason reason){    
	switch (reason) {
		case QSystemTrayIcon::Trigger:
			if(this->isVisible()){ this->showNormal(); this->activateWindow(); }
			break;
		case QSystemTrayIcon::DoubleClick:
			if(this->isVisible()) this->hide();
			else { this->show(); this->activateWindow(); }
			break;
		case QSystemTrayIcon::MiddleClick:
			break;
		case QSystemTrayIcon::Context:
			if(worker.isRunning()){
				startAction->setVisible(false);
				stopAction->setVisible(true);
			} else {
				startAction->setVisible(true);
				stopAction->setVisible(false);
			}
			if(qApp->activeModalWidget()){
				startAction->setEnabled(false);
				stopAction->setEnabled(false);
				quitAction->setEnabled(false);
			} else {
				startAction->setEnabled(true);
				stopAction->setEnabled(true);
				quitAction->setEnabled(true);
			}
			break;
		default:
			;
     }
}

void QtClient::newVersion(QString newver){
	trayIcon->showMessage(tr("Client Update available"), tr("Version ") + newver + tr(" is available.\nCurrent Version: ") + MC_VERSION + tr(". Click to update"));
	disconnect(trayIcon,SIGNAL(messageClicked()));
	connect(trayIcon,SIGNAL(messageClicked()),&updateChecker,SLOT(getUpdate()));
	ui.updateButton->setText(tr("new Version ") + newver + tr(" available!"));
	ui.updateButton->setVisible(true);
	connect(ui.updateButton,SIGNAL(clicked()),&updateChecker,SLOT(getUpdate()));
}

int QtClient::listSyncs(){
	int rc;
	std::list<mc_sync_db> l;
	std::vector<mc_sync_db>::iterator lit,lend;
	mc_status s;

	rc = db_select_status(&s);
	if(rc) return rc;

	rc = db_list_sync(&l);
	if(rc) return rc;

	l.sort(compare_mc_sync_db_prio);

	synclist.assign(l.begin(),l.end());

	lit = synclist.begin();
	lend = synclist.end();

	ui.syncTable->clearContents();
	ui.syncTable->setRowCount(0);
	while(lit != lend){
		ui.syncTable->insertRow(ui.syncTable->rowCount());
		if(lit->crypted)
			if(lit->uid != s.uid) ui.syncTable->setItem(ui.syncTable->rowCount()-1,0,new QTableWidgetItem(lock,QString(lit->name.c_str())+tr(" (shared)")));
			else ui.syncTable->setItem(ui.syncTable->rowCount()-1,0,new QTableWidgetItem(lock,QString(lit->name.c_str())));
		else
			if(lit->uid != s.uid) ui.syncTable->setItem(ui.syncTable->rowCount()-1,0,new QTableWidgetItem(icon,QString(lit->name.c_str())+tr(" (shared)")));
			else ui.syncTable->setItem(ui.syncTable->rowCount()-1,0,new QTableWidgetItem(icon,QString(lit->name.c_str())));
		ui.syncTable->setItem(ui.syncTable->rowCount()-1,1,new QTableWidgetItem(QString(lit->path.c_str())));
		ui.syncTable->setItem(ui.syncTable->rowCount()-1,2,new QTableWidgetItem(QString(TimeToString(lit->lastsync).c_str())));
		switch(lit->status){
			case MC_SYNCSTAT_UNKOWN:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_new,tr("Unknown / Waiting")));
				break;
			case MC_SYNCSTAT_RUNNING:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_sync,tr("Sync running")));
				break;
			case MC_SYNCSTAT_COMPLETED:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_done,tr("Last Sync completed")));
				break;
			case MC_SYNCSTAT_SYNCED:
				if(time(NULL) - lit->lastsync < 300)
					ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_ok,tr("Last Sync successful")));
				else
					ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_done,tr("Last Sync successful")));
				break;
			case MC_SYNCSTAT_UNAVAILABLE:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_err,tr("Not available")));
				break;
			case MC_SYNCSTAT_FAILED:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_err,tr("Last Sync failed")));
				break;
			case MC_SYNCSTAT_ABORTED:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_err,tr("Last Sync aborted")));
				break;
			case MC_SYNCSTAT_DISABLED:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_disabled,tr("disabled")));
				break;
			case MC_SYNCSTAT_CRYPTOFAIL:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_warn,tr("Decryption failure!")));
				break;

			default:
				ui.syncTable->setItem(ui.syncTable->rowCount()-1,3,new QTableWidgetItem(status_unknown,tr("Unkown Status")));
		}
		//ui.syncTable->setItem(ui.syncTable->rowCount()-1,4,new QTableWidgetItem(/*icon,*/QString::number(lit->priority)));
		++lit;
	}
	return 0;
}

void QtClient::updateStatus(QString object){
	QFontMetrics met(ui.statusBar->font());
	QString obj = object;
	int prwidth = met.width(currentPrefix);
	int powidth = met.width("...");

	trayIcon->setToolTip(currentPrefix + (object.length()>60-currentPrefix.length()?"..."+object.right(60-currentPrefix.length()):object));
	statusLabel->setToolTip(currentPrefix + object);
	if(progressBar->isVisible()){
		if(met.width(obj)+prwidth < this->geometry().width()-RESERVED_STATUSWIDTH_PROGRESS) statusLabel->setText(currentPrefix + obj);
		else {
			if(this->geometry().width() < RESERVED_STATUSWIDTH_PROGRESS+prwidth+powidth+20) obj = ""; else
				while(met.width(obj)+prwidth+powidth > this->geometry().width()-RESERVED_STATUSWIDTH_PROGRESS) obj = obj.right(obj.length()-5);
			statusLabel->setText(currentPrefix + "..." + obj);
		}
		//statusLabel->setText(currentPrefix + (object.length()>(((this->geometry().width()-310)/charWidth)-(currentPrefix.length()+4))?"..."+object.right(((this->geometry().width()-310)/charWidth)-(currentPrefix.length()+4)):object));
	} else {
		if(met.width(obj)+prwidth < this->geometry().width()-RESERVED_STATUSWIDTH) statusLabel->setText(currentPrefix + obj);
		else {
			if(this->geometry().width() < RESERVED_STATUSWIDTH+prwidth+powidth+20) obj = ""; else
				while(met.width(obj)+prwidth+powidth > this->geometry().width()-RESERVED_STATUSWIDTH) obj = obj.right(obj.length()-5);
			statusLabel->setText(currentPrefix + "..." + obj);
		}
		//statusLabel->setText(currentPrefix + (object.length()>(((this->geometry().width()-80)/charWidth)-(currentPrefix.length()+4))?"..."+object.right(((this->geometry().width()-80)/charWidth)-(currentPrefix.length()+4)):object));
	}
}
void QtClient::setStatus(QString prefix, QString object, QIcon icon, bool wicon){
	QFontMetrics met(ui.statusBar->font());
	QString obj = object;
	int prwidth = met.width(prefix);
	int powidth = met.width("...");

	currentPrefix = prefix;
	if(wicon){
		trayIcon->setToolTip(prefix + (object.length()>60-prefix.length()?"..."+object.right(60-prefix.length()):object));
		trayIcon->setIcon(icon);
		this->setWindowIcon(icon);
	}
	statusLabel->setToolTip(prefix + object);
	if(progressBar->isVisible()){
		if(met.width(obj)+prwidth < this->geometry().width()-RESERVED_STATUSWIDTH_PROGRESS) statusLabel->setText(prefix + obj);
		else {
			if(this->geometry().width() < RESERVED_STATUSWIDTH_PROGRESS+prwidth+powidth+20) obj = ""; else
				while(met.width(obj)+prwidth+powidth > this->geometry().width()-RESERVED_STATUSWIDTH_PROGRESS) obj = obj.right(obj.length()-5);
			statusLabel->setText(prefix + "..." + obj);
		}
		//statusLabel->setText(prefix + (object.length()>(((this->geometry().width()-310)/charWidth)-(prefix.length()+4))?"..."+object.right(((this->geometry().width()-310)/charWidth)-(prefix.length()+4)):object));
	} else {
		if(met.width(obj)+prwidth < this->geometry().width()-RESERVED_STATUSWIDTH) statusLabel->setText(prefix + obj);
		else {
			if(this->geometry().width() < RESERVED_STATUSWIDTH+prwidth+powidth+20) obj = ""; else
				while(met.width(obj)+prwidth+powidth > this->geometry().width()-RESERVED_STATUSWIDTH) obj = obj.right(obj.length()-5);
			statusLabel->setText(prefix + "..." + obj);
		}
		//statusLabel->setText(prefix + (object.length()>(((this->geometry().width()-80)/charWidth)-(prefix.length()+4))?"..."+object.right(((this->geometry().width()-80)/charWidth)-(prefix.length()+4)):object));
	}
}