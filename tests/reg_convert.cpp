#include "registry.h"
#include <iostream>
#include <fstream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: reg_convert <input> <output>\n";
        return 1;
    }

    rsjfw::RegistryKey root;
    std::ifstream is(argv[1]);
    if (!is.is_open()) {
        std::cerr << "Failed to open input\n";
        return 1;
    }

    std::cout << "Loading..." << std::endl;
    if (!root.load(is)) {
        std::cerr << "Failed to load registry\n";
        return 1;
    }
    std::cout << "Loaded. Subkeys: " << root.subkeys.size() << std::endl;

    std::ofstream os(argv[2]);
    if (!os.is_open()) {
        std::cerr << "Failed to open output\n";
        return 1;
    }

    std::cout << "Saving..." << std::endl;
    os << "WINE REGISTRY Version 2\n;; All keys relative to REGISTRY\\Machine\n\n#arch=win64\n\n";
    root.save(os, "");
    std::cout << "Done.\n";
    return 0;
}
