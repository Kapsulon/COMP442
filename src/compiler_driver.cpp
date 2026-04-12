#include "Compiler/Compiler.hpp"

#include <filesystem>
#include <fstream>

#include "argparse/argparse.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char *const *const argv)
{
    argparse::ArgumentParser parser("compiler");

    Compiler::Settings compiler_settings;

    parser.add_argument("files").help("Files to compile to MOON assembly.").nargs(argparse::nargs_pattern::at_least_one).store_into(compiler_settings.files);

    parser.add_argument("--tokens").help("Write token stream to .outlextokens").flag().store_into(compiler_settings.emit_tokens);

    parser.add_argument("--tokens-flaci").help("Write filtered token types to .outlextokensflaci").flag().store_into(compiler_settings.emit_tokens_flaci);

    parser.add_argument("--derivation").help("Write LL(1) derivation steps to .outderivation").flag().store_into(compiler_settings.emit_derivation);

    parser.add_argument("--ast").help("Write Graphviz DOT AST to .outast").flag().store_into(compiler_settings.emit_ast);

    parser.add_argument("--symbol-tables").help("Write symbol table to .outsymboltables").flag().store_into(compiler_settings.emit_symbol_tables);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception &err) {
        spdlog::error(err.what());
        return 1;
    }

    Compiler compiler(compiler_settings);
    auto results = compiler.compileAll();

    for (auto &[file, out] : results) {
        if (!out.errors_text.empty()) {
            auto errPath = std::filesystem::path(file);
            errPath.replace_extension(".outerrors");
            std::ofstream f(errPath);
            if (f)
                f << out.errors_text;
            else
                spdlog::error("Cannot write {}", errPath.string());
        }

        auto writeOptional = [&](const std::string &content, std::string_view ext) {
            if (content.empty())
                return;
            auto p = std::filesystem::path(file);
            p.replace_extension(ext);
            std::ofstream f(p);
            if (f)
                f << content;
            else
                spdlog::error("Cannot write {}", p.string());
        };

        writeOptional(out.tokens_text, ".outlextokens");
        writeOptional(out.tokens_flaci_text, ".outlextokensflaci");
        writeOptional(out.derivation_text, ".outderivation");
        writeOptional(out.ast_dot_text, ".outast");
        writeOptional(out.symbol_table_text, ".outsymboltables");

        if (out.assembly.empty()) {
            spdlog::warn("No assembly generated for {}", file);
            continue;
        }
        auto moonPath = std::filesystem::path(file);
        moonPath.replace_extension(".moon");
        std::ofstream f(moonPath);
        if (!f) {
            spdlog::error("Cannot write {}", moonPath.string());
            continue;
        }
        f << out.assembly;
        spdlog::info("Wrote {}", moonPath.string());
    }

    return 0;
}
