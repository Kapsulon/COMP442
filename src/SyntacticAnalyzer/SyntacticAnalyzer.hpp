#pragma once

#include <cstddef>
#include <stack>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "LexicalAnalyzer/LexicalAnalyzer.hpp"

namespace lang
{
    enum class NonTerminal {
        START,
        prog,
        classList,
        classDecl,
        classInheritOpt,
        classInheritTail,
        classMemberList,
        visibility,
        memberDecl,
        funcDeclTail,
        funcDef,
        funcDefList,
        funcHead,
        funcHeadTail,
        funcHeadReturn,
        funcBody,
        localDeclOpt,
        varDecl,
        varDeclList,
        varArrayList,
        arraySize,
        arraySizeTail,
        fParams,
        fParamsTail,
        fParamsArrayList,
        aParams,
        aParamsTail,
        stmtList,
        statBlock,
        statement,
        statementEnd,
        assignOp,
        variable,
        postfix,
        postfixList,
        postfixNoCall,
        postfixListNoCall,
        indice,
        expr,
        exprRelTail,
        relOp,
        arithExpr,
        arithExprTail,
        term,
        termTail,
        factor,
        multOp,
        addOp,
        sign,
        type,
        type_no_id
    };

    inline std::string to_string(NonTerminal nt)
    {
        static const std::unordered_map<NonTerminal, std::string> names = { { NonTerminal::START, "START" },
                                                                            { NonTerminal::prog, "prog" },
                                                                            { NonTerminal::classList, "classList" },
                                                                            { NonTerminal::classDecl, "classDecl" },
                                                                            { NonTerminal::classInheritOpt, "classInheritOpt" },
                                                                            { NonTerminal::classInheritTail, "classInheritTail" },
                                                                            { NonTerminal::classMemberList, "classMemberList" },
                                                                            { NonTerminal::visibility, "visibility" },
                                                                            { NonTerminal::memberDecl, "memberDecl" },
                                                                            { NonTerminal::funcDeclTail, "funcDeclTail" },
                                                                            { NonTerminal::funcDef, "funcDef" },
                                                                            { NonTerminal::funcDefList, "funcDefList" },
                                                                            { NonTerminal::funcHead, "funcHead" },
                                                                            { NonTerminal::funcHeadTail, "funcHeadTail" },
                                                                            { NonTerminal::funcHeadReturn, "funcHeadReturn" },
                                                                            { NonTerminal::funcBody, "funcBody" },
                                                                            { NonTerminal::localDeclOpt, "localDeclOpt" },
                                                                            { NonTerminal::varDecl, "varDecl" },
                                                                            { NonTerminal::varDeclList, "varDeclList" },
                                                                            { NonTerminal::varArrayList, "varArrayList" },
                                                                            { NonTerminal::arraySize, "arraySize" },
                                                                            { NonTerminal::arraySizeTail, "arraySizeTail" },
                                                                            { NonTerminal::fParams, "fParams" },
                                                                            { NonTerminal::fParamsTail, "fParamsTail" },
                                                                            { NonTerminal::fParamsArrayList, "fParamsArrayList" },
                                                                            { NonTerminal::aParams, "aParams" },
                                                                            { NonTerminal::aParamsTail, "aParamsTail" },
                                                                            { NonTerminal::stmtList, "stmtList" },
                                                                            { NonTerminal::statBlock, "statBlock" },
                                                                            { NonTerminal::statement, "statement" },
                                                                            { NonTerminal::statementEnd, "statementEnd" },
                                                                            { NonTerminal::assignOp, "assignOp" },
                                                                            { NonTerminal::variable, "variable" },
                                                                            { NonTerminal::postfix, "postfix" },
                                                                            { NonTerminal::postfixList, "postfixList" },
                                                                            { NonTerminal::postfixNoCall, "postfixNoCall" },
                                                                            { NonTerminal::postfixListNoCall, "postfixListNoCall" },
                                                                            { NonTerminal::indice, "indice" },
                                                                            { NonTerminal::expr, "expr" },
                                                                            { NonTerminal::exprRelTail, "exprRelTail" },
                                                                            { NonTerminal::relOp, "relOp" },
                                                                            { NonTerminal::arithExpr, "arithExpr" },
                                                                            { NonTerminal::arithExprTail, "arithExprTail" },
                                                                            { NonTerminal::term, "term" },
                                                                            { NonTerminal::termTail, "termTail" },
                                                                            { NonTerminal::factor, "factor" },
                                                                            { NonTerminal::multOp, "multOp" },
                                                                            { NonTerminal::addOp, "addOp" },
                                                                            { NonTerminal::sign, "sign" },
                                                                            { NonTerminal::type, "type" },
                                                                            { NonTerminal::type_no_id, "type_no_id" } };
        return names.at(nt);
    }

    struct Symbol {
        bool is_terminal;
        TokenType term;
        NonTerminal nonterm;

        static Symbol T(TokenType t)
        {
            return { true, t, {} };
        }

        static Symbol N(NonTerminal n)
        {
            return { false, {}, n };
        }
    };

    using Production = std::vector<Symbol>;
    using Grammar = std::unordered_map<NonTerminal, std::vector<Production>>;

    struct EpsilonTag {
        friend constexpr bool operator==(EpsilonTag, EpsilonTag) = default;
    };
    constexpr EpsilonTag EPS{};

    using FirstSymbol = std::variant<TokenType, EpsilonTag>;
    using FirstSet = std::unordered_map<NonTerminal, std::unordered_set<FirstSymbol>>;
    using FollowSet = std::unordered_map<NonTerminal, std::unordered_set<TokenType>>;

    using ParseTable = std::unordered_map<NonTerminal, std::unordered_map<TokenType, Production>>;

    class SyntacticAnalyzer
    {
    public:
        SyntacticAnalyzer();

        void openFile(std::string_view path);
        std::stack<Symbol> parse();

        std::string getFirstSet();
        std::string getFollowSet();

    private:
        LexicalAnalyzer m_lexicalAnalyzer;
        std::vector<Token> m_tokens;

        void closeFile();
        void lex();

        bool isEpsilon(const FirstSymbol &s);

        FirstSet generateFirstSet();
        FollowSet generateFollowSet();
        ParseTable generateParseTable();

        static const Grammar grammar;
        const FirstSet m_firstSet;
        const FollowSet m_followSet;
        const ParseTable m_parseTable;
    };
} // namespace lang

namespace std
{
    template <> struct hash<lang::EpsilonTag> {
        size_t operator()(const lang::EpsilonTag &) const noexcept
        {
            return static_cast<size_t>(0x9E3779B97F4A7C15ull);
        }
    };
} // namespace std
