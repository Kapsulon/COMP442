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
#include "Problems/Problems.hpp"

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
        MakeSavedId,
        MakeType,
        MakeSavedType,
        MakeNum,
        MakeDim,
        MakeDimList,
        MakeVarDecl,
        MakeMemberVarDecl,
        MakeParamList,
        MakeInheritList,
        MakeClass,
        MakeClassList,
        MakeFuncDef,
        MakeFuncDecl,
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
        SaveOp,
        SaveLeadId,
        SaveVisibility
    };

    inline std::string to_string(NonTerminal nt)
    {
        switch (nt) {
            case NonTerminal::START:
                return "START";
            case NonTerminal::prog:
                return "prog";
            case NonTerminal::classList:
                return "classList";
            case NonTerminal::classDecl:
                return "classDecl";
            case NonTerminal::classInheritOpt:
                return "classInheritOpt";
            case NonTerminal::classInheritTail:
                return "classInheritTail";
            case NonTerminal::classMemberList:
                return "classMemberList";
            case NonTerminal::visibility:
                return "visibility";
            case NonTerminal::memberDecl:
                return "memberDecl";
            case NonTerminal::memberAfterId:
                return "memberAfterId";
            case NonTerminal::funcDeclTail:
                return "funcDeclTail";
            case NonTerminal::funcDef:
                return "funcDef";
            case NonTerminal::funcDefList:
                return "funcDefList";
            case NonTerminal::funcHead:
                return "funcHead";
            case NonTerminal::funcHeadTail:
                return "funcHeadTail";
            case NonTerminal::funcHeadReturn:
                return "funcHeadReturn";
            case NonTerminal::funcBody:
                return "funcBody";
            case NonTerminal::localDeclOpt:
                return "localDeclOpt";
            case NonTerminal::varDecl:
                return "varDecl";
            case NonTerminal::varDeclList:
                return "varDeclList";
            case NonTerminal::varArrayList:
                return "varArrayList";
            case NonTerminal::arraySize:
                return "arraySize";
            case NonTerminal::arraySizeTail:
                return "arraySizeTail";
            case NonTerminal::fParams:
                return "fParams";
            case NonTerminal::fParamsTail:
                return "fParamsTail";
            case NonTerminal::fParamsArrayList:
                return "fParamsArrayList";
            case NonTerminal::aParams:
                return "aParams";
            case NonTerminal::aParamsTail:
                return "aParamsTail";
            case NonTerminal::stmtList:
                return "stmtList";
            case NonTerminal::statBlock:
                return "statBlock";
            case NonTerminal::statement:
                return "statement";
            case NonTerminal::statementEnd:
                return "statementEnd";
            case NonTerminal::assignOp:
                return "assignOp";
            case NonTerminal::variable:
                return "variable";
            case NonTerminal::postfix:
                return "postfix";
            case NonTerminal::postfixList:
                return "postfixList";
            case NonTerminal::postfixNoCall:
                return "postfixNoCall";
            case NonTerminal::postfixListNoCall:
                return "postfixListNoCall";
            case NonTerminal::indice:
                return "indice";
            case NonTerminal::expr:
                return "expr";
            case NonTerminal::exprRelTail:
                return "exprRelTail";
            case NonTerminal::relOp:
                return "relOp";
            case NonTerminal::arithExpr:
                return "arithExpr";
            case NonTerminal::arithExprTail:
                return "arithExprTail";
            case NonTerminal::term:
                return "term";
            case NonTerminal::termTail:
                return "termTail";
            case NonTerminal::factor:
                return "factor";
            case NonTerminal::multOp:
                return "multOp";
            case NonTerminal::addOp:
                return "addOp";
            case NonTerminal::sign:
                return "sign";
            case NonTerminal::type:
                return "type";
            case NonTerminal::type_no_id:
                return "type_no_id";
        }
        return "?";
    }

    inline std::string to_string(ASTNode::Kind kind)
    {
        switch (kind) {
            case ASTNode::Kind::Prog:
                return "Prog";
            case ASTNode::Kind::ClassList:
                return "ClassList";
            case ASTNode::Kind::Class:
                return "Class";
            case ASTNode::Kind::InheritList:
                return "InheritList";
            case ASTNode::Kind::FuncDefList:
                return "FuncDefList";
            case ASTNode::Kind::FuncDef:
                return "FuncDef";
            case ASTNode::Kind::FuncDecl:
                return "FuncDecl";
            case ASTNode::Kind::ProgramBlock:
                return "ProgramBlock";
            case ASTNode::Kind::ParamList:
                return "ParamList";
            case ASTNode::Kind::StatBlock:
                return "StatBlock";
            case ASTNode::Kind::VarDecl:
                return "VarDecl";
            case ASTNode::Kind::DimList:
                return "DimList";
            case ASTNode::Kind::AssignStat:
                return "AssignStat";
            case ASTNode::Kind::PutStat:
                return "PutStat";
            case ASTNode::Kind::ReturnStat:
                return "ReturnStat";
            case ASTNode::Kind::IfStat:
                return "IfStat";
            case ASTNode::Kind::WhileStat:
                return "WhileStat";
            case ASTNode::Kind::ReadStat:
                return "ReadStat";
            case ASTNode::Kind::FuncCall:
                return "FuncCall";
            case ASTNode::Kind::AddOp:
                return "AddOp";
            case ASTNode::Kind::MultOp:
                return "MultOp";
            case ASTNode::Kind::RelOp:
                return "RelOp";
            case ASTNode::Kind::NotExpr:
                return "NotExpr";
            case ASTNode::Kind::SignExpr:
                return "SignExpr";
            case ASTNode::Kind::IndexedVar:
                return "IndexedVar";
            case ASTNode::Kind::MemberAccess:
                return "MemberAccess";
            case ASTNode::Kind::Id:
                return "Id";
            case ASTNode::Kind::Type:
                return "Type";
            case ASTNode::Kind::Dim:
                return "Dim";
            case ASTNode::Kind::Num:
                return "Num";
            case ASTNode::Kind::Marker:
                return "Marker";
        }
        return "?";
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
        SyntacticAnalyzer();

        void openFile(std::string_view path);
        void parse();

        ASTNodePtr getAST() const;

        std::string getFirstSet();
        std::string getFollowSet();

        std::string_view getCurrentFilePath() const;

        void outputSyntaxErrors();
        void outputDerivationSteps();
        void outputDotAST();

        const LexicalAnalyzer &getLexer() const;

    private:
        LexicalAnalyzer m_lexicalAnalyzer;
        std::vector<Token> m_tokens;
        std::string m_currentFilePath;

        Problems m_problems;
        std::string m_outDerivationSteps;

        std::stack<ASTNodePtr> m_nodeStack;
        ASTNodePtr m_astRoot;
        Token m_lastToken{ TokenType::END_OF_FILE, "", 0, 0, "" };
        std::vector<std::string> m_savedOperators;
        std::string m_savedLeadId;
        std::string m_currentVisibility;

        void executeAction(SemanticAction action);
        void writeDerivationSteps(const NonTerminal &A, const ParseTableEntry &entry);

        void closeFile();
        void lex();

        bool isEpsilon(const FirstSymbol &s);

        FirstSet generateFirstSet();
        FollowSet generateFollowSet();
        ParseTable generateParseTable();

        static void WireASTParents(ASTNode *parent, ASTNode *child)
        {
            if (parent)
                child->parent = parent;

            for (const auto &grandChild : child->children) WireASTParents(child, grandChild.get());
        }

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
