#include "repository.h"


namespace storage {
    std::string sessions;
    int callback(void *NotUsed, int argc, char **argv, char **azColName){
        int i;
        sessions.append("{");
        for(i=0; i<argc; i++) {        
            sessions.append("\"");
            sessions.append(azColName[i]);
            sessions.append("\"");
            sessions.append(": ");
            sessions.append("\"");
            sessions.append(argv[i]);

            if(i + 1 == argc) {
                sessions.append("\"");
            }
            else {
                sessions.append("\",");
            }
        }
        sessions.append("},");

        return 0;
    }

    std::string getSessions()
    {
        sqlite3 *db; // sqlite3 db struct
        char *zErrMsg = 0;
        char *szSQL;
        int rc;

        // Open db
        rc = sqlite3_open("precedex_calc.sqlite", &db);

        if( rc ) {
            // failed
            // fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        } else {
            // success
            // fprintf(stderr, "Open database successfully\n");
        }

        // create myTable
        // szSQL = "create table myTable (FirstName varchar(30), LastName varchar(30), Age smallint not null)";

        // rc = sqlite3_exec(db, szSQL, callback, 0, &zErrMsg);

        if( rc == SQLITE_OK ) {
            // fetch records
            szSQL = "select * from sessions";
            rc = sqlite3_exec(db, szSQL, callback, 0, &zErrMsg);
        }

        // Close test.db file
        sqlite3_close(db);

        size_t found = sessions.find_last_of(",");
        if (found != std::string::npos)
            sessions.replace(sessions.find_last_of(","), 1, "");
        return sessions;
    }

    // Insert record
    void insertSession(char *name, char *status) {
      sqlite3 *db;

      // Open db
      int rc = sqlite3_open("precedex_calc.sqlite", &db);

      if( rc ) {
            // failed
            // fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        } else {
            // success
            // fprintf(stderr, "Open database successfully\n");
        }

      if (!db)
        return;

      char *zErrMsg = 0;
      sqlite3_stmt *stmt;
      const char *pzTest;
      char *szSQL;

      // Insert data item into myTable
      szSQL = "insert into sessions (name, status) values (?,?)";

      rc = sqlite3_prepare(db, szSQL, strlen(szSQL), &stmt, &pzTest);

      if( rc == SQLITE_OK ) {
        // bind the value 
        sqlite3_bind_text(stmt, 1, name, strlen(name), 0);
        sqlite3_bind_text(stmt, 2, status, strlen(status), 0);

        // commit 
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
      }

      // Close test.db file
      sqlite3_close(db);
    }
}