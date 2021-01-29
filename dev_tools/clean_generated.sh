#!/bin/bash
# deletes all Lyo generated files
# except the AdapterManager as that is the one with manual changes

cd "${BASH_SOURCE%/*}"

rm -r ../adapter/src/main/webapp/static
rm -r ../adapter/src/main/webapp/hon
rm -r ../adapter/src/main/webapp/delegatedUI.js
rm -r ../adapter/src/main/webapp/index.jsp

rm -r ../adapter/src/main/java/org

rm -r ../adapter/src/main/java/hon/oslc/automation/resources
rm -r ../adapter/src/main/java/hon/oslc/automation/services
rm -r ../adapter/src/main/java/hon/oslc/automation/servlet
rm -r ../adapter/src/main/java/hon/oslc/automation/*ProviderInfo.java
