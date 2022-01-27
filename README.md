
# OSLC Automation Adapter
- author: Ondrej Vasicek
- project: AUFOVER
- description: OSLC Verification Server based on LYO client developed by Ondrej Vasicek from Honeywell. Need to run on linux server with formal verification tools installed and server ports opened.

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
# Full deployment 
- Install DIVINE at least version 4.4 from: https://divine.fi.muni.cz/ 
- Install Symbiotic at least version 8 from: https://github.com/staticafi/symbiotic 
- Install Spectra: https://pajda.fit.vutbr.cz/testos/spectra
- Install ANaConDA: https://pajda.fit.vutbr.cz/anaconda/anaconda
- Optionaly install also CPACheck, cmbc, Infer or formal verification tools SW verification.
- Set all paths to all tools in the configuration.

![The integration of complete verification platform as deployed in Honeywell for both testing and production servers](https://github.com/tkratochvila/VerifyAll/blob/main/WebApp/Imgs/AUFOVER-Security.png?raw=true)

# How to configure
Use the adapter/adapter.properties file.

# Directory structure
- adapter - source of the adapter, oslc4j project
- adapter-model - model of the adapter, Lyo Designer modeling project
- dev\_tools - my development scripts
- lyo.domains - imported domain models (https://github.com/eclipse/lyo.domains)
