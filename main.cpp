#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include "json.hpp" // nlohmann/json

using namespace std;
using json = nlohmann::json;

unsigned char currentChar, oldChar, buffer[65536];
unsigned int counter = 0, outSize = 0, compressedLength = 0, finSize;
int width = 0, height = 0;
string filenameOutput;

const string HELP = "Usage:\n        mapp [infile] [options]\n"
                    "Options:\n"
                    "    -h  prints this message.\n"
                    "    -o  outputs with custom filename.\n"
                    "    -w  sets width.\n"
                    "    -t  sets height.\n"
                    "    -j  imports a json file.\n";

// structs
typedef struct{
    string name;
    uint8_t src_x, src_y, dst_x, dst_y;
} warp;

// vectors
vector<warp> warps;

void from_json(const json& j, warp& w) {
    try {
        j.at("map").get_to(w.name);
        j.at("src_x").get_to(w.src_x);
        j.at("src_y").get_to(w.src_y);
        j.at("dst_x").get_to(w.dst_x);
        j.at("dst_y").get_to(w.dst_y);
    }catch (nlohmann::detail::out_of_range &e) {
        cout << "Warning: One or more values in \"" << w.name << "\" are missing." << endl;
    }catch (nlohmann::detail::type_error &e) {
        cout << "Warning: One or more values in \"" << w.name << "\" are the wrong type." << endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        cerr << "Error: Arguments not provided." << endl;
        return 1;
    }

    ifstream fin(argv[1]);
    if (!fin.is_open()) {
        cerr << "Error: Can't open file." << endl;
        return 1;
    }

    filesystem::path filenameIn(argv[1]);
    filenameOutput = filenameIn.replace_extension("out").string();

    for (int a = 1; a < argc; a++) {
        string argument = argv[a];
        if (argument == "-h") {
            cout << HELP;
            return 1;
        }
        if (argument == "-w") {
            width = stoi(argv[++a]);
            if (width <= 0 || width > 255) {
                cerr << "Error: Width must be set and a number between 1 and 255." << endl;
                return 1;
            }
        }
        if (argument == "-t") {
            height = stoi(argv[++a]);
            if (height <= 0 || height > 255) {
                cerr << "Error: Height must be set and a number between 1 and 255." << endl;
                return 1;
            }
        }
        if (argument == "-o") {
            filenameOutput = argv[++a];
        }
        if (argument == "-j") {
            ifstream jsonFile(argv[++a]);
            if (!jsonFile.is_open()) {
                cerr << "Error: Can't open json file." << endl;
                return 1;
            }
            json jsonData = json::parse(jsonFile);

            try {
                width = jsonData.at("width");
                height = jsonData.at("height");
            }catch (nlohmann::detail::out_of_range &e) {
                cout << "Warning: Cannot find width or height in json." << endl;
            }catch (nlohmann::detail::type_error &e) {
                cout << "Warning: Width and height values are the wrong format." << endl;
            }

            warps = jsonData["warps"].get<vector<warp>>();
            jsonFile.close();
        }
    }

    if (width == 0 || height == 0) {
        cerr << "Error: Width and height must be set.";
        return 1;
    }

    ofstream fout(filenameOutput);

    finSize = filesystem::file_size(argv[1]);

    oldChar = fin.peek();

    // header
    buffer[outSize++] = 0x4d; // M
    buffer[outSize++] = 0x50; // P
    // size
    buffer[outSize++] = (uint8_t) width;
    buffer[outSize++] = (uint8_t) height;
    // length, left blank until counted
    buffer[outSize++] = 0;
    buffer[outSize++] = 0;

    // map data
    for (unsigned int i = 0; i <= finSize; i++) {
        currentChar = fin.get();

        if (currentChar == oldChar && counter < 15) {
            counter++;
        } else {
            buffer[outSize++] = (counter << 4) + oldChar;
            counter = 0;
            compressedLength++;
        }
        oldChar = currentChar;
    }

    // compressed map length (little endian)
    buffer[4] = (uint8_t) (((uint16_t) compressedLength) & 0x00FF);
    buffer[5] = (uint8_t) ((((uint16_t) compressedLength) & 0xFF00) >> 8);

    // warps
    for (warp w : warps) {
        buffer[outSize++] = 0x57; // W
        for (int i = 0; i <= (int)w.name.length(); i++) {
            buffer[outSize++] = w.name[i];
        }
        buffer[outSize++] = w.src_x;
        buffer[outSize++] = w.src_y;
        buffer[outSize++] = w.dst_x;
        buffer[outSize++] = w.dst_y;
    }

    fout.write(reinterpret_cast<const ostream::char_type *>(buffer), outSize);

    fin.close();
    fout.close();
    return 0;
}
