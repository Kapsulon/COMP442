#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <vector>

#include "LexicalAnalyzer/LexicalAnalyzer.hpp"
#include "spdlog/spdlog.h"

static std::ofstream openOutputFile(const std::string &path, const std::string &ext)
{
    std::string out_path = path;
    size_t ext_pos = out_path.rfind(".src");
    if (ext_pos != std::string::npos) {
        out_path.replace(ext_pos, 4, ext);
    } else {
        out_path += ext;
    }

    std::ofstream file(out_path);

    if (!file.is_open()) {
        throw std::runtime_error("Couldn't open output file: " + out_path);
    }

    return file;
}

static void outputTokens(std::ofstream &out, std::vector<lang::Token> &tokens)
{
    std::uint64_t current_line = 1;
    bool first_token_of_line = true;

    for (auto &token : tokens) {
        while (token.line > current_line) {
            out.write("\n", 1);
            current_line++;
            first_token_of_line = true;
        }
        std::string format = std::format("[{}, {}, {}:{}]", lang::tokenTypeToString(token.type), token.lexeme, token.line, token.pos);
        if (!first_token_of_line)
            out.write(" ", 1);
        out.write(format.c_str(), format.size());
        first_token_of_line = false;
    }
}

static void outputTokensFlaci(std::ofstream &out, std::vector<lang::Token> &tokens)
{
    for (auto &token : tokens) {
        if (token.type == lang::TokenType::INLINE_COMMENT || token.type == lang::TokenType::BLOCK_COMMENT || token.type == lang::TokenType::UNKNOWN)
            continue;
        out.write((token.lexeme + "\n").c_str(), token.lexeme.size() + 1);
    }
}

static void outputErrors(std::ofstream &out, std::vector<lang::Token> &tokens)
{
    for (auto &token : tokens) {
        if (token.type == lang::TokenType::UNKNOWN) {
            std::string format = std::format("Error: Unknown token '{}' at line {}, position {}\n", token.lexeme, token.line, token.pos);
            out.write(format.c_str(), format.size());
        }
    }
}

static bool lexFile(const std::string &path, std::vector<lang::Token> &tokens)
{
    lang::LexicalAnalyzer lexer{};

    try {
        lexer.readFile(path);
    } catch (const std::exception &e) {
        spdlog::error(e.what());
        return false;
    }

    for (lang::Token token = lexer.next(); token.type != lang::TokenType::END_OF_FILE; token = lexer.next()) {
        tokens.push_back(token);
    }

    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }

    std::vector<lang::Token> tokens;

    for (std::uint64_t idx = 1; idx != argc; idx++) {
        tokens.clear();
        auto start = std::chrono::high_resolution_clock::now();
        bool res = lexFile(argv[idx], tokens);
        auto end = std::chrono::high_resolution_clock::now();

        for (auto &token : tokens) {
            token.lexeme = std::regex_replace(token.lexeme, std::regex("\n"), "\\n");
        }

        std::ofstream outlextokens;
        std::ofstream outlextokensflaci;
        std::ofstream outlexerrors;

        try {
            outlextokens = openOutputFile(argv[idx], ".outlextokens");
            outlextokensflaci = openOutputFile(argv[idx], ".outlextokensflaci");
            outlexerrors = openOutputFile(argv[idx], ".outlexerrors");
        } catch (const std::exception &e) {
            spdlog::error(e.what());
            return -1;
        }

        outputTokens(outlextokens, tokens);
        outputTokensFlaci(outlextokensflaci, tokens);
        outputErrors(outlexerrors, tokens);

        std::chrono::duration<double, std::milli> elapsed_ms = end - start;

        if (res)
            spdlog::info(R"(Lexed file "{}" ({} ms))", argv[idx], elapsed_ms.count());
        else
            spdlog::error(R"(Error lexing file "{}" ({} ms))", argv[idx], elapsed_ms.count());
    }

    return 0;
}
