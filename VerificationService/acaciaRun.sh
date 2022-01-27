#!/bin/bash

cd /home/kratochvila/AcaciaPlus-v2.3/

echo $1
echo $2

python2 acacia_plus.py --ltl=/home/kratochvila/code/VerificationServer/$1 --part=/home/kratochvila/code/VerificationServer/$2 --player=1 --check=BOTH
