#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include "disk.hpp"
using namespace std;

typedef struct{
    uint8_t name[16], track, sector, type;
} file;

const int BUFFER_SIZE = 0x20;
vector<file> list;
uint8_t entry_buffer[BUFFER_SIZE];

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Error: Not enough arguments." << endl;
        return 1;
    }

    ifstream file_in(argv[1], ios::binary);
    if (!file_in.is_open()) {
        cerr << "Error: Can't open input file." << endl;
        return 1;
    }

    ofstream file_out(argv[2]);
    if (!file_out.is_open()) {
        cerr << "Error: Can't open output file." << endl;
        return 1;
    }

    int track = 18, sector = 1;
    while (track != 0) {
        file_in.seekg(track_offset(track) + sector * 256, ios::beg);
        uint8_t sector_buf[256];
        file_in.read(reinterpret_cast<char*>(sector_buf), 256);

        int next_track = sector_buf[0];
        int next_sector = sector_buf[1];

        for (int i = 0; i < 8; ++i) {
            uint8_t* e = sector_buf + i * 32;
            if (e[2] == 0) continue; // unused/scratched slot
            file entry;
            entry.type = e[2] & 0x0F;
            entry.track = e[3];
            entry.sector = e[4];
            memcpy(entry.name, e + 5, 16);
            list.push_back(entry);
        }

        track = next_track;
        sector = next_sector;
    }

    for(const auto& entry : list) {
        string name(reinterpret_cast<const char*>(entry.name), 16);
        transform(name.begin(), name.end(), name.begin(), ::tolower);
        name.erase(find(name.begin(), name.end(), '\xA0'), name.end());
        file_out << name << "," << static_cast<int>(entry.track) << "," << static_cast<int>(entry.sector) << "," << static_cast<int>(entry.type) << endl;
    }

    file_in.close();
    file_out.close();
    return 0;
}
