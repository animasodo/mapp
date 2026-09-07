#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>

using namespace std;

// token stuff
enum token_type{
    IDENT,
    NUMBER,
    STRING,
    COMMA,
    COLON,
    PERIOD
};

struct token{
    token_type type;
    std::string text;
};

enum operand_type{
    OP_NONE,
    OP_BYTE,
    OP_TWO_BYTE,
    OP_THREE_BYTE,
    OP_LABEL_REF,
    OP_FLAG_REF,
    OP_ITEM_REF,
    OP_STRING_REF
};

struct script{
    std::string name;
    unsigned int index;
};

struct symbol{
    script current_script;
    std::string label_name;
    unsigned int index;
};

struct opcode_def{
    std::string mnemonic;
    uint8_t opcode;
    operand_type operand;
};

struct declaration{
    std::string name;
    std::string content;
};

opcode_def opcode_table[] = {
    {"end", 0x00, OP_NONE},
    {"ld_r0", 0x01, OP_BYTE},
    {"ld_r1", 0x02, OP_BYTE},
    {"ld_r0_r1", 0x03, OP_NONE},
    {"ld_r1_r0", 0x04, OP_NONE},
    {"add", 0x05, OP_NONE},
    {"sub", 0x06, OP_NONE},
    {"set_flag", 0x07, OP_FLAG_REF},
    {"clr_flag", 0x08, OP_FLAG_REF},
    {"ld_r0_flag", 0x09, OP_FLAG_REF},
    // all branch commands use a single byte for branching just like the 6502
    // if bit 7 is set, the number is negative and the jump is backwards
    {"beq", 0x0A, OP_LABEL_REF}, // r0 == r1
    {"bne", 0x0B, OP_LABEL_REF}, // r0 != r1
    {"blt", 0x0C, OP_LABEL_REF}, // r0 < r1
    {"ble", 0x0D, OP_LABEL_REF}, // r0 =< r1
    {"bge", 0x0E, OP_LABEL_REF}, // r0 >= r1
    {"bgt", 0x0F, OP_LABEL_REF}, // r0 > r1

    {"load_map", 0x10, OP_TWO_BYTE},
    {"set_tile", 0x11, OP_TWO_BYTE}
};

unsigned int get_opcode_byte_count(opcode_def opcode){
    switch(opcode.operand){
        case OP_NONE:
            return 1;
        case OP_BYTE:
            return 2;
        case OP_TWO_BYTE:
            return 3;
        case OP_THREE_BYTE:
            return 4;
        case OP_LABEL_REF:
            return 2;
        case OP_FLAG_REF:
            return 3;
        case OP_ITEM_REF:
            return 2;
        case OP_STRING_REF:
            return 255;
    }
    return 0;
}

bool str_equal(string a, string b){
    if(a.length() != b.length()) return false;

    for(unsigned int i = 0; i < a.length(); i++){
        if(tolower(a[i]) != tolower(b[i])) return false;
    }
    return true;
}

opcode_def find_defined_keyword(string keyword){
    for(auto entry : opcode_table){
        if(str_equal(entry.mnemonic, keyword)) return entry;
    }
    return {"", 0x00, OP_NONE};
}

symbol find_label(string label, vector<symbol> table){
    for(auto sb : table){
        if(sb.label_name == label) return sb;
    }
    return {{"", 0}, "", 0};
}

bool has_argument(const vector<token>& line, unsigned int mnemonic_location){
    return line.size() > mnemonic_location + 1;
}

vector<token> get_arguments(vector<token> line, unsigned int mnemonic_location){
    vector<token> arguments;

    for(unsigned int i = mnemonic_location+1; i < line.size(); ){
        arguments.push_back(line[i++]);
        if(i < line.size()){
            if(line[i++].type == COMMA) continue;
            cerr << "Error: Missing comma between arguments." << endl;
            exit(1);
        }
        break;
    }
    return arguments;
}

declaration find_declaration(string name, vector<declaration> table){
    for(auto dec : table){
        if(dec.name == name) return dec;
    }
    return {"", ""};
}

