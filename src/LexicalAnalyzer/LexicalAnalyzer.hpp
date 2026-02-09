#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include "ctre.hpp"

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
        ~LexicalAnalyzer()
        {
            closeFile();
        }

        std::uint64_t readFile(std::string_view path);
        void closeFile();

        Token next();
        float getProgress() const;

    private:
        int m_fd{ -1 };
        void *m_data{ nullptr };

        std::string_view m_file_contents;
        std::string_view m_slice;
        std::uint64_t m_lineNumber{ 1 };
        std::uint64_t m_position{ 1 };
        std::uint64_t m_iter{ 0 };

        using LexRule = struct {
            TokenType type;
            std::uint32_t (*match)(std::string_view);
        };

        static constexpr auto REGEX_BLOCK_COMMENT = ctre::multiline_starts_with<R"(\/\*[\s\S]*?\*\/)">;
        static constexpr auto REGEX_INLINE_COMMENT = ctre::multiline_starts_with<R"(\/\/.*$)">;
        static constexpr auto REGEX_FLOAT_NUM = ctre::starts_with<R"(([1-9][0-9]*|0)\.([0-9]*[1-9]|0)(e(\+|\-)?([1-9][0-9]*|0))?)">;
        static constexpr auto REGEX_INT_NUM = ctre::starts_with<R"(([1-9][0-9]*)|0)">;
        static constexpr auto REGEX_ID = ctre::starts_with<R"([a-zA-Z]([a-zA-Z]|[0-9]|_)*)">;

        static std::uint32_t MatchBlockComment(std::string_view s);
        static std::uint32_t MatchInlineComment(std::string_view s);
        static std::uint32_t MatchFloatNum(std::string_view s);
        static std::uint32_t MatchIntNum(std::string_view s);
        static std::uint32_t MatchID(std::string_view s);

        static constexpr LexRule rules[] = { { TokenType::BLOCK_COMMENT, MatchBlockComment },
                                             { TokenType::INLINE_COMMENT, MatchInlineComment },
                                             { TokenType::FLOAT_NUM, MatchFloatNum },
                                             { TokenType::INT_NUM, MatchIntNum },
                                             { TokenType::ID, MatchID } };

        static const std::unordered_map<std::string, TokenType> m_Keywords;

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

        Token runRegEx();
        Token checkKeywords(lang::Token &token);
        Token checkOperators();
    };
} // namespace lang
