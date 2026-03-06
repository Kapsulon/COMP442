#pragma once

#include <cstddef>
#include <fstream>
#include <stack>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "AST/ASTNode.hpp"
#include "LexicalAnalyzer/LexicalAnalyzer.hpp"

// clang-format off
#define EPSILON {}
// clang-format on

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
        memberAfterId,
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

    enum class SemanticAction {
        MakeId,
        MakeType,
        MakeNum,
        MakeDim,
        MakeDimList,
        MakeVarDecl,
        MakeParamList,
        MakeClass,
        MakeClassList,
        MakeFuncDef,
        MakeFuncDefList,
        MakeStatBlock,
        MakeProgramBlock,
        MakeProg,
        MakeAssignStat,
        MakePutStat,
        MakeReturnStat,
        MakeIfStat,
        MakeWhileStat,
        MakeReadStat,
        MakeAddOp,
        MakeMultOp,
        MakeRelOp,
        MakeNotExpr,
        MakeSignExpr,
        MakeFuncCall,
        MakeIndexedVar,
        MakeMemberAccess,
        PushMarker,
        SaveOp // saves m_lastToken.lexeme as the pending binary operator
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
                                                                            { NonTerminal::memberAfterId, "memberAfterId" },
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
        std::variant<TokenType, NonTerminal, SemanticAction> value;

        static Symbol T(TokenType t)
        {
            return { t };
        }

        static Symbol N(NonTerminal n)
        {
            return { n };
        }

        static Symbol A(SemanticAction s)
        {
            return { s };
        }
    };

    using Production = std::vector<Symbol>;
    using Grammar = std::unordered_map<NonTerminal, std::vector<Production>>;

    namespace tags
    {
        struct EpsilonTag {
            friend constexpr bool operator==(EpsilonTag, EpsilonTag) = default;
        };
        constexpr EpsilonTag EPS{};

        struct SyncTag {
            friend constexpr bool operator==(SyncTag, SyncTag) = default;
        };
        constexpr SyncTag SYNC{};
    } // namespace tags

    using FirstSymbol = std::variant<TokenType, tags::EpsilonTag>;
    using FirstSet = std::unordered_map<NonTerminal, std::unordered_set<FirstSymbol>>;
    using FollowSet = std::unordered_map<NonTerminal, std::unordered_set<TokenType>>;

    using ParseTableEntry = std::variant<Production, tags::SyncTag>;
    using ParseTable = std::unordered_map<NonTerminal, std::unordered_map<TokenType, ParseTableEntry>>;

    class SyntacticAnalyzer
    {
    public:
        SyntacticAnalyzer(bool outputFiles = false);

        void openFile(std::string_view path);
        void parse();

        ASTNodePtr getAST() const;

        std::string getFirstSet();
        std::string getFollowSet();

    private:
        LexicalAnalyzer m_lexicalAnalyzer;
        std::vector<Token> m_tokens;
        std::string m_currentFilePath;

        bool m_outputFiles;
        std::ofstream m_outParseErrors;
        std::ofstream m_outDerivation;

        // AST construction state
        std::stack<ASTNodePtr> m_nodeStack;
        ASTNodePtr m_astRoot;
        Token m_lastToken{ TokenType::END_OF_FILE, "", 0, 0, "" };
        std::string m_savedOperator; // saved operator lexeme for binary op nodes

        void executeAction(SemanticAction action);

        void closeFile();
        void lex();

        bool isEpsilon(const FirstSymbol &s);

        FirstSet generateFirstSet();
        FollowSet generateFollowSet();
        ParseTable generateParseTable();

        void error(const Token &token, const std::string &message);
        void warn(const Token &token, const std::string &message);

        void outputDerivationStep(const NonTerminal &A, const ParseTableEntry &entry);

        static const Grammar grammar;
        const FirstSet m_firstSet;
        const FollowSet m_followSet;
        const ParseTable m_parseTable;
    };
} // namespace lang

namespace std
{
    template <> struct hash<lang::tags::EpsilonTag> {
        size_t operator()(const lang::tags::EpsilonTag &) const noexcept
        {
            return static_cast<size_t>(0x9E3779B97F4A7C15ull);
        }
    };
} // namespace std
