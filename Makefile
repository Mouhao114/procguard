CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wshadow
TARGET    = procguard
SRC       = src/procguard.cpp
CRASHER   = crasher
HTTP      = http_smoke

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# 测试靶子：一个会随机崩溃的进程，用来喂给 procguard
$(CRASHER): test/crasher.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# http.cpp 的独立验靶：起一个真接口，浏览器和 curl 都能打。
# 还没接进主循环，所以单独一个目标，不影响 procguard 的构建。
#   make http-smoke && ./http_smoke   →  http://localhost:8099/
$(HTTP): src/http.cpp test/http_smoke.cpp src/http.h src/status.h
	$(CXX) $(CXXFLAGS) -o $@ src/http.cpp test/http_smoke.cpp

http-smoke: $(HTTP)

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
	rm -f $(TARGET) $(CRASHER) $(HTTP)

.PHONY: all debug release run fmt clean $(CRASHER) $(HTTP) http-smoke demo
