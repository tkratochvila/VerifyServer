#!/bin/bash

while read line ;
do
  if [[ $line == *"error found: no"* ]] ; then
    echo "TRUE"; exit;
  fi
  if [[ $line == *"Assertion failed"* ]] ; then
    echo "FALSE_REACH"; exit;
  fi
  if [[ $line == *"E: resource exhausted: time limit"* ]] ; then
    echo "TIMEOUT"; exit;
  fi
done < <(cat "$1"; cat "$2")

echo "ERROR";
