#include <chrono>
#include <format>
#include <initializer_list>
#include <iostream>
#include <set>

#include "LexicalAnalyzer/LexicalAnalyzer.hpp"
#include "SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"
#include "utils/colors.hpp"

namespace lang
{
    SyntacticAnalyzer::SyntacticAnalyzer() : m_firstSet(generateFirstSet()), m_followSet(generateFollowSet()), m_parseTable(generateParseTable()) {}

    void SyntacticAnalyzer::openFile(std::string_view path)
    {
        closeFile();
        while (!m_nodeStack.empty()) m_nodeStack.pop();
        m_astRoot = nullptr;
        m_lastToken = Token{ TokenType::END_OF_FILE, "", 0, 0, "" };
        m_savedOperators.clear();
        m_savedLeadId.clear();
        m_currentVisibility = "";
        m_outDerivationSteps.clear();
        m_problems.clear();

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
    }

    void SyntacticAnalyzer::closeFile()
    {
        m_lexicalAnalyzer.closeFile();
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

        spdlog::info("{}: Lexed {} tokens in \t {:.2f}ms", m_currentFilePath, m_tokens.size(), elapsed.count());
    }

    // clang-format off
    const Grammar SyntacticAnalyzer::grammar = {
        {
            NonTerminal::START, {
                { Symbol::N(NonTerminal::prog) }
            }
        },
        {
            NonTerminal::aParams, {
                { Symbol::N(NonTerminal::expr), Symbol::N(NonTerminal::aParamsTail) },
                EPSILON
            }
        },
        {
            NonTerminal::aParamsTail, {
                { Symbol::T(TokenType::COMMA), Symbol::N(NonTerminal::expr), Symbol::N(NonTerminal::aParamsTail) },
                EPSILON
            }
        },
        {
            NonTerminal::addOp, {
                { Symbol::T(TokenType::PLUS) },
                { Symbol::T(TokenType::MINUS) },
                { Symbol::T(TokenType::OR) }
            }
        },
        {
            NonTerminal::arithExpr, {
                { Symbol::N(NonTerminal::term), Symbol::N(NonTerminal::arithExprTail) }
            }
        },
        {
            NonTerminal::arithExprTail, {
                { Symbol::N(NonTerminal::addOp), Symbol::A(SemanticAction::SaveOp), Symbol::N(NonTerminal::term), Symbol::N(NonTerminal::arithExprTail), Symbol::A(SemanticAction::MakeAddOp) },
                EPSILON
            }
        },
        {
            NonTerminal::arraySize, {
                { Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arraySizeTail) }
            }
        },
        {
            NonTerminal::arraySizeTail, {
                { Symbol::T(TokenType::INT_NUM), Symbol::A(SemanticAction::MakeNum), Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeDim) },
                { Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeDim) }
            }
        },
        {
            NonTerminal::assignOp, {
                { Symbol::T(TokenType::ASSIGN) }
            }
        },
        {
            NonTerminal::classDecl, {
                { Symbol::T(TokenType::CLASS), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::classInheritOpt), Symbol::A(SemanticAction::MakeInheritList), Symbol::T(TokenType::OPEN_BRACE), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::classMemberList), Symbol::T(TokenType::CLOSE_BRACE), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeClass) }
            }
        },
        {
            NonTerminal::classInheritOpt, {
                { Symbol::T(TokenType::INHERITS), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::classInheritTail) },
                EPSILON
            }
        },
        {
            NonTerminal::classInheritTail, {
                { Symbol::T(TokenType::COMMA), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::classInheritTail) },
                EPSILON
            }
        },
        {
            NonTerminal::classList, {
                { Symbol::N(NonTerminal::classDecl), Symbol::N(NonTerminal::classList) },
                EPSILON
            }
        },
        {
            NonTerminal::classMemberList, {
                { Symbol::N(NonTerminal::visibility), Symbol::A(SemanticAction::SaveVisibility), Symbol::N(NonTerminal::memberDecl), Symbol::N(NonTerminal::classMemberList) },
                EPSILON
            }
        },
        {
            NonTerminal::expr, {
                { Symbol::N(NonTerminal::arithExpr), Symbol::N(NonTerminal::exprRelTail) }
            }
        },
        {
            NonTerminal::exprRelTail, {
                { Symbol::N(NonTerminal::relOp), Symbol::A(SemanticAction::SaveOp), Symbol::N(NonTerminal::arithExpr), Symbol::A(SemanticAction::MakeRelOp) },
                EPSILON
            }
        },
        {
            NonTerminal::fParams, {
                { Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParamsArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::A(SemanticAction::MakeVarDecl), Symbol::N(NonTerminal::fParamsTail) },
                EPSILON
            }
        },
        {
            NonTerminal::fParamsArrayList, {
                { Symbol::N(NonTerminal::arraySize), Symbol::N(NonTerminal::fParamsArrayList) },
                EPSILON
            }
        },
        {
            NonTerminal::fParamsTail, {
                { Symbol::T(TokenType::COMMA), Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParamsArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::A(SemanticAction::MakeVarDecl), Symbol::N(NonTerminal::fParamsTail) },
                EPSILON
            }
        },
        {
            NonTerminal::factor, {
                { Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::postfixList) },
                { Symbol::T(TokenType::INT_NUM), Symbol::A(SemanticAction::MakeNum) },
                { Symbol::T(TokenType::FLOAT_NUM), Symbol::A(SemanticAction::MakeNum) },
                { Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_PARENTHESIS) },
                { Symbol::T(TokenType::NOT), Symbol::N(NonTerminal::factor), Symbol::A(SemanticAction::MakeNotExpr) },
                { Symbol::N(NonTerminal::sign), Symbol::A(SemanticAction::SaveOp), Symbol::N(NonTerminal::factor), Symbol::A(SemanticAction::MakeSignExpr) }
            }
        },
        {
            NonTerminal::funcBody, {
                { Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::localDeclOpt), Symbol::T(TokenType::DO), Symbol::N(NonTerminal::stmtList), Symbol::T(TokenType::END), Symbol::A(SemanticAction::MakeStatBlock) }
            }
        },
        {
            NonTerminal::funcDeclTail, {
                { Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::SEMICOLON) },
                { Symbol::T(TokenType::VOID), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::SEMICOLON) }
            }
        },
        {
            NonTerminal::funcDef, {
                { Symbol::N(NonTerminal::funcHead), Symbol::N(NonTerminal::funcBody), Symbol::A(SemanticAction::MakeFuncDef) }
            }
        },
        {
            NonTerminal::funcDefList, {
                { Symbol::N(NonTerminal::funcDef), Symbol::N(NonTerminal::funcDefList) },
                EPSILON
            }
        },
        {
            NonTerminal::funcHead, {
                { Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::funcHeadTail) }
            }
        },
        {
            NonTerminal::funcHeadReturn, {
                { Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType) },
                { Symbol::T(TokenType::VOID), Symbol::A(SemanticAction::MakeType) }
            }
        },
        {
            NonTerminal::funcHeadTail, {
                { Symbol::T(TokenType::DOUBLE_COLON), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::A(SemanticAction::MakeParamList), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcHeadReturn) },
                { Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::A(SemanticAction::MakeParamList), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcHeadReturn) }
            }
        },
        {
            NonTerminal::indice, {
                { Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET) }
            }
        },
        {
            NonTerminal::localDeclOpt, {
                { Symbol::T(TokenType::LOCAL), Symbol::N(NonTerminal::varDeclList) },
                EPSILON
            }
        },
        {
            NonTerminal::memberDecl, {
                { Symbol::N(NonTerminal::type_no_id), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::varArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeMemberVarDecl) },
                { Symbol::T(TokenType::ID), Symbol::A(SemanticAction::SaveLeadId), Symbol::N(NonTerminal::memberAfterId) }
            }
        },
        {
            NonTerminal::memberAfterId, {
                { Symbol::A(SemanticAction::MakeSavedType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::varArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeMemberVarDecl) },
                { Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::A(SemanticAction::MakeSavedId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::A(SemanticAction::MakeParamList), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcDeclTail), Symbol::A(SemanticAction::MakeFuncDecl) },
            }
        },
        {
            NonTerminal::multOp, {
                { Symbol::T(TokenType::MULTIPLY) },
                { Symbol::T(TokenType::DIVIDE) },
                { Symbol::T(TokenType::AND) }
            }
        },
        {
            NonTerminal::postfix, {
                { Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::aParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::A(SemanticAction::MakeParamList), Symbol::A(SemanticAction::MakeFuncCall) },
                { Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeIndexedVar) },
                { Symbol::T(TokenType::DOT), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::MakeMemberAccess) }
            }
        },
        {
            NonTerminal::postfixList, {
                { Symbol::N(NonTerminal::postfix), Symbol::N(NonTerminal::postfixList) },
                EPSILON
            }
        },
        {
            NonTerminal::postfixListNoCall, {
                { Symbol::N(NonTerminal::postfixNoCall), Symbol::N(NonTerminal::postfixListNoCall) },
                EPSILON
            }
        },
        {
            NonTerminal::postfixNoCall, {
                { Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeIndexedVar) },
                { Symbol::T(TokenType::DOT), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::MakeMemberAccess) }
            }
        },
        {
            NonTerminal::prog, {
                { Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::classList), Symbol::A(SemanticAction::MakeClassList), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::funcDefList), Symbol::A(SemanticAction::MakeFuncDefList), Symbol::T(TokenType::MAIN), Symbol::N(NonTerminal::funcBody), Symbol::A(SemanticAction::MakeProgramBlock), Symbol::A(SemanticAction::MakeProg) }
            }
        },
        {
            NonTerminal::relOp, {
                { Symbol::T(TokenType::EQUAL) },
                { Symbol::T(TokenType::NOT_EQUAL) },
                { Symbol::T(TokenType::LESS_THAN) },
                { Symbol::T(TokenType::GREATER_THAN) },
                { Symbol::T(TokenType::LESS_EQUAL) },
                { Symbol::T(TokenType::GREATER_EQUAL) }
            }
        },
        {
            NonTerminal::sign, {
                { Symbol::T(TokenType::PLUS) },
                { Symbol::T(TokenType::MINUS) }
            }
        },
        {
            NonTerminal::statBlock, {
                { Symbol::T(TokenType::DO), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::stmtList), Symbol::T(TokenType::END), Symbol::A(SemanticAction::MakeStatBlock) },
                { Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::statement), Symbol::A(SemanticAction::MakeStatBlock) },
                { Symbol::A(SemanticAction::PushMarker), Symbol::A(SemanticAction::MakeStatBlock) }
            }
        },
        {
            NonTerminal::statement, {
                { Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::postfixList), Symbol::N(NonTerminal::statementEnd) },
                { Symbol::T(TokenType::IF), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::THEN), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::ELSE), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeIfStat) },
                { Symbol::T(TokenType::WHILE), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeWhileStat) },
                { Symbol::T(TokenType::READ), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::variable), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeReadStat) },
                { Symbol::T(TokenType::WRITE), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakePutStat) },
                { Symbol::T(TokenType::RETURN), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeReturnStat) }
            }
        },
        {
            NonTerminal::statementEnd, {
                { Symbol::N(NonTerminal::assignOp), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeAssignStat) },
                { Symbol::T(TokenType::SEMICOLON) }
            }
        },
        {
            NonTerminal::stmtList, {
                { Symbol::N(NonTerminal::statement), Symbol::N(NonTerminal::stmtList) },
                EPSILON
            }
        },
        {
            NonTerminal::term, {
                { Symbol::N(NonTerminal::factor), Symbol::N(NonTerminal::termTail) }
            }
        },
        {
            NonTerminal::termTail, {
                { Symbol::N(NonTerminal::multOp), Symbol::A(SemanticAction::SaveOp), Symbol::N(NonTerminal::factor), Symbol::N(NonTerminal::termTail), Symbol::A(SemanticAction::MakeMultOp) },
                EPSILON
            }
        },
        {
            NonTerminal::type, {
                { Symbol::T(TokenType::INTEGER) },
                { Symbol::T(TokenType::FLOAT) },
                { Symbol::T(TokenType::ID) }
            }
        },
        {
            NonTerminal::type_no_id, {
                { Symbol::T(TokenType::INTEGER) },
                { Symbol::T(TokenType::FLOAT) }
            }
        },
        {
            NonTerminal::varArrayList, {
                { Symbol::N(NonTerminal::arraySize), Symbol::N(NonTerminal::varArrayList) },
                EPSILON
            }
        },
        {
            NonTerminal::varDecl, {
                { Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::varArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeVarDecl) }
            }
        },
        {
            NonTerminal::varDeclList, {
                { Symbol::N(NonTerminal::varDecl), Symbol::N(NonTerminal::varDeclList) },
                EPSILON
            }
        },
        {
            NonTerminal::variable, {
                { Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::postfixListNoCall) }
            }
        },
        {
            NonTerminal::visibility, {
                { Symbol::T(TokenType::PUBLIC) },
                { Symbol::T(TokenType::PRIVATE) }
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
                        if (std::holds_alternative<SemanticAction>(sym.value))
                            continue;

                        if (auto token = std::get_if<TokenType>(&sym.value)) {
                            changed |= first[A].insert(*token).second;
                            allNullable = false;
                            break;
                        } else if (auto nonterm = std::get_if<NonTerminal>(&sym.value)) {
                            for (auto &f : first[*nonterm])
                                if (!isEpsilon(f))
                                    changed |= first[A].insert(f).second;

                            if (!first[*nonterm].contains(tags::EPS)) {
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

        for (const auto &[A, _] : grammar) follow[A];

        follow[NonTerminal::START].insert(TokenType::END_OF_FILE);

        bool changed = true;

        while (changed) {
            changed = false;

            for (auto &[A, prods] : grammar) {
                for (auto &p : prods) {
                    for (size_t i = 0; i < p.size(); ++i) {
                        if (std::holds_alternative<SemanticAction>(p[i].value))
                            continue;

                        if (auto B = std::get_if<NonTerminal>(&p[i].value)) {
                            bool nullableSuffix = true;

                            for (size_t j = i + 1; j < p.size(); ++j) {
                                auto &next = p[j];

                                if (std::holds_alternative<SemanticAction>(next.value))
                                    continue;

                                if (auto next_term = std::get_if<TokenType>(&next.value)) {
                                    changed |= follow[*B].insert(*next_term).second;
                                    nullableSuffix = false;
                                    break;
                                }

                                auto next_nonterm = std::get_if<NonTerminal>(&next.value);
                                for (auto &f : m_firstSet.at(*next_nonterm))
                                    if (!isEpsilon(f))
                                        changed |= follow[*B].insert(std::get<TokenType>(f)).second;

                                if (!m_firstSet.at(*next_nonterm).contains(tags::EPS)) {
                                    nullableSuffix = false;
                                    break;
                                }
                            }

                            if (nullableSuffix)
                                for (auto t : follow[A]) changed |= follow[*B].insert(t).second;
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
                    if (std::holds_alternative<SemanticAction>(sym.value))
                        continue;

                    if (auto sym_term = std::get_if<TokenType>(&sym.value)) {
                        table[A][*sym_term] = ParseTableEntry{ p };
                        nullable = false;
                        break;
                    }

                    auto sym_nonterm = std::get_if<NonTerminal>(&sym.value);
                    for (auto &f : m_firstSet.at(*sym_nonterm))
                        if (!isEpsilon(f))
                            table[A][std::get<TokenType>(f)] = ParseTableEntry{ p };

                    if (!m_firstSet.at(*sym_nonterm).contains(tags::EPS)) {
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

    void SyntacticAnalyzer::outputSyntaxErrors()
    {
        std::string errorFilePath = m_currentFilePath + ".outsyntaxerrors";
        std::ofstream out(errorFilePath);

        if (!out.is_open())
            spdlog::error("Failed to open output file for syntax errors: {}", errorFilePath);

        out << m_problems.getProblems(m_lexicalAnalyzer);
    }

    void SyntacticAnalyzer::writeDerivationSteps(const NonTerminal &A, const ParseTableEntry &entry)
    {
        std::string res;

        res.append(to_string(A)).append(" -> ");

        if (std::holds_alternative<Production>(entry)) {
            auto &prod = std::get<Production>(entry);
            if (prod.empty())
                res.append("EPSILON");

            for (auto &sym : prod) {
                if (auto term = std::get_if<TokenType>(&sym.value))
                    res.append("'").append(lang::tokenTypeToCompString(*term)).append("' ");
                else if (auto nonterm = std::get_if<NonTerminal>(&sym.value))
                    res.append("<").append(to_string(*nonterm)).append("> ");
            }
        }

        while (!res.empty() && res.back() == ' ') res.pop_back();

        m_outDerivationSteps.append(res).append("\n");
    }

    void SyntacticAnalyzer::outputDerivationSteps()
    {
        std::string errorFilePath = m_currentFilePath + ".outderivation";
        std::ofstream out(errorFilePath);

        if (!out.is_open()) {
            spdlog::error("Failed to open output file for derivation steps: {}", errorFilePath);
            return;
        }

        out << m_outDerivationSteps;
    }

    void SyntacticAnalyzer::outputDotAST()
    {
        static const std::set<char> needs_escape = { '<', '>', '{', '}', '|', '"', '\\' };

        std::string errorFilePath = m_currentFilePath + ".outast";
        std::ofstream out(errorFilePath);

        if (!out.is_open())
            spdlog::error("Failed to open output file for derivation step: {}", errorFilePath);

        out << "digraph AST {\n";
        out << "node [shape=record];\n";
        out << " node [fontname=Sans];charset=\"UTF-8\" splines=true splines=spline rankdir =LR\n";

        std::uint64_t counter = 0;
        std::function<void(const lang::ASTNodePtr &)> assign;
        std::unordered_map<lang::ASTNode *, std::uint64_t> ids;

        assign = [&](const lang::ASTNodePtr &n) {
            if (!n)
                return;
            ids[n.get()] = counter++;
            for (auto &c : n->children) assign(c);
        };
        assign(m_astRoot);

        std::function<void(const lang::ASTNodePtr &)> emit;
        emit = [&](const lang::ASTNodePtr &n) {
            if (!n)
                return;
            auto id = ids[n.get()];
            std::string label = lang::to_string(n->kind);
            if (!n->lexeme.empty()) {
                label += " | ";
                std::string new_lexeme;
                for (char c : n->lexeme) {
                    if (needs_escape.contains(c))
                        new_lexeme += '\\';
                    new_lexeme += c;
                }
                label += new_lexeme;
            }
            out << id << "[label=\"" << label << "\"];\n";

            if (n->children.empty()) {
                if (n->kind == lang::ASTNode::Kind::DimList) {
                    out << "none" << id << "[shape=point];\n";
                    out << id << "->none" << id << ";\n";
                }
            } else {
                for (auto &c : n->children) {
                    out << id << "->" << ids[c.get()] << ";\n";
                    emit(c);
                }
            }
        };
        emit(m_astRoot);

        out << "}\n";
    }

    const LexicalAnalyzer &SyntacticAnalyzer::getLexer() const
    {
        return m_lexicalAnalyzer;
    }

#define SYNTAX_ERROR()                                                                   \
    if (m_problems.getWarningCount() + m_problems.getErrorCount() >= maxErrors) {        \
        m_problems.error("Fatal Error:", "too many syntax errors; aborting", { token }); \
        return;                                                                          \
    }

    void SyntacticAnalyzer::parse()
    {
        auto start = std::chrono::high_resolution_clock::now();
        std::stack<Symbol> st;
        st.push(Symbol::T(TokenType::END_OF_FILE));
        st.push(Symbol::N(NonTerminal::START));

        std::uint32_t idx = 0;
        constexpr std::uint32_t maxErrors = 20;

        while (!st.empty()) {
            if (idx >= m_tokens.size()) {
                m_problems.error(
                    "Syntax Error", "unexpected end of token stream", { m_tokens.empty() ? Token{ TokenType::END_OF_FILE, "", 0, 0, "" } : m_tokens.back() });
                return;
            }

            auto top = st.top();
            auto token = m_tokens[idx];

            if (auto top_action = std::get_if<SemanticAction>(&top.value)) {
                executeAction(*top_action);
                st.pop();
                continue;
            } else if (auto top_term = std::get_if<TokenType>(&top.value)) {
                if (*top_term == token.type) [[likely]] {
                    m_lastToken = token;
                    st.pop();
                    idx++;
                } else {
                    if (*top_term == TokenType::END_OF_FILE) {
                        m_problems.warn(
                            "Syntax Error (recovered)",
                            std::format(R"(discarding unexpected token "{}" before end of file)", lang::tokenTypeToCompString(token.type)),
                            { token });
                        idx++;
                    } else if (token.type == TokenType::END_OF_FILE || isLikelyMissingDelimiter(*top_term)) {
                        m_problems.warn(
                            "Syntax Error (recovered)",
                            std::format(
                                R"(expected "{}", but got "{}"; inserting missing token)",
                                lang::tokenTypeToCompString(*top_term),
                                lang::tokenTypeToCompString(token.type)),
                            { token });
                        st.pop();
                    } else {
                        m_problems.warn(
                            "Syntax Error (recovered)",
                            std::format(
                                R"(expected "{}", but got "{}"; discarding unexpected token)",
                                lang::tokenTypeToCompString(*top_term),
                                lang::tokenTypeToCompString(token.type)),
                            { token });
                        idx++;
                    }

                    SYNTAX_ERROR();
                }
            } else {
                const auto A = std::get<NonTerminal>(top.value);

                if (m_parseTable.contains(A) && m_parseTable.at(A).contains(token.type)) [[likely]] {
                    auto &entry = m_parseTable.at(A).at(token.type);

                    if (std::holds_alternative<tags::SyncTag>(entry)) {
                        m_problems.warn(
                            "Syntax Error (recovered)",
                            std::format(R"(synchronizing: popping non-terminal <{}> on lookahead "{}")", to_string(A), lang::tokenTypeToCompString(token.type)),
                            { token });
                        st.pop();
                        SYNTAX_ERROR();
                        continue;
                    }

                    writeDerivationSteps(A, entry);

                    st.pop();
                    auto &prod = std::get<Production>(entry);
                    for (auto it = prod.rbegin(); it != prod.rend(); ++it) st.push(*it);
                } else {
                    if (token.type == TokenType::END_OF_FILE) {
                        m_problems.error(
                            "Syntax Error",
                            std::format(R"(no production for non-terminal <{}> with lookahead "{}")", to_string(A), lang::tokenTypeToCompString(token.type)),
                            { token });
                        return;
                    }

                    m_problems.warn(
                        "Syntax Error (recovered)",
                        std::format(
                            R"(no production for non-terminal <{}> with lookahead "{}"; discarding token)",
                            to_string(A),
                            lang::tokenTypeToCompString(token.type)),
                        { token });
                    idx++;
                    SYNTAX_ERROR();
                }
            }
        }

        if (idx < m_tokens.size()) {
            m_problems.warn(
                "Syntax Error (recovered)", std::format("skipping {} extra tokens after parse completion", m_tokens.size() - idx), { m_tokens[idx] });
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        m_problems.displayProblems(m_lexicalAnalyzer);

        spdlog::info(
            "{}: Parsed in \t\t {:.2f}ms \t [" CYAN "{} info(s)" RESET ", " YELLOW "{} warning(s)" RESET ", " RED "{} error(s)" RESET "]",
            m_currentFilePath,
            elapsed.count(),
            m_problems.getInfoCount(),
            m_problems.getWarningCount(),
            m_problems.getErrorCount());
    }

    ASTNodePtr SyntacticAnalyzer::getAST() const
    {
        return m_astRoot;
    }

    static ASTNodePtr makeNode(ASTNode::Kind kind, std::string lexeme = "")
    {
        auto n = std::make_shared<ASTNode>();
        n->kind = kind;
        n->lexeme = std::move(lexeme);
        return n;
    }

    static inline bool isMarker(const ASTNodePtr &p)
    {
        return p == nullptr || p->kind == ASTNode::Kind::Marker;
    }

    static std::vector<ASTNodePtr> popToMarker(std::stack<ASTNodePtr> &st)
    {
        std::vector<ASTNodePtr> children;

        while (!st.empty() && !isMarker(st.top())) {
            children.push_back(st.top());
            st.pop();
        }

        if (!st.empty())
            st.pop();

        std::reverse(children.begin(), children.end());
        return children;
    }

    void SyntacticAnalyzer::executeAction(SemanticAction action)
    {
        switch (action) {
            case SemanticAction::MakeId:
                {
                    m_nodeStack.push(makeNode(ASTNode::Kind::Id, m_lastToken.lexeme));
                    break;
                }
            case SemanticAction::MakeSavedId:
                {
                    if (!m_savedLeadId.empty())
                        m_nodeStack.push(makeNode(ASTNode::Kind::Id, m_savedLeadId));
                    break;
                }
            case SemanticAction::MakeType:
                {
                    m_nodeStack.push(makeNode(ASTNode::Kind::Type, m_lastToken.lexeme));
                    break;
                }
            case SemanticAction::MakeSavedType:
                {
                    if (!m_savedLeadId.empty())
                        m_nodeStack.push(makeNode(ASTNode::Kind::Type, m_savedLeadId));
                    break;
                }
            case SemanticAction::MakeNum:
                {
                    m_nodeStack.push(makeNode(ASTNode::Kind::Num, m_lastToken.lexeme));
                    break;
                }
            case SemanticAction::MakeDim:
                {
                    std::string val;
                    if (!m_nodeStack.empty() && !isMarker(m_nodeStack.top()) && m_nodeStack.top()->kind == ASTNode::Kind::Num) {
                        val = m_nodeStack.top()->lexeme;
                        m_nodeStack.pop();
                    }
                    m_nodeStack.push(makeNode(ASTNode::Kind::Dim, val));
                    break;
                }
            case SemanticAction::MakeDimList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::DimList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeVarDecl:
                {
                    if (m_nodeStack.size() < 3)
                        break;
                    auto dimList = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto id = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto type = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::VarDecl);
                    node->children = { type, id, dimList };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeMemberVarDecl:
                {
                    if (m_nodeStack.size() < 3)
                        break;
                    auto dimList = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto id = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto type = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::VarDecl, m_currentVisibility);
                    node->children = { type, id, dimList };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeParamList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::ParamList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeClass:
                {
                    auto members = popToMarker(m_nodeStack);
                    ASTNodePtr inherits;
                    if (!m_nodeStack.empty() && !isMarker(m_nodeStack.top()) && m_nodeStack.top()->kind == ASTNode::Kind::InheritList) {
                        inherits = m_nodeStack.top();
                        m_nodeStack.pop();
                    }
                    ASTNodePtr id;
                    if (!m_nodeStack.empty() && !isMarker(m_nodeStack.top()) && m_nodeStack.top()->kind == ASTNode::Kind::Id) {
                        id = m_nodeStack.top();
                        m_nodeStack.pop();
                    }
                    auto node = makeNode(ASTNode::Kind::Class);
                    if (id)
                        node->children.push_back(id);
                    if (inherits)
                        node->children.push_back(inherits);
                    for (auto &m : members) node->children.push_back(m);
                    m_nodeStack.push(node);
                    m_currentVisibility.clear();
                    break;
                }
            case SemanticAction::MakeInheritList:
                {
                    auto children = popToMarker(m_nodeStack);
                    if (!children.empty()) {
                        auto node = makeNode(ASTNode::Kind::InheritList);
                        node->children = std::move(children);
                        m_nodeStack.push(node);
                    }
                    break;
                }
            case SemanticAction::MakeClassList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::ClassList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeFuncDef:
                {
                    if (m_nodeStack.size() < 4)
                        break;
                    auto statBlock = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto type = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto paramList = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto name = m_nodeStack.top();
                    m_nodeStack.pop();

                    ASTNodePtr id = name;
                    if (!m_nodeStack.empty() && !isMarker(m_nodeStack.top()) && m_nodeStack.top()->kind == ASTNode::Kind::Id) {
                        auto scope = m_nodeStack.top();
                        m_nodeStack.pop();
                        auto scopedName = makeNode(ASTNode::Kind::MemberAccess);
                        scopedName->children = { scope, name };
                        id = scopedName;
                    }

                    auto node = makeNode(ASTNode::Kind::FuncDef);
                    node->children = { type, id, paramList, statBlock };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeFuncDecl:
                {
                    if (m_nodeStack.size() < 3)
                        break;
                    auto type = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto paramList = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto id = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::FuncDecl, m_currentVisibility);
                    node->children = { type, id, paramList };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeFuncDefList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::FuncDefList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeStatBlock:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::StatBlock);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeProgramBlock:
                {
                    if (!m_nodeStack.empty() && m_nodeStack.top()->kind == ASTNode::Kind::StatBlock) {
                        m_nodeStack.top()->kind = ASTNode::Kind::ProgramBlock;
                    }
                    break;
                }
            case SemanticAction::MakeProg:
                {
                    if (m_nodeStack.size() < 3)
                        break;
                    auto programBlock = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto funcDefList = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto classList = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto root = makeNode(ASTNode::Kind::Prog);
                    root->children = { classList, funcDefList, programBlock };
                    m_nodeStack.push(root);
                    m_astRoot = root;
                    break;
                }
            case SemanticAction::MakeAssignStat:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto rhs = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto lhs = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::AssignStat);
                    node->children = { lhs, rhs };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakePutStat:
                {
                    if (m_nodeStack.empty())
                        break;
                    auto expr = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::PutStat);
                    node->children = { expr };
                    m_nodeStack.push(node);
                    break;
                }

            case SemanticAction::MakeReturnStat:
                {
                    if (m_nodeStack.empty())
                        break;
                    auto expr = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::ReturnStat);
                    node->children = { expr };
                    m_nodeStack.push(node);
                    break;
                }

            case SemanticAction::MakeIfStat:
                {
                    if (m_nodeStack.size() < 3)
                        break;
                    auto elseBlock = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto thenBlock = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto cond = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::IfStat);
                    node->children = { cond, thenBlock, elseBlock };
                    m_nodeStack.push(node);
                    break;
                }

            case SemanticAction::MakeWhileStat:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto body = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto cond = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::WhileStat);
                    node->children = { cond, body };
                    m_nodeStack.push(node);
                    break;
                }

            case SemanticAction::MakeReadStat:
                {
                    if (m_nodeStack.empty())
                        break;
                    auto var = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::ReadStat);
                    node->children = { var };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeAddOp:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    std::string op;
                    if (!m_savedOperators.empty()) {
                        op = m_savedOperators.back();
                        m_savedOperators.pop_back();
                    }
                    auto right = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto left = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::AddOp, op);
                    node->children = { left, right };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeMultOp:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    std::string op;
                    if (!m_savedOperators.empty()) {
                        op = m_savedOperators.back();
                        m_savedOperators.pop_back();
                    }
                    auto right = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto left = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::MultOp, op);
                    node->children = { left, right };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeRelOp:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    std::string op;
                    if (!m_savedOperators.empty()) {
                        op = m_savedOperators.back();
                        m_savedOperators.pop_back();
                    }
                    auto right = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto left = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::RelOp, op);
                    node->children = { left, right };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeNotExpr:
                {
                    if (m_nodeStack.empty())
                        break;
                    auto operand = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::NotExpr);
                    node->children = { operand };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeSignExpr:
                {
                    if (m_nodeStack.empty())
                        break;
                    std::string op;
                    if (!m_savedOperators.empty()) {
                        op = m_savedOperators.back();
                        m_savedOperators.pop_back();
                    }
                    auto operand = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::SignExpr, op);
                    node->children = { operand };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeFuncCall:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto params = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto id = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::FuncCall);
                    node->children = { id, params };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeIndexedVar:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto index = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto base = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::IndexedVar);
                    node->children = { base, index };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::MakeMemberAccess:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto member = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto object = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::MemberAccess);
                    node->children = { object, member };
                    m_nodeStack.push(node);
                    break;
                }
            case SemanticAction::PushMarker:
                {
                    m_nodeStack.push(makeNode(ASTNode::Kind::Marker));
                    break;
                }
            case SemanticAction::SaveOp:
                {
                    m_savedOperators.push_back(m_lastToken.lexeme);
                    break;
                }
            case SemanticAction::SaveLeadId:
                {
                    m_savedLeadId = m_lastToken.lexeme;
                    break;
                }
            case SemanticAction::SaveVisibility:
                {
                    m_currentVisibility = m_lastToken.lexeme;
                    break;
                }
        }
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

    std::string_view SyntacticAnalyzer::getCurrentFilePath() const
    {
        return m_currentFilePath;
    }
} // namespace lang
