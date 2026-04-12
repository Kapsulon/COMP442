#include "Compiler.hpp"
#include "Compiler/CodeGenerator.hpp"
#include "LexicalAnalyzer/LexicalAnalyzer.hpp"
#include "Problems/Problems.hpp"
#include "SemanticAnalyzer/SemanticAnalyzer.hpp"

#include <format>
#include <regex>

static std::string makeTokensText(const std::vector<lang::Token> &tokens)
{
    std::string out;
    std::uint64_t current_line = 1;
    bool first_token_of_line = true;

    for (const auto &token : tokens) {
        while (token.line > current_line) {
            out += '\n';
            current_line++;
            first_token_of_line = true;
        }
        std::string lexeme = std::regex_replace(token.lexeme, std::regex("\n"), "\\n");
        std::string entry = std::format("[{}, {}, {}:{}]", lang::tokenTypeToString(token.type), lexeme, token.line, token.pos);
        if (!first_token_of_line)
            out += ' ';
        out += entry;
        first_token_of_line = false;
    }

    return out;
}

static std::string makeTokensFlaciText(const std::vector<lang::Token> &tokens)
{
    std::string out;
    for (const auto &token : tokens) {
        if (token.type == lang::TokenType::INLINE_COMMENT || token.type == lang::TokenType::BLOCK_COMMENT || token.type == lang::TokenType::UNKNOWN)
            continue;
        out += lang::tokenTypeToCompString(token.type);
        out += '\n';
    }
    return out;
}

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

    const auto &synAna = sa.getSyntacticAnalyzer();
    const auto &lexer = synAna.getLexer();

    // Merge all problems: lex+syntax from syntactic analyzer, semantic from semantic analyzer
    lang::Problems allProblems;
    allProblems.merge(synAna.getProblems());
    allProblems.merge(sa.getSemanticProblems());
    output.errors_text = allProblems.getProblems(lexer);  // sorted by line+col

    // Optional intermediate outputs (only populated when the flag is set)
    if (m_settings.emit_tokens)
        output.tokens_text = makeTokensText(synAna.getRawTokens());
    if (m_settings.emit_tokens_flaci)
        output.tokens_flaci_text = makeTokensFlaciText(synAna.getRawTokens());
    if (m_settings.emit_derivation)
        output.derivation_text = std::string(synAna.getDerivationSteps());
    if (m_settings.emit_ast)
        output.ast_dot_text = synAna.getDotASTString();
    if (m_settings.emit_symbol_tables)
        output.symbol_table_text = sa.renderSymbolTable();

    // Code generation (while sa is still alive)
    if (sa.getAST() && sa.getSymbolTable()) {
        lang::CodeGenerator cg(sa.getAST(), sa.getSymbolTable());
        output.assembly = cg.generate();
    }

    return output;
    // sa destroyed here — all data already captured in output
}
