TEMPLATE = app
TARGET = IntegrationTest
QT += core
QT += network
QT += testlib
LIBS += -lssl -lcrypto	#openssl
LIBS += -ldl #sqlite-deps
QMAKE_CXXFLAGS += -std=c++11
MOC_DIR = GeneratedFiles/build
UI_DIR = GeneratedFiles
RCC_DIR = GeneratedFiles
OBJECTS_DIR = build
DESTDIR = ../bin 

INCLUDEPATH += . \
               ../../src/mycloud \
               ../../src/sqlite-amalgamation \
               ../../src/IntegrationTest

# Input
HEADERS += ../../src/mycloud/Client.h \
           ../../src/mycloud/mc.h \
           ../../src/mycloud/mc_crypt.h \
           ../../src/mycloud/mc_conflict.h \
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
           ../../src/sqlite-amalgamation/sqlite3.h \
           ../../src/IntegrationTest/IntegrationTest.h
SOURCES += ../../src/mycloud/Client.cpp \
           ../../src/mycloud/mc_crypt.cpp \
           ../../src/mycloud/mc_conflict.cpp \
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
           ../../src/sqlite-amalgamation/sqlite3.c \
           ../../src/IntegrationTest/IntegrationTest.cpp \
           ../../src/IntegrationTest/main.cpp \
