#include <chrono>
#include <format>
#include <iostream>

#include "LexicalAnalyzer/LexicalAnalyzer.hpp"
#include "SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

namespace lang
{
    SyntacticAnalyzer::SyntacticAnalyzer(bool outputFiles) :
        m_firstSet(generateFirstSet()), m_followSet(generateFollowSet()), m_parseTable(generateParseTable()), m_outputFiles(outputFiles)
    {
    }

    void SyntacticAnalyzer::openFile(std::string_view path)
    {
        closeFile();
        m_currentFilePath = path;

        m_tokens.resize(0);
        std::uint64_t read_bytes = m_lexicalAnalyzer.readFile(path);
        m_tokens.reserve(read_bytes / 10);

        lex();

        m_tokens.erase(
            std::remove_if(
                m_tokens.begin(),
                m_tokens.end(),
                [](const Token &token) { return token.type == TokenType::INLINE_COMMENT || token.type == TokenType::BLOCK_COMMENT; }),
            m_tokens.end());

        if (m_outputFiles) {
            std::string errorFilePath = std::string(m_currentFilePath) + ".outsyntaxerrors";
            m_outParseErrors.open(errorFilePath, std::ios::out);
            if (!m_outParseErrors.is_open()) {
                spdlog::error("Failed to open output file for parse errors: {}", errorFilePath);
                m_outputFiles = false;
            }

            std::string derivationFilePath = std::string(m_currentFilePath) + ".outderivation";
            m_outDerivation.open(derivationFilePath, std::ios::out);
            if (!m_outDerivation.is_open()) {
                spdlog::error("Failed to open output file for derivation: {}", derivationFilePath);
                m_outputFiles = false;
            }
        }
    }

    void SyntacticAnalyzer::closeFile()
    {
        m_lexicalAnalyzer.closeFile();

        if (m_outputFiles)
            m_outParseErrors.close();

        if (m_outputFiles)
            m_outDerivation.close();
    }

