// Usage:
//   pack-assets [input_d64] [output_d64] [manifest (name location t:s [u])] [asm_out]

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "disk.hpp"

using namespace std;

int manifest_track = -1, manifest_sector = -1, manifest_block_count = -1; // -1 means they haven't been set yet

// struct definition
struct asset {
    string name;
    string path;
    bool manual = false, listed = true;
    int m_track = 0, m_sector = 0;
};

struct asset_placement {
    string name;
    vector<pair<int,int>> blocks; // load order
    uint32_t size = 0;
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

// reserve an amount of space for the data
vector<pair<int,int>> reserve_run(vector<uint8_t>& disk,
                                   const vector<pair<int,int>>& order,
                                   const map<pair<int,int>, size_t>& pos_index,
                                   int track, int sector, int count,
                                   const string& asset_name) {
    auto it = pos_index.find({track, sector});
    if (it == pos_index.end()) { // if starting t:s cannot be found
        cerr << "Error: " << track << ":" << sector << " is invalid (track 18 is reserved)." << endl;
        exit(1);
    }
    
    size_t start = it->second; // start and count are in sectors
    if (start + count > order.size()) { // if the whole asset can't fit on the disk
        cerr << "Error: " << asset_name << " runs past the end of the disk." << endl;
        exit(1);
    }
    vector<pair<int,int>> chosen;
    // the code will always put each sector of data next to the others in order
    for (int i = 0; i < count; ++i) {
        auto [t, s] = order[start + i];
        if (!bam_is_free(disk, t, s)) {
            cerr << "Error: " << asset_name << " collides with already-used space at " << t << ":" << s << "." << endl;
            exit(1);
        }
        chosen.push_back({t, s});
        bam_set_used(disk, t, s);
    }
    return chosen;
}

// advances a shared cursor across calls so successive auto assets don't rescan from zero
vector<pair<int,int>> reserve_auto(vector<uint8_t>& disk,
                                    const vector<pair<int,int>>& order,
                                    size_t& cursor, int count,
                                    const string& asset_name) {
    vector<pair<int,int>> chosen;
    while (chosen.size() < static_cast<size_t>(count)) {
        if (cursor >= order.size()) {
            cerr << "Error: ran out of free disk space allocating " << asset_name << "." << endl;
            exit(1);
        }
        auto [t, s] = order[cursor++];
        if (bam_is_free(disk, t, s)) {
            bam_set_used(disk, t, s);
            chosen.push_back({t, s});
        }
    }
    return chosen;
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

void write_headered(vector<uint8_t>& disk, const vector<pair<int,int>>& blocks, const string& path, uint32_t size) {
    ifstream f(path, ios::binary);
    if (!f) {
        cerr << "Error: can't open asset file " << path << "." << endl;
        exit(1);
    }

    vector<uint8_t> data((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    data.resize(blocks.size() * 254, 0); // each block has a two byte header for the next sector

    for (size_t i = 0; i < blocks.size(); ++i) {
        auto [t, s] = blocks[i];
        uint8_t* dst = &disk[block_offset(t, s)];
        if (i + 1 < blocks.size()) {
            auto [nt, ns] = blocks[i + 1];
            dst[0] = static_cast<uint8_t>(nt);
            dst[1] = static_cast<uint8_t>(ns);
        } else {
            uint32_t bytes_in_last = size - static_cast<uint32_t>(i) * 254; // always 0-254, since blocks.size() was computed to fit
            dst[0] = 0;                  // 0 = "this is the last block"
            dst[1] = static_cast<uint8_t>(bytes_in_last);
        }
        memcpy(dst + 2, &data[i * 254], 254); // payload starts after the 2 link bytes
    }
}

int blocks_needed_for(const string& path) {
    ifstream f(path, ios::binary | ios::ate);
    if (!f) {
        cerr << "Error: can't open asset file " << path << endl;
        exit(1);
    }
    long size = f.tellg();
    return max(static_cast<int>((size + 253) / 254), 1);
}

long file_size_of(const string& path) {
    ifstream f(path, ios::binary | ios::ate);
    return f.tellg();
}

// build the manifest data that gets stored in manifest_track and manifest_sector
// it's essentially a custom BAM of some kind that sits in ram eventually
// the layout is as follows:
//   - byte 0 = number of assets
//   - bytes 1-4 = records
//     - byte 1 = start track
//     - byte 2 = start sector
//     - byte 3 = size lo
//     - byte 4 = size hi
// to retrieve a specific asset with an ID: (id << 2) + 1
vector<uint8_t> build_manifest(const vector<asset_placement>& placements) {
    vector<uint8_t> blob;
    blob.push_back(0);
    blob.push_back(0); // padding
    blob.push_back(static_cast<uint8_t>(placements.size()));
    for (auto& p : placements) {
        auto [t, s] = p.blocks.front(); // first entry in chain order = chain start
        blob.push_back(static_cast<uint8_t>(t));
        blob.push_back(static_cast<uint8_t>(s));
        blob.push_back(p.size & 0xFF);
        blob.push_back((p.size >> 8) & 0xFF);
    }
    return blob;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cout << "Usage:\n        " << argv[0] << " [in_d64] [out_d64] [manifest (name location t:s [u])] [options]" << endl;
        cout << "Options:" << endl;
        cout << "    -r  creates a report file." << endl;
        cout << "    -t  sets the manifest track." << endl;
        cout << "    -s  sets the manifest sector." << endl;
        cout << "    -c  sets the manifest size in blocks." << endl;
        return 1;
    }
    string in_path = argv[1], out_path = argv[2], manifest_path = argv[3], report_path;
    for(int i = 4; i < argc; i++){
        string argument = argv[i];
        if(argument == "-r"){
            report_path = argv[++i];
        }
        if(argument == "-t"){
            manifest_track = stoi(argv[++i]);
        }
        if(argument == "-s"){
            manifest_sector = stoi(argv[++i]);
        }
        if(argument == "-c"){
            manifest_block_count = stoi(argv[++i]);
        }
    }

    if (manifest_track == -1 || manifest_sector == -1 || manifest_block_count == -1) {
        cerr << "Error: manifest track, sector and/or size haven't been set." << endl;
        return 1;
    }

    ifstream in(in_path, ios::binary);
    if (!in) {
        cerr << "Error: can't open input image " << in_path << "." << endl;
        return 1;
    }

    vector<uint8_t> disk((istreambuf_iterator<char>(in)), istreambuf_iterator<char>()); // load disk image into a vector
    vector<pair<int,int>> order = all_blocks_in_order(); // then see what tracks and sectors we got
    map<pair<int,int>, size_t> pos_index; // this will act as our sector index (6:0 returns 105 for example)
    for (size_t i = 0; i < order.size(); ++i) {
        pos_index[order[i]] = i;
    }

    // reserve the blocks that will be used for the manifest
    // this will be loaded in memory fairly early on into the booting process
    vector<pair<int,int>> manifest_blocks = reserve_run(disk, order, pos_index, manifest_track, manifest_sector, manifest_block_count, "manifest region");

    // read and parse manifest
    vector<asset> definitions = load_manifest(manifest_path);

    vector<asset_placement> placements;
    vector<asset_placement> unlisted_placements;
    size_t auto_cursor = 0;

    // pass 1: manual placements
    for (auto& d : definitions) {
        if (!d.manual) continue;
        uint32_t size = file_size_of(d.path);
        vector<pair<int,int>> blocks = reserve_run(disk, order, pos_index, d.m_track, d.m_sector, blocks_needed_for(d.path), "manual placement for '" + d.name + "'");
        write_headered(disk, blocks, d.path, size);
        if (d.listed) {
            placements.push_back({d.name, blocks, size, true});
        }else {
            unlisted_placements.push_back({d.name, blocks, size, true});
        }
    }

    // pass 2: everything else, fit in whatever's left
    for (auto& d : definitions) {
        if (d.manual) continue;
        uint32_t size = file_size_of(d.path);
        vector<pair<int,int>> blocks = reserve_auto(disk, order, auto_cursor, blocks_needed_for(d.path), d.name);
        write_headered(disk, blocks, d.path, size);
        if (d.listed) {
            placements.push_back({d.name, blocks, size, false});
        }else {
            unlisted_placements.push_back({d.name, blocks, size, false});
        }
    }

    // build and write the manifest table itself
    vector<uint8_t> blob = build_manifest(placements);
    blob.resize(manifest_blocks.size() * 256, 0);
    for (size_t i = 0; i < manifest_blocks.size(); ++i) {
        auto [t, s] = manifest_blocks[i];
        memcpy(&disk[block_offset(t, s)], &blob[i * 256], 256);
    }

    // write the output image
    ofstream out(out_path, ios::binary);
    out.write(reinterpret_cast<char*>(disk.data()), disk.size());
    out.close();

    // human-readable report
    if(!report_path.empty()){
        ofstream report(report_path);
        report << "*** Disk layout report ***\n\n";
        report << "Manifest region: track " << manifest_track << " sector " << manifest_sector
            << ", " << manifest_block_count << " sectors\n\n";
        report << "Listed assets (featured in manifest):\n\n";
        for (size_t i = 0; i < placements.size(); ++i) {
            auto& p = placements[i];
            report << i << ". " << p.name << (p.manual ? " (manual)" : " (auto)")
                << "  size=" << p.size << "  blocks=" << p.blocks.size() << "\n    ";
            for (auto& [t, s] : p.blocks) report << t << ":" << s << " ";
            report << "\n";
        }
        report << "\nUnlisted assets:\n\n";
        for (size_t i = 0; i < unlisted_placements.size(); ++i) {
            auto& p = unlisted_placements[i];
            report << p.name << (p.manual ? " (manual)" : " (auto)")
                << "  size=" << p.size << "  blocks=" << p.blocks.size() << "\n    ";
            for (auto& [t, s] : p.blocks) report << t << ":" << s << " ";
            report << "\n";
        }
        report.close();
    }

    // assembly file, gets picked up by ca65
    // ofstream asm_out(asm_path);
    // asm_out << "; auto-generated by pack-assets\n\n";
    // asm_out << "manifest_track        = " << manifest_track << "\n";
    // asm_out << "manifest_sector       = " << manifest_sector << "\n";
    // asm_out << "manifest_block_count = " << manifest_block_count << "\n\n";
    // asm_out << "; symbol IDs\n";
    // for (size_t i = 0; i < placements.size(); i++) {
    //     asm_out << sanitize_symbol(placements[i].name) << " = " << i << endl;
    // }
    // asm_out << "\n; unlisted assets\n";
    // for (auto &p : unlisted_placements) {
    //     asm_out << sanitize_symbol(p.name) << "_TRACK = " << p.blocks[0].first << endl;
    //     asm_out << sanitize_symbol(p.name) << "_SECTOR = " << p.blocks[0].second << endl;
    // }
    // asm_out.close();

    // this is now done with manifest-gen

    cout << "OK: " << (placements.size() + unlisted_placements.size()) << " assets placed. Wrote " << out_path << ", " << report_path << endl;
    return 0;
}
