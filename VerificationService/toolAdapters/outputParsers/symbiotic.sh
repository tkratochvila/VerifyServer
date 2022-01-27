#!/bin/bash

while read line ;
do
  if [[ $line == *"RESULT: true"* ]] ; then
    echo "TRUE"; exit;
  fi
  if [[ $line == *"RESULT: unknown"* ]] ; then
    echo "UNKNOWN"; exit;
  fi
  if [[ $line == *"RESULT: done"* ]] ; then
    echo "DONE"; exit;
  fi
  if [[ $line == *"RESULT: false(valid-deref)"* ]] ; then
    echo "FALSE_DEREF"; exit;
  fi
  if [[ $line == *"RESULT: false(valid-free)"* ]] ; then
    echo "FALSE_FREE"; exit;
  fi
  if [[ $line == *"RESULT: false(valid-memtrack)"* ]] ; then
    echo "FALSE_MEMTRACK"; exit;
  fi
  if [[ $line == *"RESULT: false(valid-memcleanup)"* ]] ; then
    echo "FALSE_MEMCLEANUP"; exit;
  fi
  if [[ $line == *"false(no-overflow)"* ]] ; then
    echo "FALSE_OVERFLOW"; exit;
  fi
  if [[ $line == *"RESULT: false(termination)"* ]] ; then
    echo "FALSE_TERMINATION"; exit;
  fi
  if [[ $line == *"RESULT: false"* ]] ; then
    echo "FALSE_REACH"; exit;
  fi
  if [[ $line == *"RESULT: timeout"* ]] ; then
    echo "TIMEOUT"; exit;
  fi
done < <(cat "$1"; cat "$2")

echo "ERROR";
