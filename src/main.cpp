#include <unistd.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <system_error>

#include "App.hpp"

int main(int argc, char** argv) {
    std::optional<std::string> syntax_style;
    std::optional<std::filesystem::path> syntax_dir;
    std::optional<std::filesystem::path> start_path;
    for (int i = 1; i < argc; ++i) {
        std::string_view argument = argv[i];
        if (argument == "--syntax") {
            if (++i == argc) {
                std::cerr << "lss: --syntax requires a style name\n";
                return 1;
            }
            syntax_style = argv[i];
        } else if (argument == "--syntax-dir") {
            if (++i == argc) {
                std::cerr << "lss: --syntax-dir requires a path\n";
                return 1;
            }
            syntax_dir = argv[i];
        } else if (argument.starts_with('-') || start_path) {
            std::cerr << "usage: lss [--syntax style] [--syntax-dir path] [path]\n";
            return 1;
        } else {
            start_path = argv[i];
        }
    }

    if (!start_path && !isatty(fileno(stdin))) {
        std::ostringstream buffer;
        buffer << std::cin.rdbuf();

        try {
            listless::App app(buffer.str(), "(stdin)", std::move(syntax_style),
                              std::move(syntax_dir));
            return app.run();
        } catch (const std::exception& e) {
            std::cerr << "lss: " << e.what() << "\n";
            return 1;
        }
    }

    std::filesystem::path path;
    if (start_path) {
        path = *start_path;

        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            std::cerr << "lss: " << path.string() << ": no such file or directory\n";
            return 1;
        }
    }

    try {
        listless::App app(path, std::move(syntax_style), std::move(syntax_dir));
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "lss: " << e.what() << "\n";
        return 1;
    }
}
