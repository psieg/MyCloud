#ifndef MC_H
#define MC_H
#include "mc_version.h"
//For portability we dont use _s fuctions
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#endif
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <list>
#include <errno.h>
using namespace std;

#define MC_WATCHMODE

#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__BORLANDC__)
#	define MC_OS_WIN
#	undef MC_OS_UNIX
#else
#	undef MC_OS_WIN
#	define MC_OS_UNIX
#endif

#ifdef _DEBUG
#	define MC_DEBUGLOW
#	define MC_DEBUG
#	define MC_INFOLOW
#	define MC_INFO
#	define MC_WARN
#	define MC_ERROR
#	define MC_LOGFILE
#	define MC_IONOTIFY
#else
#	define MC_INFOLOW
#	define MC_INFO
#	define MC_WARN
#	define MC_ERROR
#	define MC_LOGFILE
#	define MC_IONOTIFY
#endif

#ifdef MC_LOGFILE
extern ofstream mc_logfile;
#endif

#ifdef MC_DEBUG
#	ifdef MC_OS_WIN
#		define MC_DBG_BRK(cond, msg)	{ if(cond) { std::cout << msg << endl; DebugBreak(); } }
#	else
#		define MC_DBG_BRK(cond, msg)	{ if(cond) { std::cout << msg << endl; raise(SIGINT); } }
#	endif
#else
#	define MC_DBG_BRK(cond, msg)
#endif


#ifdef MC_DEBUGLOW
#	ifdef MC_LOGFILE
#		define MC_DBGL(msg)				{ /*std::cout << " L " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;*/	\
											mc_logfile << " L " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;	}
#	else
#		define MC_DBGL(msg)				{ /*std::cout << " L " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;*/ }
#	endif
#else
#	define MC_DBGL(msg)					{}
#endif
#ifdef MC_DEBUG
#	ifdef MC_LOGFILE
#		define MC_DBG(msg)				{ /*std::cout << " D " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;*/	\
											mc_logfile << " D " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;}
#	else
#		define MC_DBG(msg)				{ std::cout << " D " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;	}
#	endif
#else
#	define MC_DBG(msg)					{}
#endif
#ifdef MC_INFOLOW
#	ifdef MC_LOGFILE
#		define MC_INFL(msg)				{ std::cout << "L  " << msg << std::endl;	\
											mc_logfile << "L  " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;	}
#	else
#		define MC_INFL(msg)				{ std::cout << "L  " << msg << std::endl; }
#	endif
#else
#	define MC_INFL(msg)					{}
#endif
#ifdef MC_INFO
#	ifdef MC_LOGFILE
#		define MC_INF(msg)				{ std::cout << "I  " << msg << std::endl;	\
											mc_logfile << "I  " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;	}
#	else
#		define MC_INF(msg)				{ std::cout << "I  " << msg << std::endl; }
#	endif
#else
#	define MC_INF(msg)					{}
#endif
#ifdef MC_WARN
#	ifdef MC_QTCLIENT
#		ifdef MC_LOGFILE
#			define MC_WRN(msg)			{ std::cout << "<b>WW " << msg << "</b>" << std::endl;	\
											mc_logfile << "WW " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;	}
#		else
#			define MC_WRN(msg)			{ std::cerr << "<b>WW " << msg << "</b>" << std::endl; }
#		endif
#	else
#		ifdef MC_LOGFILE
#			define MC_WRN(msg)			{ std::cout << "WW " << msg << std::endl;	\
											mc_logfile << "WW " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << std::endl;	}
#		else
#			define MC_WRN(msg)			{ std::cerr << "WW " << msg << std::endl; }
#		endif
#	endif
#else
#	define MC_WRN(msg)					{}
#endif
#ifdef MC_ERROR
#	ifdef MC_QTCLIENT
#		ifdef MC_LOGFILE
			/* displays msg and returns rc */
#			define MC_ERR_MSG(rc, msg)		{ std::cerr << "<b><u>EE " << msg << " (Code " << rc << ")</u></b>" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << ")" << std::endl; return rc; }
			/* closes fd, displays msg and returns rc */
#			define MC_ERR_MSG_FD(rc, fd, msg)	{ if (fd) fs_fclose(fd); std::cerr << "<b><u>EE " << msg << " (Code " << rc << ")</u></b>" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << ")" << std::endl; return rc; }
			/* Checks rc for true, otherwise displays msg and returns rc */
#			define MC_CHKERR_MSG(rc, msg)	{ if (rc) { std::cerr << "<b><u>EE " << msg << " (Code " << rc << ")</u></b>" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << ")" << std::endl; return rc; } }
			/* Checks rc for exp, otherwise displays msg and returns rc */