// try to convert argument into a number and if it can't, check declaration table
int convert_arg_to_num(token arg, vector<declaration> table){
    int num;
    if(arg.type == NUMBER){
        num = stoi(arg.text);
    }else{
        auto dec = find_declaration(arg.text, table);
        if(!dec.name.empty()){
            num = stoi(dec.content);
        }else{
            cout << "Error: The token cannot be translated into a number." << endl;
            exit(1);
        }
    }
    return num;
}

void check_arg_number(unsigned int number, vector<token> args){
    if(args.size() > number){
        cout << "Error: Too many arguments." << endl;
        exit(1);
    }else if(args.size() < number){
        cout << "Error: Not enough arguments." << endl;
        exit(1);
    }
}

vector<token> tokenize(std::string line){
    vector<token> tokens;
    if(line.empty()) return tokens; // skip blank line
    for(unsigned int i = 0; i < line.size(); ){
        char c = line[i];
        if(isspace(c)) {i++; continue;}
        if(c == ';') break; // stop when comment encountered
        if(c == ',') {tokens.push_back({COMMA, string(1, c)}); i++;}
        if(c == ':') {tokens.push_back({COLON, string(1, c)}); i++;}
        if(c == '.') {tokens.push_back({PERIOD, string(1, c)}); i++;}
        if(isalpha(c) || c == '_'){
            std::string token_buf;
            while(isalnum(line[i]) || line[i] == '_'){
                token_buf += line[i];
                i++;
            }
            tokens.push_back({IDENT, token_buf});
        }
        if(isdigit(c)){
            std::string token_buf;
            while(isdigit(line[i])){
                token_buf += line[i];
                i++;
            }
            tokens.push_back({NUMBER, token_buf});
        }
        if(c == '\"'){
            i++; // advance to skip quotes
            std::string token_buf;
            while(line[i] != '\"'){
                token_buf += line[i];
                i++;
            }
            tokens.push_back({STRING, token_buf});
            i++; // advance to skip ending quotes
        }
    }
    return tokens;
}

