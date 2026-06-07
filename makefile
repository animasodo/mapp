ifeq ($(OS), Windows_NT)
    TARGET       = mapp.exe
    TARGET_LISTER = mapp-lister.exe
else
    TARGET       = mapp
    TARGET_LISTER = mapp-lister
endif

CXX      = g++
CXXFLAGS = -Wall -Wextra

all: $(TARGET) $(TARGET_LISTER)

$(TARGET): mapp.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) mapp.cpp

$(TARGET_LISTER): mapp-lister.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_LISTER) mapp-lister.cpp

clean:
	rm -f $(TARGET) $(TARGET_LISTER)
