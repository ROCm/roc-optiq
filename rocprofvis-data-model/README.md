To build library under Windows:
unzip src/database/sqlite3/sqlite3.zip
mkdir build
cd build
cmake ..
load rocprofvis-datamodel-lib.sln to Visual Studio 
build

To build library under Linux:
unzip src/database/sqlite3/sqlite3.zip
mkdir build
cd build
cmake ..
make 

To build library and test under Windows:
unzip src/database/sqlite3/sqlite3.zip
mkdir build
cd build
cmake -DTEST-ON ..
load rocprofvis-datamodel-lib.sln to Visual Studio 
build

To build library under Linux:
unzip src/database/sqlite3/sqlite3.zip
mkdir build
cd build
cmake -DTEST-ON ..
make 
