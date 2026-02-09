#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <vector>

#include "LexicalAnalyzer/LexicalAnalyzer.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }

    // std::vector<lang::Token> tokens;

    // for (std::uint64_t idx = 1; idx != argc; idx++) {
    //     tokens.clear();
    //     bool res = lexFile(argv[idx], tokens);

    //     for (auto &token : tokens) {
    //         token.lexeme = std::regex_replace(token.lexeme, std::regex("\n"), "\\n");
    //     }

    //     std::ofstream outlextokens;
    //     std::ofstream outlextokensflaci;
    //     std::ofstream outlexerrors;

    //     try {
    //         outlextokens = openOutputFile(argv[idx], ".outlextokens");
    //         outlextokensflaci = openOutputFile(argv[idx], ".outlextokensflaci");
    //         outlexerrors = openOutputFile(argv[idx], ".outlexerrors");
    //     } catch (const std::exception &e) {
    //         spdlog::error(e.what());
    //         return -1;
    //     }

    //     outputTokens(outlextokens, tokens);
    //     outputTokensFlaci(outlextokensflaci, tokens);
    //     outputErrors(outlexerrors, tokens);

    //     if (!res)
    //         spdlog::error(R"(Error lexing file "{}")", argv[idx]);
    // }

    return 0;
}
