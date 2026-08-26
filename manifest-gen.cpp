// this tool is just a cutdown version of the pack-assets tool
// its purpose is to generate an assembly file from a manifest text file, without actually packing any assets into a disk image or generating a manifest table
// it's useful for the earlier steps of the build process

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

int manifest_track = -1, manifest_sector = -1; // -1 means they haven't been set yet

// struct definition
struct asset {
    string name;
    string path;
    bool manual = false, listed = true;
    int m_track = 0, m_sector = 0;
};

struct asset_placement {
    string name;
    uint32_t size = 0;
    int m_track = 0, m_sector = 0;
    bool manual = false;
};

// something for the assembly strings
string sanitize_symbol(const string& name) {
    string out;
    for (char c : name) {
        if (isalnum(c)){
            out += static_cast<char>(toupper(c));
        }
        else out += '_';
    }
    return out;
}

vector<asset> load_manifest(const string& path) {
    ifstream f(path);
    if (!f) {
        cerr << "Error: can't open manifest file " << path << "." << endl;
        exit(1);
    }
    vector<asset> defs;
    string line;
    int lineno = 0;
    while (getline(f, line)) {
        lineno++;
        istringstream iss(line);
        string name, filepath, placement, listed;
        if (!(iss >> name)) continue; // blank line
        if (!(iss >> filepath >> placement)) { // arguments are separated by one or more spaces
            cerr << "Error: manifest line " << lineno << " is malformed: " << line << "." << endl;
            exit(1);
        }
        iss >> listed; // optional
        // if the manifest line contains a 'u' at the end (unlisted), instead of having an entry in the manifest blocks,
        // it just places a track and sector pointer in the assembly file for direct use
        asset def;
        def.name = name;
        def.path = filepath;
        if (listed == "u") {
            def.listed = false;
        }
        if (placement == "auto") {
            def.manual = false;
        } else {
            auto colon = placement.find(':');
            if (colon == string::npos) {
                cerr << "Error: manifest line " << lineno << " placement must be 'auto' or 't:s': " << placement << endl;
                exit(1);
            }
            def.manual = true;
            def.m_track = stoi(placement.substr(0, colon));
            def.m_sector = stoi(placement.substr(colon + 1));
        }
        defs.push_back(def);
    }
    return defs;
}

long file_size_of(const string& path) {
    ifstream f(path, ios::binary | ios::ate);
    return f.tellg();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage:\n        " << argv[0] << " [in_manifest] [out_asm] [options]" << endl;
        cout << "Options:" << endl;
        cout << "    -t  sets the manifest track." << endl;
        cout << "    -s  sets the manifest sector." << endl;
        return 1;
    }
    string manifest_path = argv[1], asm_path = argv[2];
    for(int i = 3; i < argc; i++){
        string argument = argv[i];
        if(argument == "-t"){
            manifest_track = stoi(argv[++i]);
        }
        if(argument == "-s"){
            manifest_sector = stoi(argv[++i]);
        }
    }

    if (manifest_track == -1 || manifest_sector == -1) {
        cerr << "Error: manifest track, sector and/or size haven't been set." << endl;
        return 1;
    }

    // read and parse manifest
    vector<asset> definitions = load_manifest(manifest_path);

    vector<asset_placement> placements;
    vector<asset_placement> unlisted_placements;

    // to respect the order that will be used in the manifest table, we need to do two passes over the definitions: first the manual placements, then the auto placements
    // this mirrors pack-assets, but we don't actually need to reserve any blocks or write anything to a disk image, we just need to record the placements for the assembly file
    for (auto& d : definitions) {
        if (!d.manual) continue;
        uint32_t size = file_size_of(d.path);
        if (d.listed) {
            placements.push_back({d.name, size, d.m_track, d.m_sector, true});
        }else {
            unlisted_placements.push_back({d.name, size, d.m_track, d.m_sector, true});
        }
    }

    // we don't know if the track and sector for auto placements are valid but that's okay because we only need to assign an ID to each asset
    // if there's some kind of error, it will be caught later when pack-assets is run
    for (auto& d : definitions) {
        if (d.manual) continue;
        uint32_t size = file_size_of(d.path);
        if (d.listed) {
            placements.push_back({d.name, size, d.m_track, d.m_sector, true});
        }else {
            unlisted_placements.push_back({d.name, size, d.m_track, d.m_sector, true});
        }
    }

    // assembly file, gets picked up by ca65
    ofstream asm_out(asm_path);
    asm_out << "; auto-generated by manifest-gen\n\n";
    asm_out << "MANIFEST_TRACK  = " << manifest_track << "\n";
    asm_out << "MANIFEST_SECTOR = " << manifest_sector << "\n";
    asm_out << "MANIFEST_SIZE   = " << (definitions.size() * 4) + 1 << "\n\n";
    asm_out << "; symbol IDs\n";
    for (size_t i = 0; i < placements.size(); i++) {
        asm_out << sanitize_symbol(placements[i].name) << " = " << i << endl;
    }
    asm_out << "\n; unlisted assets\n";
    for (auto &p : unlisted_placements) {
        asm_out << sanitize_symbol(p.name) << "_TRACK = " << p.m_track << endl;
        asm_out << sanitize_symbol(p.name) << "_SECTOR = " << p.m_sector << endl;
        asm_out << sanitize_symbol(p.name) << "_SIZE = " << p.size << endl;
    }
    asm_out.close();

    cout << "OK: " << (placements.size() + unlisted_placements.size()) << " assets placed. Wrote " << asm_path << endl;
    return 0;
}
