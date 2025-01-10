#ifndef SQLITE_DATABASE_H
#define SQLITE_DATABASE_H
#include "../database.h"
#include "sqlite3.h" 

typedef int (*SqliteExecuteQueryCallback)(void*,int,char**,char**);

class SqliteDatabase : public Database
{
    public:
        SqliteDatabase(const char* path) : Database(path) { m_db=nullptr; };
        virtual bool open();
        virtual void close();
        bool isOpen() {return m_db != nullptr && m_db_status == SQLITE_OK;}
    protected:
        bool executeSQLQuery(const char* query, SqliteExecuteQueryCallback callback);
    private:
        
        sqlite3 *m_db;
        int m_db_status;
};
#endif //SQLITE_DATABASE_H