#!/usr/bin/perl

$build = $ENV{'BNumber'};

open(CMD, "> AMDVersion.cmd") or die "can't open $newfile: $!";
print(CMD "SET PKG_VERSION=1.0.0.$build\n");
print(CMD "SET LABEL=1.0.0.$build\n");
print(CMD "SET BuildNumber=$build\n");
close(CMD);
print "BuildVersionString: Dropped AMDVersion.cmd\n"; 
print("BuildVersionString: Setup Version will be 1.0.0.$build\n");
