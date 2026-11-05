CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wshadow
TARGET    = procguard
SRC       = src/procguard.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

debug:
	$(CXX) $(CXXFLAGS) -g -O0 -o $(TARGET) $(SRC)

release:
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

fmt:
	clang-format -i $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all debug release run fmt clean
