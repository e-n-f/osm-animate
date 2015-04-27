#!/bin/sh

bzcat $1.osm.bz2 | ./snap > $1.snap
cat $1.snap | egrep '(natural|building|highway)' | /data2/data/github/datamaps/encode -z20 -m32 -o $1.dm


