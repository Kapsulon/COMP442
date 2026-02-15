#include "SyntacticAnalyzer.hpp"
#include <format>
#include "LexicalAnalyzer/LexicalAnalyzer.hpp"
#include "spdlog/spdlog.h"

namespace lang
{
    SyntacticAnalyzer::SyntacticAnalyzer() : m_firstSet(generateFirstSet()), m_followSet(generateFollowSet()), m_parseTable(generateParseTable()) {}

    void SyntacticAnalyzer::openFile(std::string_view path)
    {
        closeFile();

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
    }

    void SyntacticAnalyzer::closeFile()
    {
        m_lexicalAnalyzer.closeFile();
    }

    void SyntacticAnalyzer::lex()
    {
        while (true) {
            Token token = m_lexicalAnalyzer.next();
            m_tokens.push_back(token);

            if (token.type == TokenType::END_OF_FILE) {
                m_tokens.push_back(token);
                break;
            }
        }
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
                {}
            }
        },
        {
            NonTerminal::aParamsTail, {
                {Symbol::T(TokenType::COMMA), Symbol::N(NonTerminal::expr), Symbol::N(NonTerminal::aParamsTail)},
                {}
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
                {}
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
                {}
            }
        },
        {
            NonTerminal::classInheritTail, {
                {Symbol::T(TokenType::COMMA), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::classInheritTail)},
                {}
            }
        },
        {
            NonTerminal::classList, {
                {Symbol::N(NonTerminal::classDecl), Symbol::N(NonTerminal::classList)},
                {}
            }
        },
        {
            NonTerminal::classMemberList, {
                {Symbol::N(NonTerminal::visibility), Symbol::N(NonTerminal::memberDecl), Symbol::N(NonTerminal::classMemberList)},
                {}
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
                {}
            }
        },
        {
            NonTerminal::fParams, {
                {Symbol::N(NonTerminal::type), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::fParamsArrayList), Symbol::N(NonTerminal::fParamsTail)},
                {}
            }
        },
        {
            NonTerminal::fParamsArrayList, {
                {Symbol::N(NonTerminal::arraySize), Symbol::N(NonTerminal::fParamsArrayList)},
                {}
            }
        },
        {
            NonTerminal::fParamsTail, {
                {Symbol::T(TokenType::COMMA), Symbol::N(NonTerminal::type), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::fParamsArrayList), Symbol::N(NonTerminal::fParamsTail)},
                {}
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
                {Symbol::N(NonTerminal::funcHead), Symbol::N(NonTerminal::funcBody), Symbol::T(TokenType::SEMICOLON)}
            }
        },
        {
            NonTerminal::funcDefList, {
                {Symbol::N(NonTerminal::funcDef), Symbol::N(NonTerminal::funcDefList)},
                {}
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
                {}
            }
        },
        {
            NonTerminal::memberDecl, {
                {Symbol::T(TokenType::ID), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcDeclTail)},
                {Symbol::N(NonTerminal::type_no_id), Symbol::T(TokenType::ID), Symbol::N(NonTerminal::varArrayList), Symbol::T(TokenType::SEMICOLON)}
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
                {}
            }
        },
        {
            NonTerminal::postfixListNoCall, {
                {Symbol::N(NonTerminal::postfixNoCall), Symbol::N(NonTerminal::postfixListNoCall)},
                {}
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
                {}
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
                {}
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
                {}
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
                {}
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
                {}
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
        return std::holds_alternative<EpsilonTag>(s);
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
                        changed |= first[A].insert(EPS).second;
                        continue;
                    }

                    bool allNullable = true;

                    for (auto &sym : p) {
                        if (sym.is_terminal) {
                            changed |= first[A].insert(sym.term).second;
                            allNullable = false;
                            break;
                        } else {
                            for (auto &f : first[sym.nonterm]) {
                                if (!isEpsilon(f))
                                    changed |= first[A].insert(f).second;
                            }

                            if (!first[sym.nonterm].contains(EPS)) {
                                allNullable = false;
                                break;
                            }
                        }
                    }

                    if (allNullable)
                        changed |= first[A].insert(EPS).second;
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

                                if (!m_firstSet.at(next.nonterm).contains(EPS)) {
                                    nullableSuffix = false;
                                    break;
                                }
                            }

                            if (nullableSuffix) {
                                for (auto t : follow[A]) changed |= follow[B].insert(t).second;
                            }
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
            for (auto &p : prods) {

                if (p.empty()) {
                    for (auto t : m_followSet.at(A)) table[A][t] = p;
                    continue;
                }

                bool nullable = true;

                for (auto &sym : p) {
                    if (sym.is_terminal) {
                        table[A][sym.term] = p;
                        nullable = false;
                        break;
                    }

                    for (auto &f : m_firstSet.at(sym.nonterm))
                        if (!isEpsilon(f))
                            table[A][std::get<TokenType>(f)] = p;

                    if (!m_firstSet.at(sym.nonterm).contains(EPS)) {
                        nullable = false;
                        break;
                    }
                }

                if (nullable) {
                    for (auto t : m_followSet.at(A)) table[A][t] = p;
                }
            }
        }

        return table;
    }

    void SyntacticAnalyzer::parse()
    {
        std::stack<Symbol> st;
        st.push(Symbol::T(TokenType::END_OF_FILE));
        st.push(Symbol::N(NonTerminal::prog));

        std::uint32_t idx = 0;

        while (!st.empty()) {
            auto x = st.top();
            auto a = m_tokens[idx];

            if (x.is_terminal) {
                if (x.term == a.type) {
                    st.pop();
                    idx++;
                } else {
                    error(
                        a,
                        std::format(
                            "expected token of type {}, but got token of type {}", lang::tokenTypeToString(x.term), lang::tokenTypeToString(a.type), idx));
                    return;
                }
            } else {
                if (m_parseTable.contains(x.nonterm) && m_parseTable.at(x.nonterm).contains(a.type)) {
                    st.pop();
                    auto &prod = m_parseTable.at(x.nonterm).at(a.type);
                    for (auto it = prod.rbegin(); it != prod.rend(); ++it) st.push(*it);
                } else {
                    error(
                        a,
                        std::format(
                            "no production for non-terminal <{}> with lookahead token of type {}", to_string(x.nonterm), lang::tokenTypeToString(a.type), idx));
                    return;
                }
            }
        }

        if (idx != m_tokens.size()) {
            error(m_tokens[idx], std::format("expected end of file, but got {} extra tokens", m_tokens.size() - idx));
        }

        return;
    }

    static std::string underlineProblematicToken(const Token &token)
    {
        std::string res = "";

        for (std::uint32_t i = 1; i < token.pos; i++) res.append(" ");
        res.append("^");
        for (std::uint32_t i = token.pos + 1; i <= token.lexeme.size(); i++) res.append("~");

        return res;
    }

    void SyntacticAnalyzer::error(const Token &token, const std::string &message)
    {
        auto line = m_lexicalAnalyzer.getLine(token.line);
        while (!line.empty() && std::isspace(line[0])) line.remove_prefix(1);

        std::string underline = underlineProblematicToken(token);

        spdlog::error(
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
