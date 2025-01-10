#include "SqliteDatabase.h"


bool SqliteDatabase::open()
{
    m_db_status = sqlite3_open(m_path.c_str(), &m_db);
    if( m_db_status != SQLITE_OK ) {

        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(m_db));
        return false;
    }
    return true;
}

void SqliteDatabase::close()
{
    if (isOpen())
        sqlite3_close(m_db);
}


bool SqliteDatabase::executeSQLQuery(const char* query, SqliteExecuteQueryCallback callback)
{
    int rc;
    char *zErrMsg = 0;
    if (!isOpen()) return false;

    rc = sqlite3_exec(m_db, query, callback, this, &zErrMsg);
    if( rc != SQLITE_OK ) {
        fprintf(stderr, "Query: %s\n", query);
        fprintf(stderr, "SQL error %d: %s\n",rc, zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    } 
    return true;
}