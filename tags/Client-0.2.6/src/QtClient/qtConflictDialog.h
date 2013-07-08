#ifndef QTCONFLICTDIALOG_H
#define QTCONFLICTDIALOG_H

#include <QtWidgets/QDialog>
#include <QtGui/QCloseEvent>
#include <QtCore/QTimer>
#include "ui_qtConflictDialog.h"
#include "mc.h"
#include "mc_workerthread.h"

class QtConflictDialog : public QDialog
{
	Q_OBJECT

public:
	QtConflictDialog(QWidget *parent = 0);
	~QtConflictDialog();
	int exec(std::string *fullPath, std::string *descLocal, std::string *descServer, int defaultValue, bool isDir, bool manualSolvePossible);

	enum DialogCode {
		// Bitmap 
		//     32        16       8    4      2        1
		// 0 0 Recursive Direcory Keep Upload Download Skip
		// Recursive is an extension to Directory!
		// Used in conflicted()
		Skip = 0,
		Download = 2,
		Upload = 4,
		Keep = 8,
		
		SkipD = 16,
		DownloadD = 18,
		UploadD = 20,
		KeepD = 24,
		
		DownloadDR = 50,
		UploadDR = 52,

		Terminating = 64
	};
	bool quit;

protected:
	void closeEvent(QCloseEvent *event);
	void keyPressEvent(QKeyEvent *e);
	void reject();

private:
	Ui::QtConflictDialog ui;
	QTimer quittimer;

private slots:
	void on_downloadButton_clicked();
	void on_uploadButton_clicked();
	void on_keepButton_clicked();
	void on_skipButton_clicked();
	void checkquit();
};

#endif // QTCONFLICTDIALOG_H
