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

**To build library and test under Windows:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake -DTEST=ON ..<br />
load rocprofvis-datamodel-lib.sln to Visual Studio <br />
build<br />

**To build library under Linux:**<br />
unzip src/database/sqlite3/sqlite3.zip -d src/database/sqlite3/<br />
mkdir build<br />
cd build<br />
cmake -DTEST=ON ..<br />
make <br />
