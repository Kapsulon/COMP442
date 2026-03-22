#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AST/ASTNode.hpp"
#include "Problems/Problems.hpp"
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

#define GENERATOR_ARGS SymbolTableNode *node, std::shared_ptr<const ASTNode> ast

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
            if (kind == Kind::Table)
                return "table";

            if (kind == Kind::Class)
                return "class";

            if (kind == Kind::Inherit)
                return "inherit";

            if (kind == Kind::Function)
                return "function";

            if (kind == Kind::Parameter)
                return "param";

            if (kind == Kind::Local)
                return "local";

            if (kind == Kind::Data)
                return "data";

            return "";
        }

        enum class Visibility {
            None,
            Public,
            Private
        };

        static std::string ToString(Visibility visibility)
        {
            if (visibility == Visibility::Public)
                return "public";

            if (visibility == Visibility::Private)
                return "private";

            return "";
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
        std::string name = "";
        Signature signature;
        Visibility visibility = Visibility::None;

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

    private:
        Problems m_problems;
        SyntacticAnalyzer m_syntacticAnalyzer;
        SymbolTableNode *m_symbolTable = nullptr;

        // clang-format off
        const std::unordered_map<ASTNode::Kind, std::function<void(SymbolTableNode *node, std ::shared_ptr<const ASTNode> ast)>> m_generators = {
            {
                ASTNode::Kind::Prog, [&](GENERATOR_ARGS) {
                    node->kind = SymbolTableNode::Kind::Table;
                    node->name = "global";
                },
            },
            {
                ASTNode::Kind::Class, [&](GENERATOR_ARGS) {
                    node->kind = SymbolTableNode::Kind::Class;
                    node->name = getExpectedChild(ast, 0, ASTNode::Kind::Id)->lexeme;

                    if (std::none_of(ast->children.begin(), ast->children.end(), [](const std::shared_ptr<const ASTNode> &child) { return child->kind == ASTNode::Kind::InheritList; })) {
                        node->table.emplace_back(new SymbolTableNode{
                            .kind = SymbolTableNode::Kind::Inherit,
                            .name = "none"
                        });
                    }
                }
            },
            {
                ASTNode::Kind::InheritList, [&](GENERATOR_ARGS) {
                    node->kind = SymbolTableNode::Kind::Inherit;

                    for (auto &child : ast->children) {
                        if (child->kind == ASTNode::Kind::Id) {
                            node->name += child->lexeme;
                            if (child != ast->children.back())
                                node->name += ", ";
                        }
                    }
                }
            },
            {
                ASTNode::Kind::FuncDecl, [&](GENERATOR_ARGS) {
                    node->kind = SymbolTableNode::Kind::Function;
                    node->visibility = SymbolTableNode::FromString(ast->lexeme);
                    node->name = getExpectedChild(ast, 1, ASTNode::Kind::Id)->lexeme;

                    node->signature.type = getExpectedChild(ast, 0, ASTNode::Kind::Type)->lexeme;

                    for (auto param : getExpectedChild(ast, 2, ASTNode::Kind::ParamList)->children)
                        node->signature.params.emplace_back(getExpectedChild(param, 0, ASTNode::Kind::Type)->lexeme);
                }
            },
            {
                ASTNode::Kind::VarDecl, [&](GENERATOR_ARGS) {
                    switch (ast->parent->kind) {
                        case ASTNode::Kind::Class:
                            node->kind = SymbolTableNode::Kind::Data;
                            break;
                        case ASTNode::Kind::ParamList:
                            node->kind = SymbolTableNode::Kind::Parameter;
                            break;
                        case ASTNode::Kind::StatBlock:
                            node->kind = SymbolTableNode::Kind::Local;
                            break;
                        case ASTNode::Kind::ProgramBlock:
                            node->kind = SymbolTableNode::Kind::Local;
                            break;
                        default:
                            spdlog::warn("Unexpected parent kind {} for VarDecl", lang::to_string(ast->parent->kind));
                            break;
                    }

                    node->visibility = SymbolTableNode::FromString(ast->lexeme);

                    node->signature.type = getExpectedChild(ast, 0, ASTNode::Kind::Type)->lexeme;
                    node->name = getExpectedChild(ast, 1, ASTNode::Kind::Id)->lexeme;

                    for (auto dim : getExpectedChild(ast, 2, ASTNode::Kind::DimList)->children) {
                        node->signature.type += std::format("[{}]", dim->lexeme);
                    }
                }
            }
        };
        // clang-format on

        SymbolTableNode *generateSymbolTable();
        SymbolTableNode *applyGenerator(SymbolTableNode *parent, std::shared_ptr<const ASTNode> ast);
        std::shared_ptr<const ASTNode> getExpectedChild(std::shared_ptr<const ASTNode> ast, int index, ASTNode::Kind kind);

        static std::string GetFullNamespace(const SymbolTableNode *node);

        void semanticChecks();
    };
} // namespace lang
