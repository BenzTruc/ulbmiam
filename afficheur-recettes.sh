#!/bin/bash
if [ $# -ne 1 ] || [ ! -d "$1" ]; then
    echo "Usage: $0 DOSSIER" >&2
    exit 1
fi

dossier=$(realpath "$1")    
deja_vus=""                

while true; do
    for f in "$dossier"/*.csv; do
        [ -e "$f" ] || continue          

        if ! grep -Fxq -- "$f" <<< "$deja_vus"; then
            echo "$f"
            deja_vus+="$f"$'\n'
        fi
    done
    sleep 30
done