#!/usr/bin/perl

$build = $ENV{'BNumber'};
$version = "0.6.2.$build";

open(CMD, "> AMDVersion.cmd") or die "can't open $newfile: $!";
print(CMD "SET PKG_VERSION=$version\n");
print(CMD "SET LABEL=$version\n");
print(CMD "SET BuildNumber=$build\n");
close(CMD);
print "BuildVersionString: Dropped AMDVersion.cmd\n"; 
print("BuildVersionString: Setup Version will be $version\n");
