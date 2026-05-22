ifeq ($(OS), Windows_NT)
    TARGET = mapp.exe
else
    TARGET = mapp
endif

CXX     = g++
CXXFLAGS = -Wall -Wextra

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp

clean:
	rm -f $(TARGET)
