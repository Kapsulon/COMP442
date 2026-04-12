#include "SemanticAnalyzer.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <vector>

#include "tabulate/cell.hpp"
#include "tabulate/font_align.hpp"
#include "tabulate/table.hpp"
#include "utils/colors.hpp"

namespace lang
{
    static void deleteSymbolTree(SymbolTableNode *node)
    {
        if (!node)
            return;
        for (auto *child : node->table) deleteSymbolTree(child);
        delete node;
    }

    SemanticAnalyzer::~SemanticAnalyzer()
    {
        deleteSymbolTree(m_symbolTable);
    }

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

        m_problems.clear();
        m_classTypeNames.clear();
        m_ast = ast;

        deleteSymbolTree(m_symbolTable);
        m_symbolTable = nullptr;

        auto start = std::chrono::high_resolution_clock::now();

        m_symbolTable = generateSymbolTable(ast);

        if (!m_symbolTable) {
            spdlog::error("{}: Failed to generate symbol table", m_syntacticAnalyzer.getCurrentFilePath());
            return;
        }

        semanticChecks();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        spdlog::info(
            "{}: Generated Symbol Table in \t {:.2f}ms \t [" CYAN "{} info(s)" RESET ", " YELLOW "{} warning(s)" RESET ", " RED "{} error(s)" RESET "]\n",
            m_syntacticAnalyzer.getCurrentFilePath(),
            elapsed.count(),
            m_problems.getInfoCount(),
            m_problems.getWarningCount(),
            m_problems.getErrorCount());

