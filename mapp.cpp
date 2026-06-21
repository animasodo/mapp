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
bool use_list = false;
map<string, int> name_map;
string filenameOutput;

const string HELP = "Usage:\n        mapp [infile] [options]\n"
                    "Options:\n"
                    "    -h  prints this message.\n"
                    "    -o  outputs with custom filename.\n"
                    "    -w  sets width.\n"
                    "    -t  sets height.\n"
                    "    -j  imports a json file.\n"
                    "    -l  imports a csv for storing ids instead of map names.\n";

// structs
typedef struct{
    string name;
    uint8_t src_x, src_y, dst_x, dst_y;
} warp;
typedef struct{
    uint8_t x, y;
} door;

// vectors
vector<warp> warps;
vector<door> doors;

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

void from_json(const json& j, door& d) {
    try {
        j.at("x").get_to(d.x);
        j.at("y").get_to(d.y);
    }catch (nlohmann::detail::out_of_range &e) {
        cout << "Warning: One or more values in a door are missing." << endl;
    }catch (nlohmann::detail::type_error &e) {
        cout << "Warning: One or more values in a door are the wrong type." << endl;
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        cerr << "Error: Arguments not provided." << endl;
        return 1;
    }

    ifstream file_in(argv[1], ios::binary);
    if (!file_in.is_open()) {
        cerr << "Error: Can't open map file." << endl;
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
            doors = jsonData["doors"].get<vector<door>>();
            jsonFile.close();
        }
        if (argument == "-l") {
            use_list = true;
            ifstream name_list(argv[++a]);
            if (!name_list.is_open()) {
                cerr << "Error: Can't open name list." << endl;
                return 1;
            }

            string line;
            while (getline(name_list, line)) {
                istringstream ss(line);
                string key;
                string valueStr;

                if (getline(ss, key, ',') && getline(ss, valueStr)) {
                    try {
                        name_map[key] = stoi(valueStr);
                    } catch (const invalid_argument&) {
                        cerr << "Warning: Invalid value " << valueStr << " for key " << key << "." << endl;
                    }
                }
            }
            name_list.close();
        }
    }

    if (width == 0 || height == 0) {
        cerr << "Error: Width and height must be set." << endl;
        return 1;
    }

    ofstream file_out(filenameOutput, ios::binary);

    finSize = filesystem::file_size(argv[1]);

    int currentByte = file_in.get();
    if (currentByte == EOF) {
        cerr << "Error: Input file is empty." << endl;
        return 1;
    }

    oldChar = static_cast<unsigned char>(currentByte);
    counter = 1;

    // header
    buffer[outSize++] = 0x4d; // M
    buffer[outSize++] = 0x50; // P
    // size
    buffer[outSize++] = static_cast<uint8_t>(width);
    buffer[outSize++] = static_cast<uint8_t>(height);
    // length, left blank until counted
    buffer[outSize++] = 0;
    buffer[outSize++] = 0;

    // map data
    while ((currentByte = file_in.get()) != EOF) {
        currentChar = static_cast<unsigned char>(currentByte);

        if (currentChar == oldChar && counter < 255) {
            counter++;
        } else {
            buffer[outSize++] = static_cast<uint8_t>(counter);
            buffer[outSize++] = oldChar;
            compressedLength += 2;
            counter = 1;
            oldChar = currentChar;
        }
    }

    buffer[outSize++] = static_cast<uint8_t>(counter);
    buffer[outSize++] = oldChar;
    compressedLength += 2;

    // compressed map length (little endian)
    buffer[4] = static_cast<uint8_t>(static_cast<uint16_t>(compressedLength) & 0x00FF);
    buffer[5] = static_cast<uint8_t>((static_cast<uint16_t>(compressedLength) & 0xFF00) >> 8);

    // warps
    for (warp w : warps) {
        buffer[outSize++] = 0x57; // W
        if (use_list == true) {
            if (name_map.find(w.name) != name_map.end()) {
                buffer[outSize++] = static_cast<uint8_t>(name_map[w.name]);
            } else {
                cerr << "Error: Key " << w.name << " not found." << endl;
                return 1;
            }
        }else {
            for (int i = 0; i <= static_cast<int>(w.name.length()); i++) {
                buffer[outSize++] = w.name[i];
            }
        }
        buffer[outSize++] = w.src_x;
        buffer[outSize++] = w.src_y;
        buffer[outSize++] = w.dst_x;
        buffer[outSize++] = w.dst_y;
    }

    for (door d : doors) {
        buffer[outSize++] = 0x44; // D
        buffer[outSize++] = d.x;
        buffer[outSize++] = d.y;
    }

    // end of file
    buffer[outSize++] = 0x45; // E

    file_out.write(reinterpret_cast<const ostream::char_type *>(buffer), outSize);

    file_in.close();
    file_out.close();
    return 0;
}
