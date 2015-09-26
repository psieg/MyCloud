
#include <QtCore/QCoreApplication>

#include "mc.h"
#include "mc_fs.h"
#include "mc_srv.h"
#include "mc_crypt.h"
#include "mc_util.h"
#include "mc_watch.h"

#	define MC_NOTIFYPROGRESS(val, tot)	{			\
	cout << BytesToSize(val, false) << "/" << BytesToSize(tot, false) << " ";	\
	cout << (((double)val)/tot)*100 << "%" << endl;	\
}

#define MC_CHKERR(rc)					{ if (rc) { getchar(); getchar(); return rc; } }

void SetStdinEcho(bool enable = true)
{
#ifdef WIN32
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);

	if (!enable)
		mode &= ~ENABLE_ECHO_INPUT;
	else
		mode |= ENABLE_ECHO_INPUT;

	SetConsoleMode(hStdin, mode);

#else
	struct termios tty;
	tcgetattr(STDIN_FILENO, &tty);
	if (!enable)
		tty.c_lflag &= ~ECHO;
	else
		tty.c_lflag |= ECHO;

	(void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

int download_actual(mc_crypt_ctx *cctx, const string& fpath, mc_file *srv, FILE* fdesc) {
	int64 offset = 0, written = 0;
	int rc;
	rc = crypt_init_download(cctx, srv);
	MC_CHKERR_DOWN(rc, cctx);

	while (offset < srv->size) {
		MC_NOTIFYPROGRESS(offset, srv->size);
		MC_CHECKTERMINATING_DOWN(cctx);
		int i = 0;
		rc = 1;
		while (rc) {
			rc = crypt_getfile(cctx, srv->id, offset, MC_RECVBLOCKSIZE, fdesc, &written, srv->hash);
			i++;
			if (i > 10) break;
		}
		MC_CHKERR_DOWN(rc, cctx);
		offset += written;
	}

	rc = crypt_finish_download(cctx);
	MC_CHKERR(rc);
	return 0;
}

#define HARDCODED

int main(int argc, char *argv[])
{
	int rc;
	QCoreApplication a(argc, argv);

	//get connection data
	string url, uname, passwd;
	int64 basedate;
	int uid;
#ifdef HARDCODED
	url = "siegler-wolf.de/cloud";
	uname = "padi";
#else
	cout << "Server url: ";
	cin >> url;
	cout << "User Name: ";
	cin >> uname;
#endif
	cout << "Password: ";
	SetStdinEcho(false);
	cin >> passwd; 
	SetStdinEcho(true);
	cout << endl;

	rc = srv_open(url, "trustCA.crt", uname, passwd, &basedate, &uid, false);
	MC_CHKERR(rc);
	
	// list syncs
	list<mc_sync> srvsyncs;// = list<mc_sync>();
	rc = srv_listsyncs(&srvsyncs);
	MC_CHKERR(rc);

	for (mc_sync& s : srvsyncs) {
		cout << s.id << " " << s.name << endl;
	}
	
	// choose sync and get key
	string idstr;
	int id;
#ifdef HARDCODED
	idstr = "10";
#else
	cout << "Sync ID: ";
	cin >> idstr;
#endif;
	id = std::stoi(idstr);

	mc_sync_db sync;
	sync.id = 0;
	for (mc_sync& s : srvsyncs) {
		if (s.id == id) {
			sync.id = id;
			sync.crypted = s.crypted;
			memcpy(sync.hash, s.hash, 16);
			sync.lastsync = 0;
			sync.name = s.name;

			break;
		}
	}
	if (sync.id == 0) {
		cout << "Sync not found" << endl;
		return 1;
	}
	string keystr;
	if (sync.crypted) {
#ifdef HARDCODED
		keystr = "92aa88fdd09130855407c812e34ae5cceba770d53881433c9ae7306157548eb4";
#else
		cout << "Key (hex): ";
		cin >> keystr;
#endif;
		QByteArray ckey = QByteArray::fromHex(keystr.c_str());
		memcpy(sync.cryptkey, ckey.constData(), 32);
	}

	mc_sync_ctx ctx;
	init_sync_ctx(&ctx, &sync, NULL);
	string path = "";
	int folderid = -id;
	string filename;
	mc_file selected_file;

	while (true) {
		// list files
		if (path.size()) path.append("/");
		list<mc_file> onsrv;
		rc = srv_listdir(&onsrv, folderid);
		MC_CHKERR(rc);

		rc = crypt_filelist_fromsrv(&ctx, path, &onsrv);
		MC_CHKERR(rc);
		onsrv.sort(compare_mc_file);

		for (mc_file& f : onsrv) {
			cout << f.id << " " << f.name << endl;
		}
//#ifdef HARDCODED
//		idstr = "29312";
//#else
		cout << "File ID: ";
		cin >> idstr;
//#endif
		id = std::stoi(idstr);

		mc_file* file = 0;
		for (mc_file& f : onsrv) {
			if (f.id == id) {
				file = &f;
				break;
			}
		}
		if (file == NULL) {
			cout << "File not found" << endl;
			return 1;
		}

		if (!file->is_dir) {
			selected_file.id = file->id;
			selected_file.parent = file->parent;
			selected_file.is_dir = false;
			selected_file.name = file->name;
			selected_file.cryptname = file->cryptname;
			selected_file.ctime = file->ctime;
			selected_file.mtime = file->mtime;
			selected_file.size = file->size;
			selected_file.status = file->status;
			memcpy(selected_file.hash, file->hash, 16);
			break;
		} else {
			folderid = file->id;
			path += file->name;
		}
	}

	// now we have a file!
	mc_crypt_ctx cctx;
	init_crypt_ctx(&cctx, &ctx);

	if (fs_exists(selected_file.name)) {
		init_crypt_ctx(&cctx, &ctx);

		rc = crypt_initresume_down(&cctx, &selected_file);
		MC_CHKERR_DOWN(rc, &cctx);

		MC_DBG("Opening file " << selected_file.name << " for writing");
		MC_NOTIFYSTART(MC_NT_DL, selected_file.name);
		FILE* fdesc = fs_fopen(selected_file.name, MC_FA_READWRITEEXISTING);
		if (!fdesc) {
			crypt_abort_download(&cctx);
			MC_CHKERR_MSG(MC_ERR_IO, "Could not write the file");
		}

		fs_fseek(fdesc, 0, SEEK_END);
		int64 offset = fs_ftell(fdesc);

		rc = crypt_resumetopos_down(&cctx, &selected_file, offset, fdesc);
		MC_CHKERR_FD_DOWN(rc, fdesc, &cctx);

		int64 written;
		while (offset < selected_file.size) {
			MC_NOTIFYPROGRESS(offset, selected_file.size);
			MC_CHECKTERMINATING_FD_DOWN(fdesc, &cctx);

			int i = 0;
			rc = 1;
			while (rc) {
				rc = crypt_getfile(&cctx, selected_file.id, offset, MC_RECVBLOCKSIZE, fdesc, &written, selected_file.hash);
				i++;
				if (i > 10) break;
			}
			MC_CHKERR_FD_DOWN(rc, fdesc, &cctx);
			offset += written;
		}

		rc = crypt_finish_download(&cctx);
		MC_CHKERR_FD(rc, fdesc);

		fs_fclose(fdesc);
		MC_NOTIFYEND(MC_NT_DL);
	}
	else {
		MC_BEGIN_WRITING_FILE(selected_file.name);
		MC_DBGL("Opening file " << selected_file.name << " for writing");
		MC_NOTIFYSTART(MC_NT_DL, selected_file.name);
		FILE* fdesc = fs_fopen(selected_file.name, MC_FA_OVERWRITECREATE);
		if (!fdesc) MC_CHKERR_MSG(MC_ERR_IO, "Could not (re)write the file");
		rc = download_actual(&cctx, selected_file.name, &selected_file, fdesc);
		MC_CHKERR(rc);

		fs_fclose(fdesc);
		MC_NOTIFYEND(MC_NT_DL);
		MC_END_WRITING_FILE(selected_file.name);
		MC_CHKERR(rc);
	}

	rc = fs_touch(selected_file.name, selected_file.mtime, selected_file.ctime);
	MC_CHKERR(rc);

	cout << "done" << endl;

	srv_close();
	getchar();
	getchar();
	return 0;
	//return a.exec();
}
