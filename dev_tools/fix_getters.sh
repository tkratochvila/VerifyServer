#!/bin/bash
# fixes a Lyo bug (probably a bug) - getters return a HashSet instead of a Set

cd "${BASH_SOURCE%/*}"

cd ../adapter/src/main/java/org/eclipse/lyo/oslc/domains
find . -type f -exec sed -i "s/public HashSet/public Set/g" {} \;
cd -
cd ../adapter/src/main/java/hon/oslc/automation/resources
find . -type f -exec sed -i "s/public HashSet/public Set/g" {} \;
