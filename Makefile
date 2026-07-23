CXX = g++
CXXFLAGS = -std=c++17 -O3
TARGET = codectx

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp