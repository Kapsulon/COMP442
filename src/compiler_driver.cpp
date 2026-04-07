#include "Compiler/Compiler.hpp"

#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char *const *const argv)
{
    argparse::ArgumentParser parser("compiler");

    Compiler::Settings compiler_settings;

    parser.add_argument("files")
        .help("Files to compile to moon assembly code.")
        .nargs(argparse::nargs_pattern::at_least_one)
        .store_into(compiler_settings.files);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &err) {
        spdlog::error(err.what());
        return 1;
    }

    Compiler compiler(compiler_settings);

    return 0;
}
