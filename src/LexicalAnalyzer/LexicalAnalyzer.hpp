#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

#define TAB_SIZE 4
#define NEXT_TAB_POS(pos) TAB_SIZE - ((pos - 1) % TAB_SIZE)

namespace lang
{
    enum class TokenType {
        BLOCK_COMMENT,
        INLINE_COMMENT,
        INT_NUM,
        FLOAT_NUM,
        ID,
        EQUAL,
        NOT_EQUAL,
        LESS_THAN,
        GREATER_THAN,
        LESS_EQUAL,
        GREATER_EQUAL,
        PLUS,
        MINUS,
        MULTIPLY,
        DIVIDE,
        ASSIGN,
        OPEN_PARENTHESIS,
        CLOSE_PARENTHESIS,
        OPEN_BRACE,
        CLOSE_BRACE,
        OPEN_BRACKET,
        CLOSE_BRACKET,
        SEMICOLON,
        COMMA,
        DOT,
        COLON,
        DOUBLE_COLON,
        IF,
        THEN,
        ELSE,
        WHILE,
        CLASS,
        INTEGER,
        FLOAT,
        DO,
        END,
        PUBLIC,
        PRIVATE,
        OR,
        AND,
        NOT,
        READ,
        WRITE,
        RETURN,
        INHERITS,
        LOCAL,
        VOID,
        MAIN,
        END_OF_FILE,
        UNKNOWN
    };

    using Token = struct {
        TokenType type;
        std::string lexeme;
        std::uint64_t line;
        std::uint64_t pos;
    };

    std::string tokenTypeToString(TokenType type);

    class LexicalAnalyzer
    {
    public:
        LexicalAnalyzer() = default;
        ~LexicalAnalyzer() = default;

        void readFile(std::string_view path);

        Token next();

    private:
        std::string m_file_contents;
        std::uint64_t m_lineNumber{ 1 };
        std::uint64_t m_position{ 1 };
        std::uint64_t m_iter{ 0 };

        static constexpr std::array<std::pair<TokenType, std::string_view>, 5> m_RegExPatterns{
            { { TokenType::BLOCK_COMMENT, R"(^\/\*[\s\S]*?\*\/)" },
              { TokenType::INLINE_COMMENT, R"(^\/\/.*$)" },
              { TokenType::FLOAT_NUM, R"(^([1-9][0-9]*|0)\.([0-9]*[1-9]|0)(e(\+|\-)?([1-9][0-9]*|0))?)" },
              { TokenType::INT_NUM, R"(^([1-9][0-9]*)|0)" },
              { TokenType::ID, R"(^[a-zA-Z]([a-zA-Z]|[0-9]|_)*)" } }
        };

        static constexpr std::array<std::pair<std::string_view, TokenType>, 21> m_Keywords{
            { { "if", TokenType::IF },         { "then", TokenType::THEN },         { "else", TokenType::ELSE },       { "while", TokenType::WHILE },
              { "class", TokenType::CLASS },   { "integer", TokenType::INTEGER },   { "float", TokenType::FLOAT },     { "do", TokenType::DO },
              { "end", TokenType::END },       { "public", TokenType::PUBLIC },     { "private", TokenType::PRIVATE }, { "or", TokenType::OR },
              { "and", TokenType::AND },       { "not", TokenType::NOT },           { "read", TokenType::READ },       { "write", TokenType::WRITE },
              { "return", TokenType::RETURN }, { "inherits", TokenType::INHERITS }, { "local", TokenType::LOCAL },     { "void", TokenType::VOID },
              { "main", TokenType::MAIN } }
        };

        static constexpr std::array<std::pair<std::string_view, TokenType>, 22> m_Operators{ { { "==", TokenType::EQUAL },
                                                                                               { "<>", TokenType::NOT_EQUAL },
                                                                                               { "<=", TokenType::LESS_EQUAL },
                                                                                               { ">=", TokenType::GREATER_EQUAL },
                                                                                               { "::", TokenType::DOUBLE_COLON },
                                                                                               { "<", TokenType::LESS_THAN },
                                                                                               { ">", TokenType::GREATER_THAN },
                                                                                               { "+", TokenType::PLUS },
                                                                                               { "-", TokenType::MINUS },
                                                                                               { "*", TokenType::MULTIPLY },
                                                                                               { "/", TokenType::DIVIDE },
                                                                                               { "=", TokenType::ASSIGN },
                                                                                               { "(", TokenType::OPEN_PARENTHESIS },
                                                                                               { ")", TokenType::CLOSE_PARENTHESIS },
                                                                                               { "{", TokenType::OPEN_BRACE },
                                                                                               { "}", TokenType::CLOSE_BRACE },
                                                                                               { ";", TokenType::SEMICOLON },
                                                                                               { ",", TokenType::COMMA },
                                                                                               { ".", TokenType::DOT },
                                                                                               { ":", TokenType::COLON },
                                                                                               { "[", TokenType::OPEN_BRACKET },
                                                                                               { "]", TokenType::CLOSE_BRACKET } } };

        Token makeToken(TokenType type, std::string lexeme);

        Token runRegEx(const std::string &sub);
        Token checkKeywords(lang::Token &token);
        Token checkOperators(const std::string &sub);
    };
} // namespace lang