        m_problems.displayProblems(m_syntacticAnalyzer.getLexer());
    }

    void SemanticAnalyzer::outputSymbolTable() const
    {
        if (!m_symbolTable)
            return;

        const std::string outputPath = std::string(m_syntacticAnalyzer.getCurrentFilePath()) + ".outsymboltables";
        std::ofstream out(outputPath);

        if (!out.is_open()) {
            spdlog::error("Failed to open output file for symbol tables: {}", outputPath);
            return;
        }

        out << renderSymbolTable();
    }

    void SemanticAnalyzer::outputSemanticErrors() const
    {
        if (m_problems.getProblemCount() == 0)
            return;

        const std::string outputPath = std::string(m_syntacticAnalyzer.getCurrentFilePath()) + ".outsemanticerrors";
        m_problems.outputProblems(m_syntacticAnalyzer.getLexer(), outputPath);
    }

    const Problems &SemanticAnalyzer::getSemanticProblems() const
    {
        return m_problems;
    }

    const SyntacticAnalyzer &SemanticAnalyzer::getSyntacticAnalyzer() const
    {
        return m_syntacticAnalyzer;
    }

    SymbolTableNode *SemanticAnalyzer::makeSymbol(SymbolTableNode::Kind kind, const std::string &name, SymbolTableNode *parent, Token token) const
    {
        auto *node = new SymbolTableNode();
        node->kind = kind;
        node->name = name;
        node->parent = parent;
        node->token = std::move(token);
        return node;
    }

    SymbolTableNode *SemanticAnalyzer::findClassSymbol(SymbolTableNode *globalTable, const std::string &className) const
    {
        if (!globalTable)
            return nullptr;
        for (auto *child : globalTable->table) {
            if (child->kind == SymbolTableNode::Kind::Class && child->name == className)
                return child;
        }
        return nullptr;
    }

    SymbolTableNode *SemanticAnalyzer::findMemberFunctionSymbol(
        SymbolTableNode *classNode, const std::string &functionName, const std::vector<std::string> &paramTypes, const std::string &returnType) const
    {
        if (!classNode)
            return nullptr;
        for (auto *entry : classNode->table) {
            if (entry->kind != SymbolTableNode::Kind::Function)
                continue;
            if (entry->name != functionName)
                continue;
            if (entry->signature.params != paramTypes)
                continue;
            if (entry->signature.type != returnType)
                continue;
            return entry;
        }
        return nullptr;
    }

    std::string SemanticAnalyzer::lowercase(std::string src) const
    {
        for (char &ch : src) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return src;
    }

    std::string SemanticAnalyzer::normalizeType(std::string typeName) const
    {
        if (typeName.empty())
            return typeName;
        const std::string key = lowercase(typeName);
        if (key == "integer")
            return "int";
        if (key == "float")
            return "float";
        if (key == "void")
            return "void";
        if (m_classTypeNames.contains(key))
            return m_classTypeNames.at(key);
        return typeName;
    }

    std::string SemanticAnalyzer::varDeclType(std::shared_ptr<const ASTNode> varDecl) const
    {
        if (!varDecl || varDecl->kind != ASTNode::Kind::VarDecl || varDecl->children.size() < 3)
            return "";
        std::string type = normalizeType(varDecl->children[0]->lexeme);
        for (auto &dim : varDecl->children[2]->children) type += std::format("[{}]", dim->lexeme);
        return type;
    }

    std::vector<std::string> SemanticAnalyzer::parameterTypes(std::shared_ptr<const ASTNode> paramList) const
    {
        std::vector<std::string> params;
        if (!paramList || paramList->kind != ASTNode::Kind::ParamList)
            return params;
        for (const auto &param : paramList->children) params.emplace_back(varDeclType(param));
        return params;
    }

    std::string SemanticAnalyzer::joinInheritedTypes(std::shared_ptr<const ASTNode> inheritList) const
    {
        if (!inheritList || inheritList->kind != ASTNode::Kind::InheritList || inheritList->children.empty())
            return "none";
        std::string joined;
        for (const auto &child : inheritList->children) {
            if (child->kind != ASTNode::Kind::Id)
                continue;
            if (!joined.empty())
                joined += ", ";
            joined += normalizeType(child->lexeme);
        }
        return joined.empty() ? "none" : joined;
    }

    void SemanticAnalyzer::buildClassTables(SymbolTableNode *globalTable, std::shared_ptr<const ASTNode> classList)
    {
        if (!globalTable || !classList || classList->kind != ASTNode::Kind::ClassList)
            return;

        for (const auto &classNode : classList->children) {
            if (!classNode || classNode->kind != ASTNode::Kind::Class || classNode->children.empty())
                continue;

            const std::string className = classNode->children[0]->lexeme;
            m_classTypeNames[lowercase(className)] = className;

            auto *classSymbol = makeSymbol(SymbolTableNode::Kind::Class, className, globalTable, classNode->children[0]->token);
            globalTable->table.emplace_back(classSymbol);

            std::shared_ptr<const ASTNode> inheritList;
            for (const auto &child : classNode->children) {
                if (child->kind == ASTNode::Kind::InheritList) {
                    inheritList = child;
                    break;
                }
            }

            classSymbol->table.emplace_back(makeSymbol(SymbolTableNode::Kind::Inherit, joinInheritedTypes(inheritList), classSymbol));

            for (const auto &child : classNode->children) {
                if (!child)
                    continue;
                const SymbolTableNode::Visibility visibility = SymbolTableNode::FromString(child->lexeme);
                switch (child->kind) {
                    case ASTNode::Kind::VarDecl:
                        {
                            auto *dataSymbol = makeSymbol(SymbolTableNode::Kind::Data, child->children[1]->lexeme, classSymbol, child->children[1]->token);
                            dataSymbol->signature.type = varDeclType(child);
                            dataSymbol->visibility = visibility;
                            classSymbol->table.emplace_back(dataSymbol);
                            break;
                        }
                    case ASTNode::Kind::FuncDecl:
                        {
                            auto *functionSymbol =
                                makeSymbol(SymbolTableNode::Kind::Function, child->children[1]->lexeme, classSymbol, child->children[1]->token);
                            functionSymbol->signature.type = normalizeType(child->children[0]->lexeme);
                            functionSymbol->signature.params = parameterTypes(child->children[2]);
                            functionSymbol->visibility = visibility;
                            classSymbol->table.emplace_back(functionSymbol);
                            break;
                        }
                    default:
                        break;
                }
            }
        }
    }

    void SemanticAnalyzer::populateFunctionTable(SymbolTableNode *function, std::shared_ptr<const ASTNode> paramList, std::shared_ptr<const ASTNode> statBlock)
    {
        if (!function)
            return;
        for (const auto *entry : function->table) {
            if (entry->kind == SymbolTableNode::Kind::Parameter || entry->kind == SymbolTableNode::Kind::Local)
                return;
        }
        for (const auto &param : paramList->children) {
            if (param->kind != ASTNode::Kind::VarDecl)
                continue;
            auto *paramSymbol = makeSymbol(SymbolTableNode::Kind::Parameter, param->children[1]->lexeme, function, param->children[1]->token);
            paramSymbol->signature.type = varDeclType(param);
            function->table.emplace_back(paramSymbol);
        }
        for (const auto &statement : statBlock->children) {
            if (statement->kind != ASTNode::Kind::VarDecl)
                continue;
            auto *localSymbol = makeSymbol(SymbolTableNode::Kind::Local, statement->children[1]->lexeme, function, statement->children[1]->token);
            localSymbol->signature.type = varDeclType(statement);
            function->table.emplace_back(localSymbol);
        }
    }

    void SemanticAnalyzer::buildFunctionDefinitions(SymbolTableNode *globalTable, std::shared_ptr<const ASTNode> funcDefList)
    {
        if (!globalTable || !funcDefList || funcDefList->kind != ASTNode::Kind::FuncDefList)
            return;

        for (const auto &funcDef : funcDefList->children) {
            if (!funcDef || funcDef->kind != ASTNode::Kind::FuncDef || funcDef->children.size() < 4)
                continue;

            const auto &returnTypeNode = funcDef->children[0];
            const auto &nameNode = funcDef->children[1];
            const auto &paramList = funcDef->children[2];
            const auto &statBlock = funcDef->children[3];

            const std::string returnType = normalizeType(returnTypeNode->lexeme);
            const std::vector<std::string> params = parameterTypes(paramList);

            if (nameNode->kind == ASTNode::Kind::MemberAccess && nameNode->children.size() == 2) {
                const std::string className = normalizeType(nameNode->children[0]->lexeme);
                const std::string functionName = nameNode->children[1]->lexeme;

                auto *classSymbol = findClassSymbol(globalTable, className);
                auto *functionSymbol = findMemberFunctionSymbol(classSymbol, functionName, params, returnType);

                if (!functionSymbol && classSymbol) {
                    functionSymbol = makeSymbol(SymbolTableNode::Kind::Function, functionName, classSymbol, nameNode->children[1]->token);
                    functionSymbol->signature.type = returnType;
                    functionSymbol->signature.params = params;
                    classSymbol->table.emplace_back(functionSymbol);
                }

                populateFunctionTable(functionSymbol, paramList, statBlock);
                continue;
            }

            if (nameNode->kind == ASTNode::Kind::Id) {
                auto *functionSymbol = makeSymbol(SymbolTableNode::Kind::Function, nameNode->lexeme, globalTable, nameNode->token);
                functionSymbol->signature.type = returnType;
                functionSymbol->signature.params = params;
                globalTable->table.emplace_back(functionSymbol);
                populateFunctionTable(functionSymbol, paramList, statBlock);
            }
        }
    }

    void SemanticAnalyzer::buildMainFunction(SymbolTableNode *globalTable, std::shared_ptr<const ASTNode> programBlock)
    {
        if (!globalTable || !programBlock || programBlock->kind != ASTNode::Kind::ProgramBlock)
            return;

        auto *mainSymbol = makeSymbol(SymbolTableNode::Kind::Function, "main", globalTable);
        mainSymbol->signature.type = "void";
        globalTable->table.emplace_back(mainSymbol);

        for (const auto &statement : programBlock->children) {
            if (statement->kind != ASTNode::Kind::VarDecl)
                continue;
            auto *localSymbol = makeSymbol(SymbolTableNode::Kind::Local, statement->children[1]->lexeme, mainSymbol, statement->children[1]->token);
            localSymbol->signature.type = varDeclType(statement);
            mainSymbol->table.emplace_back(localSymbol);
        }
    }

    SymbolTableNode *SemanticAnalyzer::generateSymbolTable(std::shared_ptr<const ASTNode> ast)
    {
        if (!ast || ast->kind != ASTNode::Kind::Prog || ast->children.size() < 3)
            return nullptr;

        auto *globalTable = makeSymbol(SymbolTableNode::Kind::Table, "global", nullptr);

        buildClassTables(globalTable, ast->children[0]);
        buildFunctionDefinitions(globalTable, ast->children[1]);
        buildMainFunction(globalTable, ast->children[2]);

        return globalTable;
    }

    std::string SemanticAnalyzer::GetFullNamespace(const SymbolTableNode *node)
    {
        if (!node)
            return "";
        if (node->kind == SymbolTableNode::Kind::Table)
            return node->name;
        if (node->kind == SymbolTableNode::Kind::Class && node->parent && node->parent->kind == SymbolTableNode::Kind::Table)
            return node->name;
        if (!node->parent || node->parent->kind == SymbolTableNode::Kind::Table)
            return std::format("::{}", node->name);
        return std::format("{}::{}", node->parent->name, node->name);
    }

    tabulate::Table::Row_t SemanticAnalyzer::renderRow(const SymbolTableNode *node) const
    {
        tabulate::Table::Row_t row;
        row.emplace_back(SymbolTableNode::ToString(node->kind));
        row.emplace_back(node->name);

        if (node->kind == SymbolTableNode::Kind::Function) {
            std::string params;
            for (const auto &param : node->signature.params) {
                if (!params.empty())
                    params += ",";
                params += param;
            }
            row.emplace_back(std::format("({}):{}", params, node->signature.type));
        } else if (!node->signature.type.empty() || node->kind == SymbolTableNode::Kind::Inherit) {
            row.emplace_back(node->signature.type);
        }

        if (node->visibility != SymbolTableNode::Visibility::None)
            row.emplace_back(SymbolTableNode::ToString(node->visibility));

        return row;
    }

    tabulate::Table SemanticAnalyzer::renderTable(const SymbolTableNode *node) const
    {
        tabulate::Table table;
        table.add_row({ std::format("table: {}", GetFullNamespace(node)) });

        tabulate::Table flatRows;
        auto flushFlatRows = [&table, &flatRows]() {
            if (flatRows.size() == 0)
                return;
            table.add_row({ flatRows });
            flatRows = tabulate::Table();
        };

        for (const auto *child : node->table) {
            if (child->table.empty()) {
                auto row = renderRow(child);
                while (row.size() < 4) row.emplace_back("");
                flatRows.add_row(row);
                continue;
            }
            flushFlatRows();
            tabulate::Table childRow;
            childRow.add_row(renderRow(child));
            table.add_row({ childRow });
            tabulate::Table nestedTable = renderTable(child);
            table.add_row({ nestedTable });
            table[table.size() - 1].format().hide_border_top();
        }

        flushFlatRows();
        return table;
    }

    std::string SemanticAnalyzer::renderSymbolTable() const
    {
        if (!m_symbolTable)
            return "";
        return renderTable(m_symbolTable).str() + "\n";
    }

} // namespace lang
