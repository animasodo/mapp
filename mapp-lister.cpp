#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <vector>

using namespace std;

map<string, int> name_map;
vector<string> keys;

vector<string> map_to_vector(map<string, int> map) {
    vector<string> keys;
    keys.reserve(map.size());
    for (const auto& [key, value] : map) {
        keys.push_back(key);
    }

    sort(keys.begin(), keys.end(), [&map](string a, string b) {
        return map.at(a) < map.at(b);
    });

    return keys;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Error: Not enough arguments." << endl;
        return 1;
    }

    ifstream file_in(argv[1]);
    if (!file_in.is_open()) {
        cerr << "Error: Can't open input file." << endl;
        return 1;
    }

    ofstream file_out(argv[2]);
    if (!file_out.is_open()) {
        cerr << "Error: Can't open output file." << endl;
        return 1;
    }

    string line;
    while (getline(file_in, line)) {
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

    keys = map_to_vector(name_map);

    file_out << ".segment \"RODATA\"\n\n; filenames" << endl;
    for (string str : keys) {
        file_out << str << ":\n\t.byte \"" << str << "\", 0" << endl;
    }
    file_out << "; pointers\nptr_lo:\n";
    for (string str : keys) {
        file_out << "\t.byte <" << str << endl;
    }
    file_out << "\nptr_hi:\n";
    for (string str : keys) {
        file_out << "\t.byte >" << str << endl;
    }

    file_in.close();
    file_out.close();
    return 0;
}