    void SyntacticAnalyzer::lex()
    {
        auto start = std::chrono::high_resolution_clock::now();

        while (true) {
            Token token = m_lexicalAnalyzer.next();
            m_tokens.push_back(token);

            if (token.type == TokenType::END_OF_FILE) {
                break;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;

        spdlog::info("{}: Lexed {} tokens in {:.2f}ms", m_currentFilePath, m_tokens.size(), elapsed.count());
    }

    // clang-format off
    const Grammar SyntacticAnalyzer::grammar = {
        {
            NonTerminal::START, {
                {Symbol::N(NonTerminal::prog)}
            }
        },
        {
            NonTerminal::aParams, {
                {Symbol::N(NonTerminal::expr), Symbol::N(NonTerminal::aParamsTail)},
                EPSILON
            }
        },
        {
            NonTerminal::aParamsTail, {
                {Symbol::T(TokenType::COMMA), Symbol::N(NonTerminal::expr), Symbol::N(NonTerminal::aParamsTail)},
                EPSILON
            }
        },
        {
            NonTerminal::addOp, {
                {Symbol::T(TokenType::PLUS)},
                {Symbol::T(TokenType::MINUS)},
                {Symbol::T(TokenType::OR)}
            }
        },
        {
            NonTerminal::arithExpr, {
                {Symbol::N(NonTerminal::term), Symbol::N(NonTerminal::arithExprTail)}
            }
        },
        {
            NonTerminal::arithExprTail, {
                {Symbol::N(NonTerminal::addOp), Symbol::N(NonTerminal::term), Symbol::N(NonTerminal::arithExprTail)},
                EPSILON
            }
        },
        {
            NonTerminal::arraySize, {
                {Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arraySizeTail)}
            }
        },
        {
            NonTerminal::arraySizeTail, {
                {Symbol::T(TokenType::INT_NUM), Symbol::T(TokenType::CLOSE_BRACKET)},
                {Symbol::T(TokenType::CLOSE_BRACKET)}
            }
        },
        {
            NonTerminal::assignOp, {
                {Symbol::T(TokenType::ASSIGN)}
            }
        },
        {
            NonTerminal::classDecl, {
                {Symbol::T(TokenType::CLASS), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::classInheritOpt), Symbol::T(TokenType::OPEN_BRACE), Symbol::N(NonTerminal::classMemberList), Symbol::T(TokenType::CLOSE_BRACE), Symbol::T(TokenType::SEMICOLON)}
            }
        },
        {
            NonTerminal::classInheritOpt, {
                {Symbol::T(TokenType::INHERITS), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::classInheritTail)},
                EPSILON
            }
        },
        {
            NonTerminal::classInheritTail, {
                {Symbol::T(TokenType::COMMA), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::classInheritTail)},
                EPSILON
            }
        },
        {
            NonTerminal::classList, {
                {Symbol::N(NonTerminal::classDecl), Symbol::N(NonTerminal::classList)},
                EPSILON
            }
        },
        {
            NonTerminal::classMemberList, {
                {Symbol::N(NonTerminal::visibility), Symbol::N(NonTerminal::memberDecl), Symbol::N(NonTerminal::classMemberList)},
                EPSILON
            }
        },
        {
            NonTerminal::expr, {
                {Symbol::N(NonTerminal::arithExpr), Symbol::N(NonTerminal::exprRelTail)}
            }
        },
        {
            NonTerminal::exprRelTail, {
                {Symbol::N(NonTerminal::relOp), Symbol::N(NonTerminal::arithExpr)},
                EPSILON
            }
        },
        {
            NonTerminal::fParams, {
                {Symbol::N(NonTerminal::type), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::fParamsArrayList), Symbol::N(NonTerminal::fParamsTail)},
                EPSILON
            }
        },
        {
            NonTerminal::fParamsArrayList, {
                {Symbol::N(NonTerminal::arraySize), Symbol::N(NonTerminal::fParamsArrayList)},
                EPSILON
            }
        },
        {
            NonTerminal::fParamsTail, {
                {Symbol::T(TokenType::COMMA), Symbol::N(NonTerminal::type), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::fParamsArrayList), Symbol::N(NonTerminal::fParamsTail)},
                EPSILON
            }
        },
        {
            NonTerminal::factor, {
                {Symbol::T(TokenType::ID), Symbol::N(NonTerminal::postfixList)},
                {Symbol::T(TokenType::INT_NUM)},
                {Symbol::T(TokenType::FLOAT_NUM)},
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_PARENTHESIS)},
                {Symbol::T(TokenType::NOT), Symbol::N(NonTerminal::factor)},
                {Symbol::N(NonTerminal::sign), Symbol::N(NonTerminal::factor)}
            }
        },
        {
            NonTerminal::funcBody, {
                {Symbol::N(NonTerminal::localDeclOpt), Symbol::T(TokenType::DO), Symbol::N(NonTerminal::stmtList), Symbol::T(TokenType::END)}
            }
        },
        {
            NonTerminal::funcDeclTail, {
                {Symbol::N(NonTerminal::type), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::VOID), Symbol::T(TokenType::SEMICOLON)}
            }
        },
        {
            NonTerminal::funcDef, {
                {Symbol::N(NonTerminal::funcHead), Symbol::N(NonTerminal::funcBody)}
            }
        },
        {
            NonTerminal::funcDefList, {
                {Symbol::N(NonTerminal::funcDef), Symbol::N(NonTerminal::funcDefList)},
                EPSILON
            }
        },
        {
            NonTerminal::funcHead, {
                {Symbol::T(TokenType::ID), Symbol::N(NonTerminal::funcHeadTail)}
            }
        },
        {
            NonTerminal::funcHeadReturn, {
                {Symbol::N(NonTerminal::type)},
                {Symbol::T(TokenType::VOID)}
            }
        },
        {
            NonTerminal::funcHeadTail, {
                {Symbol::T(TokenType::DOUBLE_COLON), Symbol::T(TokenType::ID), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcHeadReturn)},
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcHeadReturn)}
            }
        },
        {
            NonTerminal::indice, {
                {Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET)}
            }
        },
        {
            NonTerminal::localDeclOpt, {
                {Symbol::T(TokenType::LOCAL), Symbol::N(NonTerminal::varDeclList)},
                EPSILON
            }
        },
        {
            NonTerminal::memberDecl, {
                {Symbol::N(NonTerminal::type_no_id), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::varArrayList), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::ID), Symbol::N(NonTerminal::memberAfterId)}
            }
        },
        {
            NonTerminal::memberAfterId, {
                {Symbol::T(TokenType::ID), Symbol::N(NonTerminal::varArrayList), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcDeclTail)},
            }
        },
        {
            NonTerminal::multOp, {
                {Symbol::T(TokenType::MULTIPLY)},
                {Symbol::T(TokenType::DIVIDE)},
                {Symbol::T(TokenType::AND)}
            }
        },
        {
            NonTerminal::postfix, {
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::aParams), Symbol::T(TokenType::CLOSE_PARENTHESIS)},
                {Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET)},
                {Symbol::T(TokenType::DOT), Symbol::T(TokenType::ID)}
            }
        },
        {
            NonTerminal::postfixList, {
                {Symbol::N(NonTerminal::postfix), Symbol::N(NonTerminal::postfixList)},
                EPSILON
            }
        },
        {
            NonTerminal::postfixListNoCall, {
                {Symbol::N(NonTerminal::postfixNoCall), Symbol::N(NonTerminal::postfixListNoCall)},
                EPSILON
            }
        },
        {
            NonTerminal::postfixNoCall, {
                {Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET)},
                {Symbol::T(TokenType::DOT), Symbol::T(TokenType::ID)}
            }
        },
        {
            NonTerminal::prog, {
                {Symbol::N(NonTerminal::classList), Symbol::N(NonTerminal::funcDefList), Symbol::T(TokenType::MAIN), Symbol::N(NonTerminal::funcBody)}
            }
        },
        {
            NonTerminal::relOp, {
                {Symbol::T(TokenType::EQUAL)},
                {Symbol::T(TokenType::NOT_EQUAL)},
                {Symbol::T(TokenType::LESS_THAN)},
                {Symbol::T(TokenType::GREATER_THAN)},
                {Symbol::T(TokenType::LESS_EQUAL)},
                {Symbol::T(TokenType::GREATER_EQUAL)}
            }
        },
        {
            NonTerminal::sign, {
                {Symbol::T(TokenType::PLUS)},
                {Symbol::T(TokenType::MINUS)}
            }
        },
        {
            NonTerminal::statBlock, {
                {Symbol::T(TokenType::DO), Symbol::N(NonTerminal::stmtList), Symbol::T(TokenType::END)},
                {Symbol::N(NonTerminal::statement)},
                EPSILON
            }
        },
        {
            NonTerminal::statement, {
                {Symbol::T(TokenType::ID), Symbol::N(NonTerminal::postfixList), Symbol::N(NonTerminal::statementEnd)},
                {Symbol::T(TokenType::IF), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::THEN), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::ELSE), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::WHILE), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::READ), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::variable), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::WRITE), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::RETURN), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON)}
            }
        },
        {
            NonTerminal::statementEnd, {
                {Symbol::N(NonTerminal::assignOp), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::SEMICOLON)}
            }
        },
        {
            NonTerminal::stmtList, {
                {Symbol::N(NonTerminal::statement), Symbol::N(NonTerminal::stmtList)},
                EPSILON
            }
        },
        {
            NonTerminal::term, {
                {Symbol::N(NonTerminal::factor), Symbol::N(NonTerminal::termTail)}
            }
        },
        {
            NonTerminal::termTail, {
                {Symbol::N(NonTerminal::multOp), Symbol::N(NonTerminal::factor), Symbol::N(NonTerminal::termTail)},
                EPSILON
            }
        },
        {
            NonTerminal::type, {
                {Symbol::T(TokenType::INTEGER)},
                {Symbol::T(TokenType::FLOAT)},
                {Symbol::T(TokenType::ID)}
            }
        },
        {
            NonTerminal::type_no_id, {
                {Symbol::T(TokenType::INTEGER)},
                {Symbol::T(TokenType::FLOAT)}
            }
        },
        {
            NonTerminal::varArrayList, {
                {Symbol::N(NonTerminal::arraySize), Symbol::N(NonTerminal::varArrayList)},
                EPSILON
            }
        },
        {
            NonTerminal::varDecl, {
                {Symbol::N(NonTerminal::type), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::varArrayList), Symbol::T(TokenType::SEMICOLON)}
            }
        },
        {
            NonTerminal::varDeclList, {
                {Symbol::N(NonTerminal::varDecl), Symbol::N(NonTerminal::varDeclList)},
                EPSILON
            }
        },
        {
            NonTerminal::variable, {
                {Symbol::T(TokenType::ID), Symbol::N(NonTerminal::postfixListNoCall)}
            }
        },
        {
            NonTerminal::visibility, {
                {Symbol::T(TokenType::PUBLIC)},
                {Symbol::T(TokenType::PRIVATE)}
            }
        }
    };
    // clang-format on

    bool SyntacticAnalyzer::isEpsilon(const FirstSymbol &s)
    {
        return std::holds_alternative<tags::EpsilonTag>(s);
    }

    FirstSet SyntacticAnalyzer::generateFirstSet()
    {
        FirstSet first;
        bool changed = true;

        while (changed) {
            changed = false;

            for (auto &[A, prods] : grammar) {
                for (auto &p : prods) {
                    if (p.empty()) {
                        changed |= first[A].insert(tags::EPS).second;
                        continue;
                    }

                    bool allNullable = true;

                    for (auto &sym : p) {
                        if (sym.is_terminal) {
                            changed |= first[A].insert(sym.term).second;
                            allNullable = false;
                            break;
                        } else {
                            for (auto &f : first[sym.nonterm])
                                if (!isEpsilon(f))
                                    changed |= first[A].insert(f).second;

                            if (!first[sym.nonterm].contains(tags::EPS)) {
                                allNullable = false;
                                break;
                            }
                        }
                    }

                    if (allNullable)
                        changed |= first[A].insert(tags::EPS).second;
                }
            }
        }

        return first;
    }

    FollowSet SyntacticAnalyzer::generateFollowSet()
    {
        FollowSet follow;

        for (const auto &[A, _] : grammar) {
            (void)_;
            follow[A];
        }

        follow[NonTerminal::START].insert(TokenType::END_OF_FILE);

        bool changed = true;

        while (changed) {
            changed = false;

            for (auto &[A, prods] : grammar) {
                for (auto &p : prods) {
                    for (size_t i = 0; i < p.size(); ++i) {
                        if (!p[i].is_terminal) {
                            auto B = p[i].nonterm;

                            bool nullableSuffix = true;

                            for (size_t j = i + 1; j < p.size(); ++j) {
                                auto &next = p[j];

                                if (next.is_terminal) {
                                    changed |= follow[B].insert(next.term).second;
                                    nullableSuffix = false;
                                    break;
                                }

                                for (auto &f : m_firstSet.at(next.nonterm))
                                    if (!isEpsilon(f))
                                        changed |= follow[B].insert(std::get<TokenType>(f)).second;

                                if (!m_firstSet.at(next.nonterm).contains(tags::EPS)) {
                                    nullableSuffix = false;
                                    break;
                                }
                            }

                            if (nullableSuffix)
                                for (auto t : follow[A]) changed |= follow[B].insert(t).second;
                        }
                    }
                }
            }
        }

        return follow;
    }

    ParseTable SyntacticAnalyzer::generateParseTable()
    {
        ParseTable table;

        for (auto &[A, prods] : grammar) {
            table[A];
            for (auto &p : prods) {

                if (p.empty()) {
                    for (auto t : m_followSet.at(A)) table[A][t] = ParseTableEntry{ p };
                    continue;
                }

                bool nullable = true;

                for (auto &sym : p) {
                    if (sym.is_terminal) {
                        table[A][sym.term] = ParseTableEntry{ p };
                        nullable = false;
                        break;
                    }

                    for (auto &f : m_firstSet.at(sym.nonterm))
                        if (!isEpsilon(f))
                            table[A][std::get<TokenType>(f)] = ParseTableEntry{ p };

                    if (!m_firstSet.at(sym.nonterm).contains(tags::EPS)) {
                        nullable = false;
                        break;
                    }
                }

                if (nullable)
                    for (auto t : m_followSet.at(A)) table[A][t] = ParseTableEntry{ p };
            }
        }

        for (const auto &[A, _] : grammar)
            for (auto t : m_followSet.at(A))
                if (!table[A].contains(t))
                    table[A][t] = ParseTableEntry{ tags::SYNC };

        return table;
    }

    static bool isLikelyMissingDelimiter(TokenType t)
    {
        switch (t) {
            case TokenType::SEMICOLON:
            case TokenType::COMMA:
            case TokenType::CLOSE_PARENTHESIS:
            case TokenType::CLOSE_BRACE:
            case TokenType::CLOSE_BRACKET:
                return true;
            default:
                return false;
        }
    }

    void SyntacticAnalyzer::outputDerivationStep(const NonTerminal &A, const ParseTableEntry &entry)
    {
        if (!m_outputFiles)
            return;

        std::string res;

        res.append(to_string(A)).append(" -> ");

        if (std::holds_alternative<Production>(entry)) {
            auto &prod = std::get<Production>(entry);
            if (prod.empty())
                res.append("EPSILON");

            for (auto &sym : prod) {
                if (sym.is_terminal)
                    res.append("'").append(lang::tokenTypeToCompString(sym.term)).append("' ");
                else
                    res.append("<").append(to_string(sym.nonterm)).append("> ");
            }
        }

        while (!res.empty() && res.back() == ' ') res.pop_back();

        m_outDerivation << res << std::endl;
    }

