MC_H = ../mycloud/*.h
MC_SRC = ../mycloud/*.cpp
CIENT_H = ../Client/*.h
CLIENT_SRC = ../Client/*.cpp
QTCIENT_H = ../QtClient/*.h
QTCLIENT_SRC = ../QtClient/*.cpp
SQLITE_H = ../sqlite-amalgamation/sqlite3.h
SQLITE_C = ../sqlite-amalgamation/sqlite3.c

CLIENT_FLAGS =
QTCLIENT_FLAGS = -DMC_QTCLIENT
CC = g++ -std=c++11

all: Client QtClient

Client: $(MC_H) $(MC_SRC) $(SQLITE_H) $(SQLITE_SRC) $(CLIENT_H) $(CLIENT_SRC)
	$(CC) $(CLIENT_FLAGS) $(MC_SRC) $(SQLITE_SRC) $(CLIENT_SRC) -o Client

QtClient: $(MC_H) $(MC_SRC) $(SQLITE_H) $(SQLITE_SRC) $(QTCLIENT_H) $(QTCLIENT_SRC)
	$(CC) $(QTCLIENT_FLAGS) $(MC_SRC) $(SQLITE_SRC) $(QTCLIENT_SRC) -o QtClient

clean:
	rm -f Client
	rm -f QtClient
