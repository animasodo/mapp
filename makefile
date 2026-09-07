BUILD_DIR		= build

MAPP          	= $(BUILD_DIR)/mapp
MAPP_LISTER   	= $(BUILD_DIR)/mapp-lister
DISK_LISTER     = $(BUILD_DIR)/disk-lister
PACK_ASSETS     = $(BUILD_DIR)/pack-assets
MANIFEST_GEN    = $(BUILD_DIR)/manifest-gen
BYTECODE_COMP	= $(BUILD_DIR)/bytecode-comp

CXX      = g++
CXXFLAGS = -Wall -Wextra

all: $(BUILD_DIR) $(MAPP) $(MAPP_LISTER) $(DISK_LISTER) $(PACK_ASSETS) $(MANIFEST_GEN) $(BYTECODE_COMP)

avail: all
	@mkdir -p ~/.local/bin
	@cp $(MAPP) ~/.local/bin/
	@cp $(MAPP_LISTER) ~/.local/bin/
	@cp $(DISK_LISTER) ~/.local/bin/
	@cp $(PACK_ASSETS) ~/.local/bin/
	@cp $(MANIFEST_GEN) ~/.local/bin/
	@cp $(BYTECODE_COMP) ~/.local/bin/

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(MAPP): mapp.cpp
	$(CXX) $(CXXFLAGS) -o $(MAPP) mapp.cpp

$(MAPP_LISTER): mapp-lister.cpp
	$(CXX) $(CXXFLAGS) -o $(MAPP_LISTER) mapp-lister.cpp

$(DISK_LISTER): disk-lister.cpp
	$(CXX) $(CXXFLAGS) -o $(DISK_LISTER) disk-lister.cpp disk.cpp

$(PACK_ASSETS): pack-assets.cpp disk.cpp
	$(CXX) $(CXXFLAGS) -o $(PACK_ASSETS) pack-assets.cpp disk.cpp

$(MANIFEST_GEN): manifest-gen.cpp
	$(CXX) $(CXXFLAGS) -o $(MANIFEST_GEN) manifest-gen.cpp

$(BYTECODE_COMP): bytecode-comp.cpp
	$(CXX) $(CXXFLAGS) -o $(BYTECODE_COMP) bytecode-comp.cpp

clean:
	rm -rf $(BUILD_DIR)