#define SYNTAX_ERROR()                                    \
    if (++errorCount >= maxErrors) {                      \
        error(token, "too many syntax errors; aborting"); \
        return;                                           \
    }

    void SyntacticAnalyzer::parse()
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::stack<Symbol> st;
        st.push(Symbol::T(TokenType::END_OF_FILE));
        st.push(Symbol::N(NonTerminal::START));

        std::uint32_t idx = 0;
        std::uint32_t errorCount = 0;
        constexpr std::uint32_t maxErrors = 20;

        while (!st.empty()) {
            if (idx >= m_tokens.size()) {
                error(m_tokens.empty() ? Token{ TokenType::END_OF_FILE, "", 0, 0, "" } : m_tokens.back(), "unexpected end of token stream");
                return;
            }

            auto top = st.top();
            auto token = m_tokens[idx];

            if (top.is_terminal) {
                if (top.term == token.type) [[likely]] {
                    st.pop();
                    idx++;
                } else {
                    if (top.term == TokenType::END_OF_FILE) {
                        warn(token, std::format(R"(discarding unexpected token "{}" before end of file)", lang::tokenTypeToCompString(token.type)));
                        idx++;
                    } else if (token.type == TokenType::END_OF_FILE || isLikelyMissingDelimiter(top.term)) {
                        warn(
                            token,
                            std::format(
                                R"(expected "{}", but got "{}"; inserting missing token)",
                                lang::tokenTypeToCompString(top.term),
                                lang::tokenTypeToCompString(token.type)));
                        st.pop();
                    } else {
                        warn(
                            token,
                            std::format(
                                R"(expected "{}", but got "{}"; discarding unexpected token)",
                                lang::tokenTypeToCompString(top.term),
                                lang::tokenTypeToCompString(token.type)));
                        idx++;
                    }

                    SYNTAX_ERROR();
                }
            } else {
                const auto A = top.nonterm;

                if (m_parseTable.contains(A) && m_parseTable.at(A).contains(token.type)) [[likely]] {
                    auto &entry = m_parseTable.at(A).at(token.type);

                    if (std::holds_alternative<tags::SyncTag>(entry)) {
                        warn(
                            token,
                            std::format(
                                R"(synchronizing: popping non-terminal <{}> on lookahead "{}")", to_string(A), lang::tokenTypeToCompString(token.type)));
                        st.pop();
                        SYNTAX_ERROR();
                        continue;
                    }

                    outputDerivationStep(A, entry);

                    st.pop();
                    auto &prod = std::get<Production>(entry);
                    for (auto it = prod.rbegin(); it != prod.rend(); ++it) st.push(*it);
                } else {
                    if (token.type == TokenType::END_OF_FILE) {
                        error(
                            token,
                            std::format(R"(no production for non-terminal <{}> with lookahead "{}")", to_string(A), lang::tokenTypeToCompString(token.type)));
                        return;
                    }

                    warn(
                        token,
                        std::format(
                            R"(no production for non-terminal <{}> with lookahead "{}"; discarding token)",
                            to_string(A),
                            lang::tokenTypeToCompString(token.type)));
                    idx++;
                    SYNTAX_ERROR();
                }
            }
        }

        if (idx < m_tokens.size()) {
            warn(m_tokens[idx], std::format("skipping {} extra tokens after parse completion", m_tokens.size() - idx));
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        spdlog::info("{}: Parsed in {:.2f}ms with {} syntax error(s)", m_currentFilePath, elapsed.count(), errorCount);
    }

    static std::string underlineProblematicToken(const Token &token)
    {
        std::string res;
        if (token.pos > 1)
            res.append(static_cast<size_t>(token.pos - 1), ' ');
        res.push_back('^');
        if (token.lexeme.size() > 1)
            res.append(token.lexeme.size() - 1, '~');
        return res;
    }

    static std::string expandTabs(std::string_view line)
    {
        std::string out;
        out.reserve(line.size());

        std::uint32_t col = 1;
        for (char ch : line) {
            if (ch == '\r')
                continue;
            if (ch == '\t') {
                auto spaces = static_cast<size_t>(NEXT_TAB_POS(col));
                out.append(spaces, ' ');
                col += static_cast<std::uint32_t>(spaces);
            } else {
                out.push_back(ch);
                col += 1;
            }
        }

        return out;
    }

    void SyntacticAnalyzer::error(const Token &token, const std::string &message)
    {
        std::string line = expandTabs(m_lexicalAnalyzer.getLine(token.line));
        std::string underline = underlineProblematicToken(token);

        auto err = std::format(
            "{}:{}:{}: Syntax error: {}\n"
            "  {}\t|\t{}\n"
            "  \t|\t{}\n",
            token.file_path,
            token.line,
            token.pos,
            message,
            token.line,
            line,
            underline);

        spdlog::error(err);

        if (m_outputFiles)
            m_outParseErrors << err << std::endl;
    }

    void SyntacticAnalyzer::warn(const Token &token, const std::string &message)
    {
        std::string line = expandTabs(m_lexicalAnalyzer.getLine(token.line));
        std::string underline = underlineProblematicToken(token);

        auto err = std::format(
            "{}:{}:{}: Syntax error (recovered): {}\n"
            "  {}\t|\t{}\n"
            "  \t|\t{}\n",
            token.file_path,
            token.line,
            token.pos,
            message,
            token.line,
            line,
            underline);

        spdlog::warn(err);

        if (m_outputFiles)
            m_outParseErrors << err << std::endl;
    }

    std::string SyntacticAnalyzer::getFirstSet()
    {
        std::string res;

        for (auto &[nonterm, tokens] : m_firstSet) {
            std::string line = std::format("FIRST(<{}>)= [", to_string(nonterm));
            std::uint32_t count = 0;
            for (auto t : tokens) {
                if (isEpsilon(t))
                    line += "EPSILON";
                else if (std::get<TokenType>(t) == TokenType::END_OF_FILE)
                    line += std::format("{}", tokenTypeToCompString(std::get<TokenType>(t)));
                else
                    line += std::format("'{}'", tokenTypeToCompString(std::get<TokenType>(t)));
                if (++count != tokens.size())
                    line += ", ";
            }
            line += "]\n";
            res += line;
        }

        return res;
    }

    std::string SyntacticAnalyzer::getFollowSet()
    {
        std::string res;

        for (auto &[nonterm, tokens] : m_followSet) {
            std::string line = std::format("FOLLOW(<{}>)= [", to_string(nonterm));
            std::uint32_t count = 0;
            for (auto t : tokens) {
                if (t == TokenType::END_OF_FILE)
                    line += std::format("{}", tokenTypeToCompString(t));
                else
                    line += std::format("'{}'", tokenTypeToCompString(t));
                if (++count != tokens.size())
                    line += ", ";
            }
            line += "]\n";
            res += line;
        }

        return res;
    }
} // namespace lang
