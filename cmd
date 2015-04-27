#!/bin/sh

gzcat $1.osm.gz | ./snap > $1.snap
cat $1.snap | egrep '(natural|building|highway)' | ../datamaps/encode -z20 -m32 -o $1.dm