#			define MC_CHKERR_EXP(rc, exp, msg) { if (rc != exp) { std::cerr << "<b><u>EE " << msg << " (Code " << rc << "!=" << exp << ")</u></b>" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << "!=" << exp << ")" << std::endl; return rc; } }
			/* Checks wether ptr is allocated, fails otherwise */
#			define MC_MEM(ptr, size)			{ if (!ptr) { std::cerr << "<b><u>MM " << "(failed to allocate " << size << " bytes</u></b>" << std::endl;	\
												mc_logfile << "MM " << __FUNCTION__ << "(" << __LINE__ << ") failed to allocate " << size << " bytes" << std::endl; throw ENOMEM; } }
#		else
			/* displays msg and returns rc */
#			define MC_ERR_MSG(rc, msg)		{ std::cerr << "<b><u>EE " << msg << " (Code " << rc << ")</u></b>" << std::endl; return rc; }
			/* closes fd, displays msg and returns rc */
#			define MC_ERR_MSG_FD(rc, fd, msg)	{ if (fd) fs_fclose(fd); std::cerr << "<b><u>EE " << msg << " (Code " << rc << ")</u></b>" << std::endl; return rc; }
			/* Checks rc for true, otherwise displays msg and returns rc */
#			define MC_CHKERR_MSG(rc, msg)	{ if (rc) { std::cerr << "<b><u>EE " << msg << " (Code " << rc << ")</u></b>" << std::endl; return rc; } }
			/* Checks rc for exp, otherwise displays msg and returns rc */
#			define MC_CHKERR_EXP(rc, exp, msg) { if (rc != exp) { std::cerr << "<b><u>EE " << msg << " (Code " << rc << "!=" << exp << ")</u></b>" << std::endl; return rc; } }
			/* Checks wether ptr is allocated, fails otherwise */
#			define MC_MEM(ptr, size)			{ if (!ptr) { std::cerr << "<b><u>MM " << "failed to allocate " << size << " bytes</u></b>" << std::endl; throw ENOMEM; } }
#		endif
#	else
#		ifdef MC_LOGFILE
			/* displays msg and returns rc */
#			define MC_ERR_MSG(rc, msg)		{ std::cerr << "EE " << msg << " (Code " << rc << ")" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << ")" << std::endl; return rc; }
			/* closes fd, displays msg and returns rc */
#			define MC_ERR_MSG_FD(rc, fd, msg)	{ if (fd) fs_fclose(fd); std::cerr << "EE " << msg << " (Code " << rc << ")" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << ")" << std::endl; return rc; }
			/* Checks rc for true, otherwise displays msg and returns rc */
#			define MC_CHKERR_MSG(rc, msg)	{ if (rc) { std::cerr << "EE " << msg << " (Code " << rc << ")" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << ")" << std::endl; return rc; } }
			/* Checks rc for exp, otherwise displays msg and returns rc */
#			define MC_CHKERR_EXP(rc, exp, msg) { if (rc != exp) { std::cerr << "EE " << msg << " (Code " << rc << "!=" << exp << ")" << std::endl;	\
												mc_logfile << "EE " << __FUNCTION__ << "(" << __LINE__ << ") " << msg << " (Code " << rc << "!=" << exp << ")" << std::endl; return rc; } }
			/* Checks wether ptr is allocated, fails otherwise */
#			define MC_MEM(ptr, size)			{ if (!ptr) { std::cerr << "MM " << "failed to allocate " << size << " bytes" << std::endl; \
												mc_logfile << "MM " << __FUNCTION__ << "(" << __LINE__ << ") failed to allocate " << size << " bytes" << std::endl; throw ENOMEM; } }
#		else
			/* displays msg and returns rc */
#			define MC_ERR_MSG(rc, msg)		{ std::cerr << "EE " << msg << " (Code " << rc << ")" << std::endl; return rc; }
			/* closes fd, displays msg and returns rc */
#			define MC_ERR_MSG_FD(rc, fd, msg)	{ if (fd) fs_fclose(fd); std::cerr << "EE " << msg << " (Code " << rc << ")" << std::endl; return rc; }
			/* Checks rc for true, otherwise displays msg and returns rc */
#			define MC_CHKERR_MSG(rc, msg)	{ if (rc) { std::cerr << "EE " << msg << " (Code " << rc << ")" << std::endl; return rc; } }
			/* Checks rc for exp, otherwise displays msg and returns rc */
#			define MC_CHKERR_EXP(rc, exp, msg) { if (rc != exp) { std::cerr << "EE " << msg << " (Code " << rc << "!=" << exp << ")" << std::endl; return rc; } }
			/* Checks wether ptr is allocated, fails otherwise */
