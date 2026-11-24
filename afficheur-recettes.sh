#!/bin/bash
precedent=""

while true; do
 actuel=$(ls "$1")
 if [ "$actuel" != "$precedent" ]; then
    ls -d "$(realpath "$1")"/*.csv
 fi
 precedent="$actuel"
 sleep 30
done