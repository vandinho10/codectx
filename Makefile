CXX      := g++
STD      := -std=c++17
WARN     := -Wall -Wextra -Wpedantic
OPTFLAGS := -O2
DBGFLAGS := -O0 -g

VERSION  := 1.0.0
TARGET   := codectx
TESTBIN  := tests/test_runner
TESTBIN_SAN := tests/test_runner_san

CXXFLAGS := $(STD) $(WARN)

.PHONY: all check test sanitize version install clean

all: $(TARGET)

$(TARGET): main.cpp codectx.cpp codectx.hpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -DCODECTX_VERSION=\"$(VERSION)\" -o $@ main.cpp codectx.cpp

# Verificacao estatica: warnings tratados como erro
check:
	$(CXX) $(CXXFLAGS) -Werror -fsyntax-only main.cpp codectx.cpp
	@echo "verificacao ok (zero warnings)"

$(TESTBIN): tests/codectx_tests.cpp codectx.cpp codectx.hpp tests/test_framework.hpp
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -DCODECTX_VERSION=\"$(VERSION)\" -o $@ tests/codectx_tests.cpp codectx.cpp

test: $(TESTBIN)
	./$(TESTBIN)

# Build com sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer)
$(TESTBIN_SAN): tests/codectx_tests.cpp codectx.cpp codectx.hpp tests/test_framework.hpp
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -DCODECTX_VERSION=\"$(VERSION)\" -o $@ tests/codectx_tests.cpp codectx.cpp

sanitize: $(TESTBIN_SAN)
	./$(TESTBIN_SAN)

version:
	@echo "$(TARGET) v$(VERSION)"

install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET) $(TESTBIN) $(TESTBIN_SAN)
	rm -rf codectx_test_tmp_*
