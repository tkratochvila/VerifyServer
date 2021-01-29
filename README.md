
# OSLC Automation Adapter
- author: Ondrej Vasicek
- project: AUFOVER
- description: OSLC Verification Server based on LYO client developed by Ondrej Vasicek for Honeywell.

# How to build and run
First run or clean run:
```
$ cd adapter
$ mvn clean install jetty:run-exploded
```
Normal run:
```
$ cd adapter
$ mvn jetty:run-exploded
```

# How to configure
Use the adapter/adapter.properties file.

# Directory structure
- adapter - source of the adapter, oslc4j project
- adapter-model - model of the adapter, Lyo Designer modeling project
- dev\_tools - my development scripts
- lyo.domains - imported domain models (https://github.com/eclipse/lyo.domains)