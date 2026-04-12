#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class Compiler
{
public:
    struct Settings {
        std::vector<std::string> files;

        bool emit_tokens        = false;  // --tokens        → .outlextokens
        bool emit_tokens_flaci  = false;  // --tokens-flaci  → .outlextokensflaci
        bool emit_derivation    = false;  // --derivation    → .outderivation
        bool emit_ast           = false;  // --ast           → .outast
        bool emit_symbol_tables = false;  // --symbol-tables → .outsymboltables
    };

    struct Output {
        std::string source_file;
        std::string assembly;           // .moon — empty if compilation failed
        std::string errors_text;        // .outerrors — unified, sorted by line+col; empty if no errors
        std::string tokens_text;        // .outlextokens
        std::string tokens_flaci_text;  // .outlextokensflaci
        std::string derivation_text;    // .outderivation
        std::string ast_dot_text;       // .outast
        std::string symbol_table_text;  // .outsymboltables
    };

    Compiler(const Settings &settings);

    std::unordered_map<std::string, Output> compileAll();

private:
    Output compile(const std::string &file);

    const Settings m_settings;
};
