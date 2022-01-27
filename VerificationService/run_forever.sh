#!/bin/bash
while true; do sudo ./VerifyServer; echo -n "Crashed" >> info.txt; date >> info.txt; done
