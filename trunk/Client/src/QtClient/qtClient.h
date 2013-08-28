#ifndef QTCLIENT_H
#define QTCLIENT_H
#include <QtCore/QTimer>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QMenu>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QMessageBox>
#include "ui_qtClient.h"
#include "qtConflictDialog.h"
#include "qtSettingsDialog.h"
#include "qtSyncDialog.h"
#include "qtUpdateChecker.h"
#include "mc_types.h"
#include "mc_util.h"
#include "mc_db.h"
#include "mc_transfer.h"

#ifndef MC_QTCLIENT
#	error If you want to compile QtClient, use -DMC_QTCLIENT
#endif

class QtClient : public QMainWindow
{
	Q_OBJECT
signals:
	void _logOutput(QString s);
	void _notifyEnd(int evt);
	void _notifyStart(int evt, QString object);
//#ifdef MC_IONOTIFY //QtDisallow
	void _notifyIOEnd(int evt);
	void _notifyIOStart(int evt);
//#endif
	void _notifyProgress(qint64 value, qint64 total);
	void _notifySubProgress(double value, double total);

public:	
	QtClient(QWidget *parent = 0, int autorun = 0);
	~QtClient();
	static QtClient* instance(){ return _instance; }

	void logOutput(QString s);
	void notifyEnd(int evt);
	void notifyStart(int evt, std::string object);
#ifdef MC_IONOTIFY
	void notifyIOEnd(int evt);
	void notifyIOStart(int evt);
#endif
	void notifyProgress(qint64 value, qint64 total);
	void notifySubProgress(double value, double total);
	static int execConflictDialog(std::string *fullPath, std::string *descLocal, std::string *descServer, int defaultValue, bool manualSolvePossible);

	int listSyncs();
	void updateStatus(QString object);
	void setStatus(QString prefix, QString object, QIcon icon, bool wicon = true);

protected:
	void showEvent(QShowEvent *event);
	void hideEvent(QHideEvent *event);
	void closeEvent(QCloseEvent *event);

public slots:
	void startNewRun();
	void quit();

private slots:
	void __logOutput(QString s);
	void __notifyEnd(int evt);
	void __notifyStart(int evt, QString object);
//#ifdef MC_IONOTIFY //Qt disallow
	void __notifyIOEnd(int evt);
	void __notifyIOStart(int evt);
//#endif
	void __notifyProgress(qint64 value, qint64 total);
	void __notifySubProgress(double value, double total);
	int _execConflictDialog(std::string *fullPath, std::string *descLocal, std::string *descServer, int defaultValue, bool manualSolvePossible);
	void on_quitButton_clicked();
	void on_pushButton_clicked();
	void on_pushButton2_clicked();
	void on_addButton_clicked();
	void on_removeButton_clicked();
	void on_upButton_clicked();
	void on_downButton_clicked();
	void on_editButton_clicked();
	void on_settingsButton_clicked();
	void on_disableButton_clicked();
	void on_syncTable_itemSelectionChanged();
	void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
	void newVersion(QString newver);
	
private:
	static QtClient *_instance;
	Ui::QtClient ui;
	QtWorkerThread worker;
	QtUpdateChecker updateChecker;
	QtConflictDialog *conflictDialog;
	QLabel *statusLabel;
#ifdef MC_IONOTIFY
	QLabel *fsLabel,*dbLabel,*srvLabel;
#endif
	QProgressBar *progressBar;
	QLabel *progressLabel;
	QSystemTrayIcon *trayIcon;
	QMenu *trayIconMenu;
	QAction *quitAction, *showAction;
	QTimer *delayTimer;
	QIcon icon,icon_conn,icon_sync,icon_ul,icon_dl,icon_cf,icon_err,icon_ok;
	QIcon status_sync,status_err,status_done,status_ok,status_new,status_disabled,status_unknown;
	QIcon lock,enable,disable;
#ifdef MC_IONOTIFY
	QIcon act_nofs,act_fs,act_nodb,act_db,act_nosrv,act_srv;
#endif
	QString currentPrefix,currentConnectString,currentSyncString,currentFileString;
	float charWidth;
	bool uploading,downloading;
	int64 progressBase,progressTotal;
	std::vector<mc_sync_db> synclist;



};

#endif // QTCLIENT_H
