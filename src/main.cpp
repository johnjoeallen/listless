#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <system_error>

#include "app.hpp"

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "usage: lss [path]\n";
        return 1;
    }

    std::filesystem::path start_path;
    if (argc == 2) {
        start_path = argv[1];

        std::error_code ec;
        if (!std::filesystem::exists(start_path, ec)) {
            std::cerr << "lss: " << start_path.string() << ": no such file or directory\n";
            return 1;
        }
    }

    try {
        listless::App app(start_path);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "lss: " << e.what() << "\n";
        return 1;
    }
}
