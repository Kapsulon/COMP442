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
        // reset AST state for this file
        while (!m_nodeStack.empty()) m_nodeStack.pop();
        m_astRoot = nullptr;
        m_lastToken = Token{ TokenType::END_OF_FILE, "", 0, 0, "" };
        m_savedOperator = "";

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
        // aParams: collect actual args between PushMarker (placed by postfix call production) and MakeParamList
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
        // addOp: MakeAddOp fires in arithExprTail after the recursive tail resolves
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
        // arithExprTail: SaveOp captures the operator right after addOp matches its terminal,
        // then MakeAddOp fires after the recursive tail to build the subtree
        {
            NonTerminal::arithExprTail, {
                {Symbol::N(NonTerminal::addOp), Symbol::A(SemanticAction::SaveOp), Symbol::N(NonTerminal::term), Symbol::N(NonTerminal::arithExprTail), Symbol::A(SemanticAction::MakeAddOp)},
                EPSILON
            }
        },
        {
            NonTerminal::arraySize, {
                {Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arraySizeTail)}
            }
        },
        // arraySizeTail: MakeNum captures the int literal, then MakeDim wraps it
        {
            NonTerminal::arraySizeTail, {
                {Symbol::T(TokenType::INT_NUM), Symbol::A(SemanticAction::MakeNum), Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeDim)},
                {Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeDim)}
            }
        },
        {
            NonTerminal::assignOp, {
                {Symbol::T(TokenType::ASSIGN)}
            }
        },
        // classDecl: MakeId captures class name; PushMarker before members; MakeClass collects
        {
            NonTerminal::classDecl, {
                {Symbol::T(TokenType::CLASS), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::classInheritOpt), Symbol::T(TokenType::OPEN_BRACE), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::classMemberList), Symbol::T(TokenType::CLOSE_BRACE), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeClass)}
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
        // classMemberList: each memberDecl pushes a VarDecl; collected by MakeClass in classDecl
        {
            NonTerminal::classMemberList, {
                {Symbol::N(NonTerminal::visibility), Symbol::N(NonTerminal::memberDecl), Symbol::N(NonTerminal::classMemberList)},
                EPSILON
            }
        },
        // expr: if exprRelTail fires MakeRelOp, it wraps both arithExprs; otherwise left side stays
        {
            NonTerminal::expr, {
                {Symbol::N(NonTerminal::arithExpr), Symbol::N(NonTerminal::exprRelTail)}
            }
        },
        {
            NonTerminal::exprRelTail, {
                {Symbol::N(NonTerminal::relOp), Symbol::A(SemanticAction::SaveOp), Symbol::N(NonTerminal::arithExpr), Symbol::A(SemanticAction::MakeRelOp)},
                EPSILON
            }
        },
        // fParams: each param → MakeType, MakeId, PushMarker for dims, MakeDimList, MakeVarDecl
        {
            NonTerminal::fParams, {
                {Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParamsArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::A(SemanticAction::MakeVarDecl), Symbol::N(NonTerminal::fParamsTail)},
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
                {Symbol::T(TokenType::COMMA), Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParamsArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::A(SemanticAction::MakeVarDecl), Symbol::N(NonTerminal::fParamsTail)},
                EPSILON
            }
        },
        // factor: MakeId/MakeNum for leaves; MakeNotExpr/MakeSignExpr for unary; postfixList handles call/index
        {
            NonTerminal::factor, {
                {Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::postfixList)},
                {Symbol::T(TokenType::INT_NUM), Symbol::A(SemanticAction::MakeNum)},
                {Symbol::T(TokenType::FLOAT_NUM), Symbol::A(SemanticAction::MakeNum)},
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_PARENTHESIS)},
                {Symbol::T(TokenType::NOT), Symbol::N(NonTerminal::factor), Symbol::A(SemanticAction::MakeNotExpr)},
                {Symbol::N(NonTerminal::sign), Symbol::N(NonTerminal::factor), Symbol::A(SemanticAction::MakeSignExpr)}
            }
        },
        // funcBody: PushMarker before local decls+stmts; MakeStatBlock collects all
        {
            NonTerminal::funcBody, {
                {Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::localDeclOpt), Symbol::T(TokenType::DO), Symbol::N(NonTerminal::stmtList), Symbol::T(TokenType::END), Symbol::A(SemanticAction::MakeStatBlock)}
            }
        },
        {
            NonTerminal::funcDeclTail, {
                {Symbol::N(NonTerminal::type), Symbol::T(TokenType::SEMICOLON)},
                {Symbol::T(TokenType::VOID), Symbol::T(TokenType::SEMICOLON)}
            }
        },
        // funcDef: funcHead emits (in order) Id, ParamList, Type; funcBody emits StatBlock; MakeFuncDef assembles
        {
            NonTerminal::funcDef, {
                {Symbol::N(NonTerminal::funcHead), Symbol::N(NonTerminal::funcBody), Symbol::A(SemanticAction::MakeFuncDef)}
            }
        },
        {
            NonTerminal::funcDefList, {
                {Symbol::N(NonTerminal::funcDef), Symbol::N(NonTerminal::funcDefList)},
                EPSILON
            }
        },
        // funcHead: MakeType for return type (via funcHeadReturn), MakeId for func name
        {
            NonTerminal::funcHead, {
                {Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::funcHeadTail)}
            }
        },
        // funcHeadReturn: MakeType after type or void token
        {
            NonTerminal::funcHeadReturn, {
                {Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType)},
                {Symbol::T(TokenType::VOID), Symbol::A(SemanticAction::MakeType)}
            }
        },
        // funcHeadTail: PushMarker placed here right before fParams so MakeParamList correctly collects only the VarDecls
        // After MakeParamList, stack has (top→bottom): ParamList, Id(funcname)
        // funcHeadReturn then pushes Type on top → stack: Type, ParamList, Id
        {
            NonTerminal::funcHeadTail, {
                {Symbol::T(TokenType::DOUBLE_COLON), Symbol::T(TokenType::ID), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::A(SemanticAction::MakeParamList), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcHeadReturn)},
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::A(SemanticAction::MakeParamList), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcHeadReturn)}
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
        // memberDecl: type_no_id path → MakeType, MakeId, PushMarker, MakeDimList, MakeVarDecl
        // id path → memberAfterId handles it (var decl only for now; method decls not in AST examples)
        {
            NonTerminal::memberDecl, {
                {Symbol::N(NonTerminal::type_no_id), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::varArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeVarDecl)},
                {Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeType), Symbol::N(NonTerminal::memberAfterId)}
            }
        },
        // memberAfterId: var decl → MakeId, PushMarker, MakeDimList, MakeVarDecl
        {
            NonTerminal::memberAfterId, {
                {Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::varArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeVarDecl)},
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::fParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::COLON), Symbol::N(NonTerminal::funcDeclTail)},
            }
        },
        // multOp: MakeMultOp fires in termTail after the recursive tail resolves
        {
            NonTerminal::multOp, {
                {Symbol::T(TokenType::MULTIPLY)},
                {Symbol::T(TokenType::DIVIDE)},
                {Symbol::T(TokenType::AND)}
            }
        },
        // postfix: call → PushMarker, aParams, MakeParamList, MakeFuncCall
        //          index → MakeIndexedVar
        //          member → MakeId for member name, MakeMemberAccess
        {
            NonTerminal::postfix, {
                {Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::aParams), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::A(SemanticAction::MakeParamList), Symbol::A(SemanticAction::MakeFuncCall)},
                {Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeIndexedVar)},
                {Symbol::T(TokenType::DOT), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::MakeMemberAccess)}
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
                {Symbol::T(TokenType::OPEN_BRACKET), Symbol::N(NonTerminal::arithExpr), Symbol::T(TokenType::CLOSE_BRACKET), Symbol::A(SemanticAction::MakeIndexedVar)},
                {Symbol::T(TokenType::DOT), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::MakeMemberAccess)}
            }
        },
        // prog: PushMarker before classList, MakeClassList; PushMarker before funcDefList, MakeFuncDefList; funcBody→MakeStatBlock; MakeProgramBlock; MakeProg
        {
            NonTerminal::prog, {
                {Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::classList), Symbol::A(SemanticAction::MakeClassList), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::funcDefList), Symbol::A(SemanticAction::MakeFuncDefList), Symbol::T(TokenType::MAIN), Symbol::N(NonTerminal::funcBody), Symbol::A(SemanticAction::MakeProgramBlock), Symbol::A(SemanticAction::MakeProg)}
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
        // statBlock: PushMarker, stmtList, MakeStatBlock  OR  single statement wrapped in StatBlock
        // EPSILON case still emits an empty StatBlock so MakeIfStat/MakeWhileStat can always pop one
        {
            NonTerminal::statBlock, {
                {Symbol::T(TokenType::DO), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::stmtList), Symbol::T(TokenType::END), Symbol::A(SemanticAction::MakeStatBlock)},
                {Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::statement), Symbol::A(SemanticAction::MakeStatBlock)},
                {Symbol::A(SemanticAction::PushMarker), Symbol::A(SemanticAction::MakeStatBlock)}
            }
        },
        // statement: id → MakeId first; statementEnd decides AssignStat vs standalone call
        {
            NonTerminal::statement, {
                {Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::postfixList), Symbol::N(NonTerminal::statementEnd)},
                {Symbol::T(TokenType::IF), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::THEN), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::ELSE), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeIfStat)},
                {Symbol::T(TokenType::WHILE), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::N(NonTerminal::statBlock), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeWhileStat)},
                {Symbol::T(TokenType::READ), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::variable), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeReadStat)},
                {Symbol::T(TokenType::WRITE), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakePutStat)},
                {Symbol::T(TokenType::RETURN), Symbol::T(TokenType::OPEN_PARENTHESIS), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::CLOSE_PARENTHESIS), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeReturnStat)}
            }
        },
        // statementEnd: assign → MakeAssignStat; semicolon-only → statement is a lone call (no extra node)
        {
            NonTerminal::statementEnd, {
                {Symbol::N(NonTerminal::assignOp), Symbol::N(NonTerminal::expr), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeAssignStat)},
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
        // termTail: SaveOp captures the operator right after multOp matches
        {
            NonTerminal::termTail, {
                {Symbol::N(NonTerminal::multOp), Symbol::A(SemanticAction::SaveOp), Symbol::N(NonTerminal::factor), Symbol::N(NonTerminal::termTail), Symbol::A(SemanticAction::MakeMultOp)},
                EPSILON
            }
        },
        // type: MakeType is fired by the parent rule (varDecl, fParams, etc.) after matching type
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
        // varDecl: MakeType, MakeId, PushMarker for dims, MakeDimList, MakeVarDecl
        {
            NonTerminal::varDecl, {
                {Symbol::N(NonTerminal::type), Symbol::A(SemanticAction::MakeType), Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::A(SemanticAction::PushMarker), Symbol::N(NonTerminal::varArrayList), Symbol::A(SemanticAction::MakeDimList), Symbol::T(TokenType::SEMICOLON), Symbol::A(SemanticAction::MakeVarDecl)}
            }
        },
        {
            NonTerminal::varDeclList, {
                {Symbol::N(NonTerminal::varDecl), Symbol::N(NonTerminal::varDeclList)},
                EPSILON
            }
        },
        // variable: MakeId for the base id; postfixListNoCall handles member/index chains
        {
            NonTerminal::variable, {
                {Symbol::T(TokenType::ID), Symbol::A(SemanticAction::MakeId), Symbol::N(NonTerminal::postfixListNoCall)}
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
                        // skip semantic action symbols — they are invisible to FIRST/FOLLOW
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
                        // skip semantic action symbols
                        if (std::holds_alternative<SemanticAction>(p[i].value))
                            continue;

                        if (auto B = std::get_if<NonTerminal>(&p[i].value)) {
                            bool nullableSuffix = true;

                            for (size_t j = i + 1; j < p.size(); ++j) {
                                auto &next = p[j];

                                // skip semantic action symbols in suffix scan
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
                    // skip semantic action symbols — they are transparent to the parse table
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
                if (auto term = std::get_if<TokenType>(&sym.value))
                    res.append("'").append(lang::tokenTypeToCompString(*term)).append("' ");
                else if (auto nonterm = std::get_if<NonTerminal>(&sym.value))
                    res.append("<").append(to_string(*nonterm)).append("> ");
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
                        warn(token, std::format(R"(discarding unexpected token "{}" before end of file)", lang::tokenTypeToCompString(token.type)));
                        idx++;
                    } else if (token.type == TokenType::END_OF_FILE || isLikelyMissingDelimiter(*top_term)) {
                        warn(
                            token,
                            std::format(
                                R"(expected "{}", but got "{}"; inserting missing token)",
                                lang::tokenTypeToCompString(*top_term),
                                lang::tokenTypeToCompString(token.type)));
                        st.pop();
                    } else {
                        warn(
                            token,
                            std::format(
                                R"(expected "{}", but got "{}"; discarding unexpected token)",
                                lang::tokenTypeToCompString(*top_term),
                                lang::tokenTypeToCompString(token.type)));
                        idx++;
                    }

                    SYNTAX_ERROR();
                }
            } else {
                const auto A = std::get<NonTerminal>(top.value);

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

    ASTNodePtr SyntacticAnalyzer::getAST() const
    {
        return m_astRoot;
    }

    // -------------------------------------------------------------------------
    // AST node construction helpers
    // -------------------------------------------------------------------------

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

    // Pop children from the node stack down to (and including) the nearest marker,
    // reversing them so they are in left-to-right order.
    static std::vector<ASTNodePtr> popToMarker(std::stack<ASTNodePtr> &st)
    {
        std::vector<ASTNodePtr> children;
        while (!st.empty() && !isMarker(st.top())) {
            children.push_back(st.top());
            st.pop();
        }
        if (!st.empty())
            st.pop(); // pop the marker itself
        std::reverse(children.begin(), children.end());
        return children;
    }

    void SyntacticAnalyzer::executeAction(SemanticAction action)
    {


        switch (action) {

            // ---- Leaf nodes ----
            case SemanticAction::MakeId:
                m_nodeStack.push(makeNode(ASTNode::Kind::Id, m_lastToken.lexeme));
                break;

            case SemanticAction::MakeType:
                m_nodeStack.push(makeNode(ASTNode::Kind::Type, m_lastToken.lexeme));
                break;

            case SemanticAction::MakeNum:
                m_nodeStack.push(makeNode(ASTNode::Kind::Num, m_lastToken.lexeme));
                break;

            // MakeDim: top of stack is a Num (or nothing for empty [])
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

            // MakeDimList: pop all Dim nodes to marker, wrap in DimList
            case SemanticAction::MakeDimList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::DimList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }

            // MakeVarDecl: DimList, Id, Type are on stack (top→bottom order)
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

            // MakeParamList: pop VarDecl (or expr for actual args) nodes to marker
            case SemanticAction::MakeParamList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::ParamList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }

            // MakeClass: pop VarDecl members to marker, then Id; assemble Class(Id, members...)
            case SemanticAction::MakeClass:
                {
                    auto members = popToMarker(m_nodeStack);
                    // The Id was pushed before the PushMarker... but our grammar pushes
                    // MakeId BEFORE PushMarker, so Id is just below the marker.
                    // Actually after popToMarker the marker is consumed; Id is now on top.
                    ASTNodePtr id;
                    if (!m_nodeStack.empty() && !isMarker(m_nodeStack.top()) && m_nodeStack.top()->kind == ASTNode::Kind::Id) {
                        id = m_nodeStack.top();
                        m_nodeStack.pop();
                    }
                    auto node = makeNode(ASTNode::Kind::Class);
                    if (id)
                        node->children.push_back(id);
                    for (auto &m : members) node->children.push_back(m);
                    m_nodeStack.push(node);
                    break;
                }

            // MakeClassList: pop Class nodes to marker, wrap in ClassList
            case SemanticAction::MakeClassList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::ClassList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }

            // MakeFuncDef: stack (top→bottom): StatBlock, Type(return), ParamList, Id(funcname)
            // funcHead pushes: Id, then funcHeadTail pushes ParamList then Type
            // funcBody pushes: StatBlock
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
                    auto id = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::FuncDef);
                    node->children = { type, id, paramList, statBlock };
                    m_nodeStack.push(node);
                    break;
                }

            // MakeFuncDefList: pop FuncDef nodes to marker, wrap in FuncDefList
            case SemanticAction::MakeFuncDefList:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::FuncDefList);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }

            // MakeStatBlock: pop statements/decls to marker, wrap in StatBlock
            case SemanticAction::MakeStatBlock:
                {
                    auto children = popToMarker(m_nodeStack);
                    auto node = makeNode(ASTNode::Kind::StatBlock);
                    node->children = std::move(children);
                    m_nodeStack.push(node);
                    break;
                }

            // MakeProgramBlock: the main body StatBlock is on top; wrap it in ProgramBlock
            case SemanticAction::MakeProgramBlock:
                {
                    // The funcBody production emits a StatBlock which contains local decls + stmts.
                    // We simply re-label it as ProgramBlock.
                    if (!m_nodeStack.empty() && m_nodeStack.top()->kind == ASTNode::Kind::StatBlock) {
                        m_nodeStack.top()->kind = ASTNode::Kind::ProgramBlock;
                    }
                    break;
                }

            // MakeProg: stack (top→bottom): ProgramBlock, FuncDefList, ClassList
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

            // ---- Statement nodes ----
            case SemanticAction::MakeAssignStat:
                {
                    // stack (top→bottom): rhs expr, lhs (Id or member/index chain)
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
                    // stack (top→bottom): else-StatBlock, then-StatBlock, condition expr
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
                    // stack (top→bottom): body-StatBlock, condition expr
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

            // ---- Binary expression nodes ----
            // For AddOp/MultOp/RelOp the last-matched terminal holds the operator lexeme.
            // Because arithExprTail is right-recursive and MakeAddOp fires after the tail,
            // the stack looks like (top→bottom): right subtree result, left subtree result.
            case SemanticAction::MakeAddOp:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto right = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto left = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::AddOp, m_savedOperator);
                    node->children = { left, right };
                    m_nodeStack.push(node);
                    break;
                }

            case SemanticAction::MakeMultOp:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto right = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto left = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::MultOp, m_savedOperator);
                    node->children = { left, right };
                    m_nodeStack.push(node);
                    break;
                }

            case SemanticAction::MakeRelOp:
                {
                    if (m_nodeStack.size() < 2)
                        break;
                    auto right = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto left = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::RelOp, m_savedOperator);
                    node->children = { left, right };
                    m_nodeStack.push(node);
                    break;
                }

            // ---- Unary expression nodes ----
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
                    auto operand = m_nodeStack.top();
                    m_nodeStack.pop();
                    auto node = makeNode(ASTNode::Kind::SignExpr, m_lastToken.lexeme);
                    node->children = { operand };
                    m_nodeStack.push(node);
                    break;
                }

            // ---- Postfix / call / member access ----
            // MakeFuncCall: stack (top→bottom): ParamList (actual args), Id (function name)
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

            // MakeIndexedVar: stack (top→bottom): index expr, base variable
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

            // MakeMemberAccess: stack (top→bottom): member Id, object expr
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

            // PushMarker: push a sentinel onto the node stack to delimit list boundaries
            case SemanticAction::PushMarker:
                m_nodeStack.push(makeNode(ASTNode::Kind::Marker));
                break;

            // SaveOp: capture the current last-matched token's lexeme as the pending operator
            case SemanticAction::SaveOp:
                m_savedOperator = m_lastToken.lexeme;
                break;
        }
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
