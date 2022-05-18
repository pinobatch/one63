#!/bin/sh
set -e
FAMITRACKER='/home/pino/.wine/drive_c/Program Files/FamiTracker/Dn-FamiTracker.exe'

wine "$FAMITRACKER" audio/parsertest.dnm -export parsertest.txt