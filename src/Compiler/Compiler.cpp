#include "Compiler.hpp"

Compiler::Compiler(const Settings &settings) : m_settings(settings) {}

std::unordered_map<std::string, Compiler::Output> Compiler::compileAll()
{
    std::unordered_map<std::string, Compiler::Output> out;

    for (auto &file : m_settings.files) out[file] = compile(file);

    return out;
}

Compiler::Output Compiler::compile(const std::string &file)
{
    Output output = { .source_file = file };

    return output;
}
