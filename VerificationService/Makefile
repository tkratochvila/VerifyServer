#CC=clang++-6.0
CC=g++-7
CFLAGS=-std=c++17 -Og -g -c

SANITIZE_MEMORY = -fsanitize=memory -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize-memory-use-after-dtor -fsanitize-memory-track-origins
SANITIZE_ADDRESS = -fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer
SANITIZE_THREAD = -fsanitize=thread

CLANGFLAGS=-std=c++17 -pedantic -Wall -g -ferror-limit=3 -c
DFLAGS=-std=c++17 -pedantic -Wall -O2 -c
LDFS=-lstdc++fs -pthread -lfolly -lgflags -lglog -lproxygenhttpserver -lproxygenlib
#SOURCES=XMLSupport.cpp VerifyServer.cpp VerifyRequestHandler.cpp subprocess.cpp RequestResponse.cpp ExecutionEngine.cpp VerificationService.cpp Archive.cpp ToolKit.cpp ToolKitXMLFactory.cpp Workspace.cpp
#HEADERS=Archive.h bbb.h XMLSupport.h VerificationService.h FileSupport.h RequestResponse.h VerifyStats.h VerifyRequestHandler.h subprocess.hpp ExecutionEngine.h ToolKit.h ToolKitXMLFactory.h DataStore.h
EXCLUDE=vacuityChecker.cpp sanity_checker.cpp realisabilityChecker.cpp test.cpp sanity_support.cpp
SOURCES=$(filter-out $(EXCLUDE),$(wildcard *.cpp))
HEADERS=$(wildcard *.h) $(wildcard *.hpp)
OBJECTS=$(SOURCES:.cpp=.o)
EXE=VerifyServer

all: $(SOURCES) $(EXE) $(HEADERS) $(OBJECTS)

$(EXE) : $(OBJECTS) $(HEADERS) $(SOURCES)
	$(CC) $(OBJECTS) $(LDFS) -v  -o $@

.cpp.o: $(HEADERS) $(SOURCES)
	$(CC) $(DFLAGS) $< -o $@

clean:
	rm $(OBJECTS) $(EXE) || true
