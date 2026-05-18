#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <regex>

using namespace std;

unsigned char currentChar, oldChar, buffer[65536];
unsigned int counter = 0, outSize = 0, finSize;
int width = 0, height = 0;
string filenameOutput;

const string HELP = "Usage:\n        mapp [infile] [options]\n"
                    "Options:\n"
                    "    -h  prints this message.\n"
                    "    -o  outputs with custom filename.\n"
                    "    -w  sets width.\n"
                    "    -t  sets height.\n";

int main(int argc, char *argv[]) {
    if (argc == 1) {
        cerr << "Error: Arguments not provided." << endl;
        return 1;
    }else if(argc >= 2){
        string argument = argv[1];
        if (argument == "-h") {
            cout << HELP;
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
            if (argument == "-o"){
                filenameOutput = argv[++a];
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
        buffer[outSize++] = 0; // type: map
        buffer[outSize++] = width;
        buffer[outSize++] = height;

        // map data
        for (unsigned int i = 0; i <= finSize; i++) {
            currentChar = fin.get();
            if (currentChar == oldChar && counter < 15) {
                counter++;
            }else {
                buffer[outSize++] = (counter << 4) + oldChar;
                counter = 0;
            }
            oldChar = currentChar;
        }

        fout.write(reinterpret_cast<const ostream::char_type *>(buffer), outSize);

        fin.close();
        fout.close();
        return 0;
    }
}