int main(int argc, char *argv[]){
    if (argc < 2) {
        cout << "Usage:\n        " << argv[0] << " [in] [options]" << endl;
        cout << "Options:" << endl;
        cout << "    -o  defines the file output." << endl;
        return 1;
    }
    string in_path = argv[1], out_path;
    for(int i = 2; i < argc; i++){
        string argument = argv[i];
        if(argument == "-o"){
            out_path = argv[++i];
        }
    }

    ifstream file_in(in_path);
    if (!file_in.is_open()) {
        cerr << "Error: Can't open input file." << endl;
        return 1;
    }

    ofstream file_out(out_path, ios::binary);
    // if (!file_out.is_open()) {
    //     cerr << "Error: Can't open output file." << endl;
    //     return 1;
    // }

    // tokenize the code
    vector<vector<token>> tokenized_code;
    std::string line;
    while(getline(file_in, line)){
        auto out = tokenize(line);
        if (!out.empty()) tokenized_code.push_back(out);
    }

    // first pass for labels
    unsigned int index = 0;
    int script_index = -1;
    string script_name;
    bool in_script = false;
    vector<symbol> symbol_table;
    vector<declaration> declaration_table;
    for(auto line : tokenized_code){
        if(line[0].type == PERIOD){ // directive
            if(line[1].text == "begin"){
                if(!in_script){
                    in_script = true;
                    script_index++;
                    script_name = line[1].text;
                    index = 0;
                    continue;
                }else{
                    cerr << "Error: Found script inside of a script." << endl;
                    return 1;
                }
            }else if(line[1].text == "end"){
                if(in_script){
                    in_script = false;
                    continue;
                }else{
                    cerr << "Error: Found end directive outside of a script." << endl;
                    return 1;
                }
            }else if(line[1].text == "declare"){
                // for whatever reason vector does not throw range exceptions
                if(line.size() == 4){
                    declaration_table.push_back({line[2].text, line[3].text});
                }else{
                    cerr << "Error: Declaration is not correctly formed." << endl;
                    return 1;
                }
                continue;
            }else{
                cerr << "Error: Unknown directive." << endl;
                return 1;
            }
        }

        if(in_script){
            unsigned int mnemonic_location = 0;
            if(line.size() > 1 && line[1].type == COLON){
                // might be a label
                symbol_table.push_back({{script_name, static_cast<unsigned int>(script_index)}, line[0].text, index});
                if(line.size() > 2){
                    mnemonic_location = 2; // if there is more code on the line, set mnemonic location to 2
                }else{
                    continue; // otherwise, skip to next line
                }
            }

            // find keyword
            opcode_def entry = find_defined_keyword(line[mnemonic_location].text);
            if(entry.mnemonic.empty()){
                cerr << "Error: Keyword '" << line[mnemonic_location].text << "' not found." << endl;
            }else{
                index += get_opcode_byte_count(entry);
            }
        }
    }

    // let's try a second pass
    index = 0;
    script_index = -1;
    in_script = false;

    for(auto line : tokenized_code){
        if(line[0].type == PERIOD){ // directive
            if(line[1].text == "begin"){
                script_index++;
                in_script = true;
                index = 0;
                continue;
            }else if(line[1].text == "end"){
                in_script = false;
                file_out.put(static_cast<uint8_t>(0x00)); // write end opcode
                continue;
            }
        }

        if(in_script){
            unsigned int mnemonic_location = 0;
            if(line.size() > 1 && line[1].type == COLON){
                if(line.size() > 2){
                    mnemonic_location = 2; // if there is more code on the line, set mnemonic location to 2
                }else{
                    continue; // otherwise, skip to next line
                }
            }

            // find keyword
            opcode_def entry = find_defined_keyword(line[mnemonic_location].text);
            if(entry.mnemonic.empty()){
                cerr << "Error: Keyword '" << line[mnemonic_location].text << "' not found." << endl;
            }else{
                // we have encountered a valid instruction
                cout << entry.mnemonic << endl;

                unsigned int byte_count = get_opcode_byte_count(entry);
                index += byte_count;
                vector<token> args = get_arguments(line, mnemonic_location);
                switch(entry.operand){
                    case OP_NONE: {
                        check_arg_number(0, args);
                        file_out.put(entry.opcode);
                        break;
                    }

                    case OP_BYTE: {
                        check_arg_number(1, args);
                        file_out.put(entry.opcode);
                        file_out.put(static_cast<uint8_t>(convert_arg_to_num(args[0], declaration_table)));
                        break;
                    }

                    case OP_TWO_BYTE: {
                        check_arg_number(2, args);
                        file_out.put(entry.opcode);
                        file_out.put(static_cast<uint8_t>(convert_arg_to_num(args[0], declaration_table)));
                        file_out.put(static_cast<uint8_t>(convert_arg_to_num(args[1], declaration_table)));
                        break;
                    }

                    case OP_THREE_BYTE: {
                        check_arg_number(3, args);
                        file_out.put(entry.opcode);
                        file_out.put(static_cast<uint8_t>(convert_arg_to_num(args[0], declaration_table)));
                        file_out.put(static_cast<uint8_t>(convert_arg_to_num(args[1], declaration_table)));
                        file_out.put(static_cast<uint8_t>(convert_arg_to_num(args[2], declaration_table)));
                        break;
                    }

                    case OP_LABEL_REF: {
                        check_arg_number(1, args);
                        file_out.put(entry.opcode);

                        string label_name = args[0].text;
                        auto sb = find_label(label_name, symbol_table);
                        if(sb.label_name == label_name && sb.current_script.index == static_cast<unsigned int>(script_index)){
                            file_out.put(static_cast<int8_t>(sb.index - index)); // we want the negative bit
                            break;
                        }
                        
                        cerr << "Error: Label '" << label_name << "' not found." << endl;
                        return 1;
                    }

                    case OP_FLAG_REF: {
                        check_arg_number(1, args);
                        file_out.put(entry.opcode);

                        unsigned int flag = convert_arg_to_num(args[0], declaration_table);

                        file_out.put(static_cast<uint8_t>(flag / 8));
                        file_out.put(static_cast<uint8_t>(0x01 << (flag % 8)));
                        break;
                    }

                    default:
                        cerr << "Error: Unknown operand type." << endl;
                        return 1;
                }
            }
        }
    }

    file_in.close();
    file_out.close();
}