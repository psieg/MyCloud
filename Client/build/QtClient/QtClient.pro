TEMPLATE = app
TARGET = QtClient
QT += core gui
QT += widgets
QT += network
LIBS += -lssl -lcrypto	#openssl
LIBS += -ldl #sqlite-deps
QMAKE_CXXFLAGS += -std=c++11 -DMC_QTCLIENT
MOC_DIR = GeneratedFiles/build
UI_DIR = GeneratedFiles
RCC_DIR = GeneratedFiles
OBJECTS_DIR = build
DESTDIR = ../bin 

INCLUDEPATH += . \
               ../../src/mycloud \
               ../../src/sqlite-amalgamation \
               ../../src/QtClient \

# Input
HEADERS += ../../src/mycloud/Client.h \
           ../../src/mycloud/mc.h \
           ../../src/mycloud/mc_crypt.h \
           ../../src/mycloud/mc_db.h \
           ../../src/mycloud/mc_filter.h \
           ../../src/mycloud/mc_fs.h \
           ../../src/mycloud/mc_helpers.h \
           ../../src/mycloud/mc_protocol.h \
           ../../src/mycloud/mc_srv.h \
           ../../src/mycloud/mc_transfer.h \
           ../../src/mycloud/mc_types.h \
           ../../src/mycloud/mc_util.h \
           ../../src/mycloud/mc_version.h \
           ../../src/mycloud/mc_walk.h \
           ../../src/mycloud/mc_watch.h \
           ../../src/mycloud/mc_workerthread.h \
           ../../src/QtClient/qdebugstream.h \
           ../../src/QtClient/qtClient.h \
           ../../src/QtClient/qtConflictDialog.h \
           ../../src/QtClient/qtFilterDialog.h \
           ../../src/QtClient/qtGeneralFilterDialog.h \
           ../../src/QtClient/qtPasswordChangeDialog.h \
           ../../src/QtClient/qtSettingsDialog.h \
           ../../src/QtClient/qtSyncDialog.h \
           ../../src/QtClient/qtUpdateChecker.h \
           ../../src/sqlite-amalgamation/sqlite3.h
FORMS += ../../src/QtClient/qtClient.ui \
         ../../src/QtClient/qtConflictDialog.ui \
         ../../src/QtClient/qtFilterDialog.ui \
         ../../src/QtClient/qtGeneralFilterDialog.ui \
         ../../src/QtClient/qtSettingsDialog.ui \
         ../../src/QtClient/qtSyncDialog.ui
RESOURCES += ../../src/QtClient/qtClient.qrc
SOURCES += ../../src/mycloud/Client.cpp \
           ../../src/mycloud/mc_crypt.cpp \
           ../../src/mycloud/mc_db.cpp \
           ../../src/mycloud/mc_filter.cpp \
           ../../src/mycloud/mc_fs.cpp \
           ../../src/mycloud/mc_helpers.cpp \
           ../../src/mycloud/mc_protocol.cpp \
           ../../src/mycloud/mc_srv.cpp \
           ../../src/mycloud/mc_transfer.cpp \
           ../../src/mycloud/mc_util.cpp \
           ../../src/mycloud/mc_walk.cpp \
           ../../src/mycloud/mc_watch.cpp \
           ../../src/mycloud/mc_workerthread.cpp \
           ../../src/QtClient/main.cpp \
           ../../src/QtClient/qtClient.cpp \
           ../../src/QtClient/qtConflictDialog.cpp \
           ../../src/QtClient/qtFilterDialog.cpp \
           ../../src/QtClient/qtGeneralFilterDialog.cpp \
           ../../src/QtClient/qtPasswordChangeDialog.cpp \
           ../../src/QtClient/qtSettingsDialog.cpp \
           ../../src/QtClient/qtSyncDialog.cpp \
           ../../src/QtClient/qtUpdateChecker.cpp \
           ../../src/sqlite-amalgamation/sqlite3.c
