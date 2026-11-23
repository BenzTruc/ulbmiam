#!/bin/bash
echo "Bienvenu dans l'afficheur de recettes !"
sleep 5

precedent=""

while true; do
 actuel=$(ls $1)
 if [ "$actuel" != "$precedent" ]; then
    clear
    ls -d "$(realpath "$1")"/*
 fi
 precedent="$actuel"
 sleep 30
done