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
    PERIOD,
    EQUAL
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
    std::string script_name;
    unsigned int size;
};

struct symbol{
    unsigned int script_index;
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
    {"set_flag", 0x07, OP_FLAG_REF}, // put r0 boolean (0 or !0) into flag
    {"ld_flag", 0x08, OP_FLAG_REF}, // put flag boolean into r0
    // all branch commands use a single byte for branching just like the 6502
    // if bit 7 is set, the number is negative and the jump is backwards
    {"beq", 0x0A, OP_LABEL_REF}, // r0 == r1
    {"bne", 0x0B, OP_LABEL_REF}, // r0 != r1
    {"blt", 0x0C, OP_LABEL_REF}, // r0 < r1
    {"bgt", 0x0D, OP_LABEL_REF}, // r0 > r1
    {"bra", 0x0E, OP_LABEL_REF}, // branch always

    {"load_map", 0x10, OP_TWO_BYTE},
    {"set_tile", 0x11, OP_TWO_BYTE},
    {"say", 0x12, OP_STRING_REF},
    {"ynq", 0x13, OP_NONE}, // yes/no question
    {"wsp", 0x14, OP_NONE} // pause until spacebar pressed
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
            return 1;
    }
    return 0;
}

string convert_string(string in){
    string buf;

    for(unsigned int i = 0; i < in.length(); i++){
        if(in[i] >= 0x41 && in[i] <= 0x5A){ // uppercase
            buf += in[i] + 0x80;
            continue;
        }else if(in[i] >= 0x61 && in[i] <= 0x7A){ // lowercase
            buf += in[i] - 0x20;
            continue;
        }else if(in[i] == '\\'){
            switch(in[++i]){
                case 'n':
                    buf += 0x0D;
                    break;
                case 'c': {
                    unsigned int number = in[++i] - '0';
                    if(number > 7){ // we only need the first 8 colors
                        cerr << "Warning: Unexpected number in color sequence." << endl;
                    }
                    buf += 0x01;
                    buf += static_cast<uint8_t>(number);
                    break;
                }
                default:
                    buf += in[i];
                    break;
            }
        }else{
            buf += in[i];
        }
    }

    buf += static_cast<char>(0x00); // for whatever reason, there was no null terminator being generated, so we're doing it ourselves lmao

    return buf;
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
    return {0, "", 0};
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
        if(c == '=') {tokens.push_back({EQUAL, string(1, c)}); i++;}
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
        cout << "    -s  output a list of the script names with their number." << endl;
        return 1;
    }
    string in_path = argv[1], out_path, script_list_path;
    for(int i = 2; i < argc; i++){
        string argument = argv[i];
        if(argument == "-o"){
            out_path = argv[++i];
        }
        if(argument == "-s"){
            script_list_path = argv[++i];
        }
    }

    ifstream file_in(in_path);
    if (!file_in.is_open()) {
        cerr << "Error: Can't open input file." << endl;
        return 1;
    }

    // tokenize the code
    vector<vector<token>> tokenized_code;
    std::string line;
    while(getline(file_in, line)){
        auto out = tokenize(line);
        if (!out.empty()) tokenized_code.push_back(out);
    }

    unsigned int index = 0;
    int script_index = -1;
    string script_name;
    bool in_script = false;
    vector<symbol> symbol_table;
    vector<declaration> declaration_table;
    vector<script> script_list;
    // data that's gonna be in the disk
    vector<uint8_t> final_index;
    vector<uint8_t> compiled_data;

    // first pass for labels
    for(auto line : tokenized_code){
        if(line[0].type == PERIOD){ // directive
            if(line[1].text == "begin"){
                if(!in_script){
                    in_script = true;
                    script_index++;
                    script_name = line[2].text;
                    index = 0;
                    continue;
                }else{
                    cerr << "Error: Found script inside of a script." << endl;
                    return 1;
                }
            }else if(line[1].text == "end"){
                if(in_script){
                    script_list.push_back({script_name, index});
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

        if(line.size() > 1 && line[1].type == EQUAL){ // equal style declaration
            if(line.size() == 3){
                declaration_table.push_back({line[0].text, line[2].text});
            }else{
                cerr << "Error: Declaration is not correctly formed." << endl;
                return 1;
            }
        }

        if(in_script){
            unsigned int mnemonic_location = 0;
            if(line.size() > 1 && line[1].type == COLON){
                // might be a label
                symbol_table.push_back({static_cast<unsigned int>(script_index), line[0].text, index});
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
                return 1;
            }else{
                index += get_opcode_byte_count(entry);
                entry.operand == OP_STRING_REF? index += convert_string(line[mnemonic_location + 1].text).length() : 0;
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
                // cout << entry.mnemonic << endl;

                index += get_opcode_byte_count(entry);
                entry.operand == OP_STRING_REF? index += convert_string(line[mnemonic_location + 1].text).length() : 0;

                vector<token> args = get_arguments(line, mnemonic_location); // this gets arguments and also makes sure they're separated by commas
                switch(entry.operand){
                    case OP_NONE: {
                        check_arg_number(0, args);
                        compiled_data.push_back(entry.opcode);
                        break;
                    }

                    case OP_BYTE: {
                        check_arg_number(1, args);
                        compiled_data.push_back(entry.opcode);
                        compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[0], declaration_table)));
                        break;
                    }

                    case OP_TWO_BYTE: {
                        check_arg_number(2, args);
                        compiled_data.push_back(entry.opcode);
                        if(entry.mnemonic == "load_map"){ // flip x and y for easier interpretation
                            compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[1], declaration_table)));
                            compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[0], declaration_table)));
                        }else{
                            compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[0], declaration_table)));
                            compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[1], declaration_table)));
                        }
                        break;
                    }

                    case OP_THREE_BYTE: {
                        check_arg_number(3, args);
                        compiled_data.push_back(entry.opcode);
                        compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[0], declaration_table)));
                        compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[1], declaration_table)));
                        compiled_data.push_back(static_cast<uint8_t>(convert_arg_to_num(args[2], declaration_table)));
                        break;
                    }

                    case OP_LABEL_REF: {
                        check_arg_number(1, args);
                        compiled_data.push_back(entry.opcode);

                        string label_name = args[0].text;
                        auto sb = find_label(label_name, symbol_table);
                        cout << sb.index << ", " << sb.label_name << ", " << sb.script_index << endl; // debugging
                        if(sb.label_name == label_name && sb.script_index == static_cast<unsigned int>(script_index)){
                            cout << "Label index: " << sb.index << ", current index: " << index << ", added: " << (sb.index - index) << endl;
                            compiled_data.push_back(static_cast<int8_t>(sb.index - index)); // we want the negative bit
                            break;
                        }
                        
                        cerr << "Error: Label '" << label_name << "' not found." << endl;
                        return 1;
                    }

                    case OP_FLAG_REF: {
                        check_arg_number(1, args);
                        compiled_data.push_back(entry.opcode);

                        unsigned int flag = convert_arg_to_num(args[0], declaration_table);

                        compiled_data.push_back(static_cast<uint8_t>(flag / 8));
                        compiled_data.push_back(static_cast<uint8_t>(0x01 << (flag % 8)));
                        break;
                    }

                    case OP_STRING_REF: {
                        check_arg_number(1, args);
                        if(args[0].type != STRING){
                            cerr << "Error: Not a string." << endl;
                        }
                        compiled_data.push_back(entry.opcode);
                        string converted_string = convert_string(args[0].text);
                        for(char c : converted_string){
                            compiled_data.push_back(c);
                        }
                        break;
                    }

                    default:
                        cerr << "Error: Unknown operand type." << endl;
                        return 1;
                }
            }
        }
    }
    
    // done compiling, let's put everything in the output file
    ofstream file_out(out_path, ios::binary);

    file_out.put(script_list.size()); // number of scripts

    unsigned int offset = 0;
    for(auto scr : script_list){ // script offset
        file_out.put(static_cast<uint8_t>(offset & 0xFF));
        file_out.put(static_cast<uint8_t>((offset & 0xFF00) >> 8));
        offset += scr.size;
    }
    for(auto b : compiled_data){
        file_out.put(static_cast<uint8_t>(b));
    }

    file_out.close();

    if(!script_list_path.empty()){
        ofstream script_file(script_list_path);
        for(auto scr : script_list){
            script_file << scr.script_name << endl;
        }
        script_file.close();
    }

    file_in.close();
}