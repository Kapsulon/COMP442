#include "Compiler/Compiler.hpp"

#include <filesystem>
#include <fstream>

#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char *const *const argv)
{
    argparse::ArgumentParser parser("compiler");

    Compiler::Settings compiler_settings;

    parser.add_argument("files")
        .help("Files to compile to MOON assembly.")
        .nargs(argparse::nargs_pattern::at_least_one)
        .store_into(compiler_settings.files);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &err) {
        spdlog::error(err.what());
        return 1;
    }

    Compiler compiler(compiler_settings);
    auto results = compiler.compileAll();

    for (auto &[file, out] : results) {
        if (out.assembly.empty()) {
            spdlog::warn("No assembly generated for {}", file);
            continue;
        }
        std::filesystem::path p(file);
        p.replace_extension(".moon");
        std::ofstream f(p);
        if (!f) {
            spdlog::error("Cannot write {}", p.string());
            continue;
        }
        f << out.assembly;
        spdlog::info("Wrote {}", p.string());
    }

    return 0;
}
