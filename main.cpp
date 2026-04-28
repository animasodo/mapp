#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <regex>

#define MAX 0xFFFF

using namespace std;

unsigned char currentChar, oldChar, buffer[MAX];
unsigned int counter = 0, outSize = 0, finSize;
int width = 0, height = 0;
string filenameOutput, filenameHeader, objectName;
bool rawOutput, customFilename, makeHeader;

const string HELP = "Usage:\n        mapp [infile] [options]\n"
                    "Options:\n"
                    "    -h  prints this message.\n"
                    "    -o  outputs with custom filename.\n"
                    "    -r  outputs raw binary data.\n"
                    "    -n  sets the object name.\n"
                    "    -w  sets width.\n"
                    "    -t  sets height.\n"
                    "    -H  generates a header file.\n";

string toHex(char a) {
    const string hexMap = "0123456789ABCDEF";
    char bfr[3];
    bfr[0] = hexMap[(a & 0xF0) >> 4];
    bfr[1] = hexMap[a & 0x0F];
    bfr[2] = '\0';

    return bfr;
}

string defineStruct(string name, char *data, unsigned int size) {
    unsigned int i = 0, counter = 0;

    string outBuffer = "Map " + name + " = {" + to_string(width) + ", " + to_string(height) + ", \n\t";
    while (i < size) {
        for (unsigned char a = 0; a < 8; a++) {
            outBuffer += "0x" + toHex(data[i++]) + ", ";
            if (++counter == size) {
                outBuffer.erase(outBuffer.length() - 2);
                break;
            }
        }
        outBuffer += "\n\t";
    }
    outBuffer.erase(outBuffer.length() - 1);
    outBuffer += "};\n";

    return outBuffer;
}

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
        filenameOutput = filenameIn.replace_extension("c").string();
        objectName = filenameIn.stem().string();
        transform(objectName.begin(), objectName.end(), objectName.begin(), ::toupper);

        for (int a = 1; a < argc; a++) {
            string argument = argv[a];
            if (argument == "-h") {
                cout << HELP;
                return 1;
            }
            if (argument == "-r") {
                rawOutput = true;
                if (!customFilename) {
                    filenameOutput = filenameIn.replace_extension("out").string();
                }
            }
            if (argument == "-n") objectName = argv[++a];
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
                customFilename = true;
            }
            if (argument == "-H") makeHeader = true;
        }

        if (width == 0 || height == 0) {
            cerr << "Error: Width and height must be set.";
            return 1;
        }

        ofstream fout(filenameOutput);

        finSize = filesystem::file_size(argv[1]);

        oldChar = fin.peek();

        for (unsigned int i = 0; i <= finSize; i++) {
            currentChar = fin.get();
            if (currentChar == oldChar && counter < 15) {
                counter++;
            }else {
                buffer[outSize] = (counter << 4) + oldChar;
                outSize++;
                counter = 0;
            }
            oldChar = currentChar;
        }

        if (rawOutput){
            fout.write(reinterpret_cast<const ostream::char_type *>(buffer), outSize);
        }else {
            string cBuffer = "#include \"globals.h\"\n\n" + defineStruct(objectName, reinterpret_cast<char *>(buffer), outSize);
            string hBuffer = "#include \"globals.h\"\n\n#ifndef " + objectName + "_H\n#define " + objectName + "_H\n\nextern const Map " + objectName + ";\n\n#endif\n";
            fout << cBuffer;
            if (makeHeader) {
                ofstream hout(filenameIn.replace_extension("h").string());
                hout << hBuffer;
                hout.close();
            }
        }
        fin.close();
        fout.close();
        return 0;
    }
}
