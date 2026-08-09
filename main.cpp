#include "compiler.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.cmm> <output.s>\n";
        return 1;
    }
    Compiler compiler(argv[1], argv[2]);
    compiler.compile();
    return compiler.has_error ? 1 : 0;
}
