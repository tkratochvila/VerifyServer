#!/bin/bash

g++ -std=c++17 -g -pedantic -Wall -o ./sanity_checker sanity_checker.cpp realisabilityChecker.cpp subprocess.cpp -lpthread
sudo cp sanity_checker /var/www/sanityChecker
