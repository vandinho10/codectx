CXX      := g++
STD      := -std=c++17
WARN     := -Wall -Wextra -Wpedantic
OPTFLAGS := -O2
DBGFLAGS := -O0 -g

VERSION  := 1.3.0
TARGET   := codectx
TESTBIN  := tests/test_runner
TESTBIN_SAN := tests/test_runner_san

# Alvo do sistema: nativo por padrao; TARGET_OS=Windows_NT com mingw-w64
# (x86_64-w64-mingw32-g++ / i686-w64-mingw32-g++) gera binario 100% estatico.
TARGET_OS ?= $(shell uname -s)
ifeq ($(TARGET_OS),Windows_NT)
  WINLIBS := -static -static-libgcc -static-libstdc++
endif

CXXFLAGS := $(STD) $(WARN)

SRCS := main.cpp codectx.cpp config.cpp

.PHONY: all check test sanitize version install clean

all: $(TARGET)

$(TARGET): $(SRCS) codectx.hpp
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -DCODECTX_VERSION=\"$(VERSION)\" -o $@ $(SRCS) $(WINLIBS)

# Verificacao estatica: warnings tratados como erro
check:
	$(CXX) $(CXXFLAGS) -Werror -fsyntax-only $(SRCS)
	@echo "verificacao ok (zero warnings)"

$(TESTBIN): tests/codectx_tests.cpp codectx.cpp config.cpp codectx.hpp tests/test_framework.hpp
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -DCODECTX_VERSION=\"$(VERSION)\" -o $@ tests/codectx_tests.cpp codectx.cpp config.cpp

test: $(TESTBIN)
	./$(TESTBIN)

# Build com sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer)
$(TESTBIN_SAN): tests/codectx_tests.cpp codectx.cpp config.cpp codectx.hpp tests/test_framework.hpp
	$(CXX) $(CXXFLAGS) $(DBGFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -DCODECTX_VERSION=\"$(VERSION)\" -o $@ tests/codectx_tests.cpp codectx.cpp config.cpp

sanitize: $(TESTBIN_SAN)
	./$(TESTBIN_SAN)

version:
	@echo "$(TARGET) v$(VERSION)"

install: $(TARGET)
	install -m 0755 $(TARGET) /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(TESTBIN) $(TESTBIN_SAN)
	rm -rf codectx_test_tmp_*
