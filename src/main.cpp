#include <iostream>
#include <string>
#include "tyco/parser.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    
    try {
        // Load and parse the Tyco file
        tyco::TycoContext context = tyco::load(filename);
        
        if (context.empty()) {
            std::cout << "No configuration found or file is empty." << std::endl;
            return 0;
        }
        
        // Display global variables
        std::cout << "Global Variables:" << std::endl;
        for (const auto& name : context.get_global_names()) {
            const auto& value = context.get_global(name);
            std::cout << "  " << name << ": ";
            
            if (value.is_string()) {
                std::cout << "\"" << value.as_string() << "\"";
            } else if (value.is_int()) {
                std::cout << value.as_int();
            } else if (value.is_float()) {
                std::cout << value.as_float();
            } else if (value.is_bool()) {
                std::cout << (value.as_bool() ? "true" : "false");
            } else if (value.is_array()) {
                std::cout << "[";
                const auto& array = value.as_array();
                for (size_t i = 0; i < array.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    const auto& item = array[i];
                    if (item.is_string()) {
                        std::cout << "\"" << item.as_string() << "\"";
                    } else if (item.is_int()) {
                        std::cout << item.as_int();
                    } else if (item.is_float()) {
                        std::cout << item.as_float();
                    } else if (item.is_bool()) {
                        std::cout << (item.as_bool() ? "true" : "false");
                    }
                }
                std::cout << "]";
            } else {
                std::cout << "[object]";
            }
            std::cout << std::endl;
        }
        
        // Display object types
        std::cout << "\nObject Types:" << std::endl;
        for (const auto& type : context.get_object_types()) {
            const auto& objects = context.get_objects(type);
            std::cout << "  " << type << ": " << objects.size() << " instances" << std::endl;
            
            for (size_t i = 0; i < objects.size(); ++i) {
                std::cout << "    [" << i << "]: ";
                if (objects[i].is_object()) {
                    const auto& obj = objects[i].as_object();
                    bool first = true;
                    for (const auto& pair : obj) {
                        if (!first) std::cout << ", ";
                        std::cout << pair.first << "=";
                        
                        const auto& val = pair.second;
                        if (val.is_string()) {
                            std::cout << "\"" << val.as_string() << "\"";
                        } else if (val.is_int()) {
                            std::cout << val.as_int();
                        } else if (val.is_float()) {
                            std::cout << val.as_float();
                        } else if (val.is_bool()) {
                            std::cout << (val.as_bool() ? "true" : "false");
                        }
                        first = false;
                    }
                }
                std::cout << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}