#			define MC_MEM(ptr, size)			{ if (!ptr) { std::cerr << "MM " << "failed to allocate " << size << " bytes" << std::endl; throw ENOMEM; } }
#		endif
#	endif
#else
#	define MC_ERR_MSG(rc, msg)		{ return rc; }
#	define MC_ERR_MSG_FD(rc, fd, msg)	{ if (fd) fclose(fd); return rc; }
#	define MC_CHKERR_MSG(rc, msg)	{ MC_CHKERR(rc); }
#	define MC_CHKERR_EXP(rc, exp, msg) { if (rc != exp) return rc; }
#	define MC_MEM(ptr, size)			{ throw ENOMEM; }
#endif

#define MC_CHKERR(rc)					{ if (rc) return rc; }
#define MC_CHKERR_FD(rc, fd)				{ if (rc) { if (fd) fs_fclose(fd); return rc; } }
#define MC_CHKERR_UP(rc, cctx)			{ if (rc) { crypt_abort_upload(cctx); return rc; } }
#define MC_CHKERR_DOWN(rc, cctx)			{ if (rc) { crypt_abort_download(cctx); return rc; } }
#define MC_CHKERR_FD(rc, fd)				{ if (rc) { if (fd) fs_fclose(fd); return rc; } }
#define MC_CHKERR_FD_UP(rc, fd, cctx)		{ if (rc) { crypt_abort_upload(cctx); if (fd) fs_fclose(fd); return rc; } }
#define MC_CHKERR_FD_DOWN(rc, fd, cctx)	{ if (rc) { crypt_abort_download(cctx); if (fd) fs_fclose(fd); return rc; } }


/* return wether the WorkerThreads terminating-indicator is set */
#define MC_TERMINATING()				QtWorkerThread::instance()->terminating
/* if the terminating indicator is set, return MC_ERR_TERMINATING */
#define MC_CHECKTERMINATING()			if (QtWorkerThread::instance()->terminating) MC_ERR_MSG(MC_ERR_TERMINATING, "Aborting");
/* if the terminating indicator is set, return MC_ERR_TERMINATING */
#define MC_CHECKTERMINATING_UP(cctx)			if (QtWorkerThread::instance()->terminating) { crypt_abort_upload(cctx); MC_ERR_MSG(MC_ERR_TERMINATING, "Aborting"); }
/* if the terminating indicator is set, return MC_ERR_TERMINATING */
#define MC_CHECKTERMINATING_DOWN(cctx)			if (QtWorkerThread::instance()->terminating) { crypt_abort_download(cctx); MC_ERR_MSG(MC_ERR_TERMINATING, "Aborting"); }
/* if the terminating indicator is set, close fd and return MC_ERR_TERMINATING */
#define MC_CHECKTERMINATING_FD(fd)		if (QtWorkerThread::instance()->terminating) { if (fd) fclose(fd); MC_ERR_MSG(MC_ERR_TERMINATING, "Aborting"); }
/* if the terminating indicator is set, close fd and return MC_ERR_TERMINATING */
#define MC_CHECKTERMINATING_FD_UP(fd, cctx)		if (QtWorkerThread::instance()->terminating) { crypt_abort_upload(cctx); if (fd) fclose(fd); MC_ERR_MSG(MC_ERR_TERMINATING, "Aborting"); }
/* if the terminating indicator is set, close fd and return MC_ERR_TERMINATING */
#define MC_CHECKTERMINATING_FD_DOWN(fd, cctx)		if (QtWorkerThread::instance()->terminating) { crypt_abort_download(cctx); if (fd) fclose(fd); MC_ERR_MSG(MC_ERR_TERMINATING, "Aborting"); }


#ifdef MC_QTCLIENT
	/* notify the UI thread of these events */
#	define MC_NOTIFY(evt, param)			QtClient::instance()->notify(evt, param);
#	define MC_NOTIFYSTART(evt, param)		QtClient::instance()->notifyStart(evt, param);
#	define MC_NOTIFYEND(evt)				QtClient::instance()->notifyEnd(evt);
#	define MC_NOTIFYPROGRESS(val, tot)		QtClient::instance()->notifyProgress(val, tot);
#	define MC_NOTIFYSUBPROGRESS(dl, ul)		QtClient::instance()->notifySubProgress(dl, ul);
#	ifdef MC_IONOTIFY
#		define MC_NOTIFYIOSTART(evt)		QtClient::instance()->notifyIOStart(evt);
#		define MC_NOTIFYIOEND(evt)			QtClient::instance()->notifyIOEnd(evt);
#	else
#		define MC_NOTIFYIOSTART(evt)
#		define MC_NOTIFYIOEND(evt)
#	endif
#else
#	define MC_NOTIFY(evt, param)
#	define MC_NOTIFYSTART(evt, param)
#	define MC_NOTIFYEND(evt)
#	define MC_NOTIFYPROGRESS(val, tot)
#	define MC_NOTIFYSUBPROGRESS(dl, ul)
#	define MC_NOTIFYIOSTART(evt)
#	define MC_NOTIFYIOEND(evt)
#endif

#include "mc_types.h"

#endif /* MC_H */