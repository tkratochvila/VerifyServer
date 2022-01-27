#!/bin/bash

pushd ./toolAdapters/outputParsers/
for script in `ls *.sh` ; do
	sed -i 's/^M$//' "$script"; # DOS-to-UNIX EOL
	chmod +x "$script" # Make executable
done
popd
