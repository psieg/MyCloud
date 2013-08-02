#include "qtSyncDialog.h"
#include "qtClient.h"

qtSyncDialog::qtSyncDialog(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	myparent = parent;
	performer = NULL;
}

qtSyncDialog::~qtSyncDialog()
{
	if(performer) delete performer;
}

void qtSyncDialog::showEvent(QShowEvent *event){
	mc_status s;
	int rc;


	rc = db_select_status(&s);
	if(rc) return;
	
	if(s.url == ""){		
		QMessageBox b(myparent);
		b.setText("No server configured");
		b.setInformativeText("To subscribe syncs, you need to set the server first.");
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	} else {
		QString _url = "https://";
		_url.append(s.url.c_str());
		_url.append("/bin.php");
		performer = new QtNetworkPerformer(_url,"trustCA.crt",s.acceptallcerts,true);
		SetBuf(&netibuf);
		SetBuf(&netobuf);
	}
	
	connect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
	srv_auth_async(&netibuf,&netobuf,performer,s.uname,s.passwd,&authtime);
	
	QDialog::showEvent(event);
}

void qtSyncDialog::authed(int rc){
	int64 dummy;
	if(rc){
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	} else {
		disconnect(performer,SIGNAL(finished(int)),this,SLOT(authed(int)));
		rc = srv_auth_process(&netobuf,&authtime,&dummy);
		if(rc){
			QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
			return;
		}
		connect(performer,SIGNAL(finished(int)),this,SLOT(listReceived(int)));
		srv_listsyncs_async(&netibuf,&netobuf,performer);
	}
	
}

void qtSyncDialog::listReceived(int rc){
	std::list<mc_sync> list;
	std::vector<mc_sync>::iterator lit,lend;
	if(rc) {
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	} else {
		rc = srv_listsyncs_process(&netobuf,&list);
		if(rc){
			QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
			return;
		}
	}

	if(list.size() > 0){
		l = vector<mc_sync>(list.begin(),list.end());
		lit = l.begin();
		lend = l.end();

		while(lit != lend){
			if(lit->crypted)
				ui.nameBox->addItem(QString(lit->name.c_str())+tr(" (encrypted)"));
			else
				ui.nameBox->addItem(QString(lit->name.c_str()));
			++lit;
		}
		ui.fetchLabel->setVisible(false);
		ui.nameBox->setEnabled(true);
		ui.okButton->setEnabled(true);
	} else {
		QMessageBox b(myparent);
		b.setText(tr("No Sync available on server"));
		b.setInformativeText(tr("There are no syncs on the server, you have to set them up on the server first."));
		b.setStandardButtons(QMessageBox::Ok);
		b.setDefaultButton(QMessageBox::Ok);
		b.setIcon(QMessageBox::Warning);
		b.exec();
		QMetaObject::invokeMethod(this, "reject", Qt::QueuedConnection);
		return;
	}
}


void qtSyncDialog::accept(){
	int rc;
	int maxprio=0;
	std::list<mc_sync_db> sl;
	std::list<mc_sync_db>::iterator slit,slend;
	QByteArray ckey;

	rc = db_list_sync(&sl);
	if(rc) reject();

	slit = sl.begin();
	slend = sl.end();
	while(slit != slend){
		if(slit->priority > maxprio) maxprio = slit->priority;
		++slit;
	}

	mc_sync_db s;
	s.filterversion = 0;
	memset(s.hash,0,16);
	s.lastsync = 0;
	s.id = l[ui.nameBox->currentIndex()].id;
	s.priority = maxprio+1;
	s.name = l[ui.nameBox->currentIndex()].name;
	s.path = qPrintable(ui.pathEdit->text().replace("\\","/").append("/"));
	s.crypted = l[ui.nameBox->currentIndex()].crypted;
	if(s.crypted){
		if(ui.keyEdit->text() != ""){
			QRegExp hexMatcher("^[0-9A-F]{64}$", Qt::CaseInsensitive);
			if (hexMatcher.exactMatch(ui.keyEdit->text())){
				ckey = QByteArray::fromHex(ui.keyEdit->text().toLatin1());
			} else {
				QMessageBox b(myparent);
				b.setText(tr("Please enter a valid 256-bit key in hex format"));
				b.setInformativeText(tr("Alternatively, you can leave the field empty to generate a new key, if you don't have one yet."));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Warning);
				b.exec();
				return;
			}
		} else {
			ckey = QByteArray(32,'\0');
			if(crypt_randkey((unsigned char*)ckey.data())){
				QMessageBox b(myparent);
				b.setText(tr("Can't generate keys atm"));
				b.setInformativeText(tr("Please enter the CryptKey for this sync."));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Warning);
				b.exec();
				return;
			} else {
				ui.keyEdit->setText(ckey.toHex());
				QMessageBox b(myparent);
				b.setText(tr("New Key generated"));
				b.setInformativeText(tr("A new key has been generated. Make a copy and store it in a safe place so you can enter it on other computers"));
				b.setStandardButtons(QMessageBox::Ok);
				b.setDefaultButton(QMessageBox::Ok);
				b.setIcon(QMessageBox::Information);
				b.exec();
				return;
			}
		}
		memcpy(s.cryptkey,ckey.constData(),32);
	} else memset(s.cryptkey,0,32);
	s.status = MC_SYNCSTAT_UNKOWN;
	rc = db_insert_sync(&s);
	if(rc) reject();
	QDialog::accept();
}

void qtSyncDialog::reject(){
	QDialog::reject();
}

void qtSyncDialog::on_browseButton_clicked(){
	QFileDialog d(this);
	d.setFileMode(QFileDialog::Directory);
	d.setNameFilter(tr("Directories"));
	d.setOption(QFileDialog::ShowDirsOnly,true);
	if(d.exec()){
		ui.pathEdit->setText(d.selectedFiles()[0]);
	};
}

void qtSyncDialog::on_nameBox_currentIndexChanged(int index){
	ui.keyEdit->setEnabled(l[index].crypted);
}
