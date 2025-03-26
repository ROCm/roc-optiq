The Data Model package include two components:<br />
    1. Data Model - layered data storage, with public interface to access data properties and possibility of freeing redundant objects. <br />
    2. Database - database query manager and processor, with public interface to add data model objects. Supports asynchronous database access.<br />
They are interconnected via binding interface<br />


**Building project:**<br />

**To build library under Windows:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake ..<br />
load rocprofvis-datamodel-lib.sln to Visual Studio <br />
build<br />

**To build library under Linux:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake ..<br />
make <br />

**To build library for testing under Windows:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake -DTEST=ON ..<br />
load rocprofvis-datamodel-lib.sln to Visual Studio <br />
build<br />

**To build library for testing under Linux:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake -DTEST=ON ..<br />
make <br />
