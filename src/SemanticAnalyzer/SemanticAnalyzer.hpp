#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AST/ASTNode.hpp"
#include "Problems/Problems.hpp"
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"
#include "tabulate/table.hpp"

namespace lang
{
    struct SymbolTableNode {
        enum class Kind {
            None,
            Table,
            Class,
            Inherit,
            Function,
            Parameter,
            Local,
            Data
        };

        static std::string ToString(Kind kind)
        {
            switch (kind) {
                case Kind::Table:
                    return "table";
                case Kind::Class:
                    return "class";
                case Kind::Inherit:
                    return "inherit";
                case Kind::Function:
                    return "function";
                case Kind::Parameter:
                    return "param";
                case Kind::Local:
                    return "local";
                case Kind::Data:
                    return "data";
                default:
                    return "";
            }
        }

        enum class Visibility {
            None,
            Public,
            Private
        };

        static std::string ToString(Visibility visibility)
        {
            switch (visibility) {
                case Visibility::Public:
                    return "public";
                case Visibility::Private:
                    return "private";
                default:
                    return "";
            }
        }

        static Visibility FromString(const std::string &src)
        {
            if (src == "public")
                return Visibility::Public;
            if (src == "private")
                return Visibility::Private;
            return Visibility::None;
        }

        struct Signature {
            std::string type;
            std::vector<std::string> params;
        };

        Kind kind = Kind::None;
        std::string name;
        Signature signature;
        Visibility visibility = Visibility::None;
        Token token;

        std::vector<SymbolTableNode *> table;
        SymbolTableNode *parent = nullptr;
    };

    class SemanticAnalyzer
    {
    public:
        ~SemanticAnalyzer();

        void openFile(std::string_view path);
        void parse();
        void outputSymbolTable() const;
        void outputSemanticErrors() const;

    private:
        Problems m_problems;
        SyntacticAnalyzer m_syntacticAnalyzer;
        SymbolTableNode *m_symbolTable = nullptr;
        std::unordered_map<std::string, std::string> m_classTypeNames;
        std::shared_ptr<const ASTNode> m_ast;

        SymbolTableNode *generateSymbolTable(std::shared_ptr<const ASTNode> ast);

        SymbolTableNode *makeSymbol(SymbolTableNode::Kind kind, const std::string &name, SymbolTableNode *parent, Token token = {}) const;
        SymbolTableNode *findClassSymbol(SymbolTableNode *globalTable, const std::string &className) const;
        SymbolTableNode *findMemberFunctionSymbol(
            SymbolTableNode *classNode, const std::string &functionName, const std::vector<std::string> &paramTypes, const std::string &returnType) const;

        void buildClassTables(SymbolTableNode *globalTable, std::shared_ptr<const ASTNode> classList);
        void buildFunctionDefinitions(SymbolTableNode *globalTable, std::shared_ptr<const ASTNode> funcDefList);
        void buildMainFunction(SymbolTableNode *globalTable, std::shared_ptr<const ASTNode> programBlock);
        void populateFunctionTable(SymbolTableNode *function, std::shared_ptr<const ASTNode> paramList, std::shared_ptr<const ASTNode> statBlock);

        std::string normalizeType(std::string typeName) const;
        std::string varDeclType(std::shared_ptr<const ASTNode> varDecl) const;
        std::string joinInheritedTypes(std::shared_ptr<const ASTNode> inheritList) const;
        std::vector<std::string> parameterTypes(std::shared_ptr<const ASTNode> paramList) const;
        std::string lowercase(std::string src) const;

        tabulate::Table::Row_t renderRow(const SymbolTableNode *node) const;
        tabulate::Table renderTable(const SymbolTableNode *node) const;
        std::string renderSymbolTable() const;
        static std::string GetFullNamespace(const SymbolTableNode *node);

        void semanticChecks();

        void checkMultiplyDeclared(SymbolTableNode *globalTable);
        void checkMultiplyDeclaredInFunction(SymbolTableNode *funcNode);
        void checkFunctionDeclarationDefinitionParity(SymbolTableNode *globalTable);
        void checkOverloadsAndOverrides(SymbolTableNode *globalTable);
        void checkShadowing(SymbolTableNode *globalTable);
        void checkCircularDependencies(SymbolTableNode *globalTable);
        void checkUndeclaredClassTypes(SymbolTableNode *globalTable);

        struct ScopeContext {
            SymbolTableNode *globalTable = nullptr;
            SymbolTableNode *classNode = nullptr;
            SymbolTableNode *functionNode = nullptr;
        };

        void checkAllFunctionBodies(SymbolTableNode *globalTable);
        void checkStatBlock(std::shared_ptr<const ASTNode> statBlock, const ScopeContext &ctx);
        void checkStatement(std::shared_ptr<const ASTNode> stmt, const ScopeContext &ctx);
        void checkAssignStat(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx);
        void checkReturnStat(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx);
        void checkFuncCallStat(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx);

        std::string inferType(std::shared_ptr<const ASTNode> expr, const ScopeContext &ctx);
        std::string inferTypeId(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx);
        std::string inferTypeMemberAccess(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx);
        std::string inferTypeIndexedVar(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx);
        std::string inferTypeFuncCall(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx);

        SymbolTableNode *lookupInScope(const std::string &name, const ScopeContext &ctx) const;
        SymbolTableNode *lookupInClass(SymbolTableNode *classNode, const std::string &name) const;
        SymbolTableNode *findClassByName(const std::string &name) const;
        SymbolTableNode *findFreeFunctionByName(const std::string &name) const;
        SymbolTableNode *findFreeFunctionByNameAndArgs(const std::string &name, const std::vector<std::string> &argTypes) const;

        static int CountArrayDimensions(const std::string &type);
        static std::string StripAllDimensions(const std::string &type);
        static std::string StripOneDimension(const std::string &type);
        static bool TypesCompatible(const std::string &a, const std::string &b);
        std::vector<SymbolTableNode *> collectInheritedClasses(SymbolTableNode *classNode) const;
    };
} // namespace lang
