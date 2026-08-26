TARGET          = mapp
TARGET_LISTER   = mapp-lister
DISK_LISTER     = disk-lister
PACK_ASSETS     = pack-assets
MANIFEST_GEN    = manifest-gen

CXX      = g++
CXXFLAGS = -Wall -Wextra

all: $(TARGET) $(TARGET_LISTER) $(DISK_LISTER) $(PACK_ASSETS) $(MANIFEST_GEN)

avail: all
	@mkdir -p ~/.local/bin
	@cp $(TARGET) ~/.local/bin/
	@cp $(TARGET_LISTER) ~/.local/bin/
	@cp $(DISK_LISTER) ~/.local/bin/
	@cp $(PACK_ASSETS) ~/.local/bin/
	@cp $(MANIFEST_GEN) ~/.local/bin/

$(TARGET): mapp.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) mapp.cpp

$(TARGET_LISTER): mapp-lister.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_LISTER) mapp-lister.cpp

$(DISK_LISTER): disk-lister.cpp
	$(CXX) $(CXXFLAGS) -o $(DISK_LISTER) disk-lister.cpp disk.cpp

$(PACK_ASSETS): pack-assets.cpp disk.cpp
	$(CXX) $(CXXFLAGS) -o $(PACK_ASSETS) pack-assets.cpp disk.cpp

$(MANIFEST_GEN): manifest-gen.cpp
	$(CXX) $(CXXFLAGS) -o $(MANIFEST_GEN) manifest-gen.cpp

clean:
	rm -f $(TARGET) $(TARGET_LISTER) $(DISK_LISTER) $(PACK_ASSETS) $(MANIFEST_GEN)
