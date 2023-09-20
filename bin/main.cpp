#include "lib/parser.h"

#include <iostream>

int main(int, char**) {
    const auto root = omfl::parse(std::filesystem::path("../../example/config.omfl"));
    
    if (!root.valid()) {
        std::cout << "Bad file!";

        return 0;
    }

    std::cout << root.Get("common.name").AsString() << "\n";
    std::cout << root.Get("common").Get("description").AsString() << "\n";
    std::cout << root.Get("servers").Get("first.enabled").AsBool() << "\n";

    return 0;
}
