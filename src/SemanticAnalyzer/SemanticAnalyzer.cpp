#include "SemanticAnalyzer.hpp"
#include <algorithm>
#include <cassert>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include "AST/ASTNode.hpp"
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"
#include "utils/colors.hpp"

namespace lang
{
    void SemanticAnalyzer::openFile(std::string_view path)
    {
        m_syntacticAnalyzer.openFile(path);
    }

    void SemanticAnalyzer::parse()
    {
        m_syntacticAnalyzer.parse();
        auto ast = m_syntacticAnalyzer.getAST();

        if (!ast) {
            spdlog::warn("{}: No AST produced (parse errors?)", m_syntacticAnalyzer.getCurrentFilePath());
            return;
        }

        auto start = std::chrono::high_resolution_clock::now();

        auto symbolTable = generateSymbolTable();

        if (!symbolTable) {
            spdlog::error("{}: Failed to generate symbol table", m_syntacticAnalyzer.getCurrentFilePath());
            return;
        }

        m_symbolTable = symbolTable.value();

        semanticChecks();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        m_problems.displayProblems(m_syntacticAnalyzer.getLexer());

        spdlog::info(
            "{}: Generated Symbol Table in \t\t {:.2f}ms \t [" CYAN "{} info(s)" RESET ", " YELLOW "{} warning(s)" RESET ", " RED "{} error(s)" RESET "]",
            m_syntacticAnalyzer.getCurrentFilePath(),
            elapsed.count(),
            m_problems.getInfoCount(),
            m_problems.getWarningCount(),
            m_problems.getErrorCount());
    }

    void SemanticAnalyzer::outputSymbolTable() const
    {
        std::function<std::string(const SymbolTableNode &, int)> printNode = [&](const SymbolTableNode &node, int indent) {
            std::string out = "\n";
            const std::string indentString(indent, ' ');

            out.append(indentString);
            if (node.kind != SymbolTableNode::Kind::Table) {
                out.append(std::format("{:<9} | {:<11}", node.ToString(node.kind), node.name));
                if (!node.signature.type.empty() || !node.signature.params.empty()) {
                    std::string signature = " | ";
                    if (node.kind == SymbolTableNode::Kind::Function) {
                        signature += "(";
                        for (const std::string &param : node.signature.params) {
                            signature += param;
                            if (&param != &node.signature.params.back())
                                signature += ", ";
                        }
                        signature += "): ";
                    }
                    signature += node.signature.type;
                    out += std::format("{:<40}", signature);
                }
                if (node.visibility != SymbolTableNode::Visibility::None) {
                    out += std::format(" | {:<9}", SymbolTableNode::ToString(node.visibility));
                }

                out += " |\n";
            }

            if (!node.table.empty()) {
                if (node.kind != SymbolTableNode::Kind::Table)
                    out += indentString + "    ";
                out += std::format("table: {}\n", GetFullNamespace(&node));
                for (auto &child : node.table) out += printNode(child, indent + 4);
            }

            return out;
        };

        std::cout << printNode(m_symbolTable, 0) << '\n';
    }

    std::string SemanticAnalyzer::GetFullNamespace(const SymbolTableNode *node)
    {
        std::string out = node->name;
        std::vector<std::string> namespaces;

        if (node->parent != nullptr) {
            for (SymbolTableNode *current = node->parent; current->kind != SymbolTableNode::Kind::Table; current = current->parent) {
                spdlog::info(out);
                namespaces.emplace_back(current->name);
            }
        }

        spdlog::info("hi");

        std::reverse(namespaces.begin(), namespaces.end());

        spdlog::info("hi2");

        for (const std::string &space : namespaces) {
            out += space;
            if (&space != &namespaces.back())
                out += "::";
        }

        return out;
    }

    std::optional<SymbolTableNode> SemanticAnalyzer::generateSymbolTable()
    {
        return applyGenerator(nullptr, m_syntacticAnalyzer.getAST());
    }

    std::optional<SymbolTableNode> SemanticAnalyzer::applyGenerator(SymbolTableNode *parent, std::shared_ptr<const ASTNode> ast)
    {
        if (m_generators.contains(ast->kind)) {
            SymbolTableNode node;
            node.parent = parent;

            m_generators.at(ast->kind)(&node, ast);

            for (auto child : ast->children) {
                auto ret = applyGenerator(&node, child);

                if (ret)
                    node.table.emplace_back(ret.value());
            }

            return node;
        } else {
            for (auto child : ast->children) {
                auto ret = applyGenerator(parent, child);

                if (ret)
                    parent->table.emplace_back(ret.value());
            }

            return std::nullopt;
        }
    }

    std::shared_ptr<const ASTNode> SemanticAnalyzer::getExpectedChild(std::shared_ptr<const ASTNode> ast, int index, ASTNode::Kind kind)
    {
        if (ast->children[index]->kind != kind)
            spdlog::error(
                "{}: Expected child of kind '{}' but got '{}'",
                m_syntacticAnalyzer.getCurrentFilePath(),
                lang::to_string(kind),
                lang::to_string(ast->children[index]->kind));

        return ast->children[index];
    }

    void SemanticAnalyzer::semanticChecks() {}
} // namespace lang
