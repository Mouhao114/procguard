CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wshadow
TARGET    = procguard
SRC       = src/procguard.cpp
CRASHER   = crasher

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# 测试靶子：一个会随机崩溃的进程，用来喂给 procguard
$(CRASHER): test/crasher.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

demo: $(TARGET) $(CRASHER)
	./$(TARGET) ./$(CRASHER)

debug:
	$(CXX) $(CXXFLAGS) -g -O0 -o $(TARGET) $(SRC)

release:
	$(CXX) $(CXXFLAGS) -O2 -DNDEBUG -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

fmt:
	clang-format -i $(SRC)

clean:
	rm -f $(TARGET) $(CRASHER)

.PHONY: all debug release run fmt clean $(CRASHER) demo
