#include "Compiler.hpp"
#include "Compiler/CodeGenerator.hpp"
#include "SemanticAnalyzer/SemanticAnalyzer.hpp"

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

    lang::SemanticAnalyzer sa;
    sa.openFile(file);
    sa.parse();

    if (!sa.getAST() || !sa.getSymbolTable()) return output;

    lang::CodeGenerator cg(sa.getAST(), sa.getSymbolTable());
    output.assembly = cg.generate();
    return output;
}
