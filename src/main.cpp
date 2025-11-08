#include <iostream>
#include <string>
#include <fstream>
#include "tyco/parser_new.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config.tyco>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    
    try {
        // Parse the Tyco file
        tyco::TycoLexer lexer;
        auto context = lexer.parse_file(filename);
        
        if (!context) {
            std::cerr << "Failed to parse file: " << filename << std::endl;
            return 1;
        }
        
        // Render (resolve references and templates)
        context->render();
        
        // Output JSON
        std::cout << context->to_json() << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
