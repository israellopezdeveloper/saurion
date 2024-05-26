#!/bin/bash

nc localhost 2000 -w 1 | awk -F'\0' '{for(i=1; i<=NF; i++) print "Longitud del chunk " i ": " length($i)}'
