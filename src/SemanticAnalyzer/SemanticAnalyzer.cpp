#include "SemanticAnalyzer.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
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

    SymbolTableNode *SemanticAnalyzer::makeSymbol(SymbolTableNode::Kind kind, const std::string &name, SymbolTableNode *parent) const
    {
        auto *node = new SymbolTableNode();
        node->kind = kind;
        node->name = name;
        node->parent = parent;
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
        auto dimList = varDecl->children[2];
        for (auto &dim : dimList->children) type += std::format("[{}]", dim->lexeme);

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

            auto *classSymbol = makeSymbol(SymbolTableNode::Kind::Class, className, globalTable);
            globalTable->table.emplace_back(classSymbol);

            std::shared_ptr<const ASTNode> inheritList;
            for (const auto &child : classNode->children) {
                if (child->kind == ASTNode::Kind::InheritList) {
                    inheritList = child;
                    break;
                }
            }

            auto *inheritSymbol = makeSymbol(SymbolTableNode::Kind::Inherit, joinInheritedTypes(inheritList), classSymbol);
            classSymbol->table.emplace_back(inheritSymbol);

            for (const auto &child : classNode->children) {
                if (!child)
                    continue;

                const SymbolTableNode::Visibility visibility = SymbolTableNode::FromString(child->lexeme);

                switch (child->kind) {
                    case ASTNode::Kind::VarDecl:
                        {
                            auto *dataSymbol = makeSymbol(SymbolTableNode::Kind::Data, child->children[1]->lexeme, classSymbol);
                            dataSymbol->signature.type = varDeclType(child);
                            dataSymbol->visibility = visibility;
                            classSymbol->table.emplace_back(dataSymbol);
                            break;
                        }
                    case ASTNode::Kind::FuncDecl:
                        {
                            auto *functionSymbol = makeSymbol(SymbolTableNode::Kind::Function, child->children[1]->lexeme, classSymbol);
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

            auto *paramSymbol = makeSymbol(SymbolTableNode::Kind::Parameter, param->children[1]->lexeme, function);
            paramSymbol->signature.type = varDeclType(param);
            function->table.emplace_back(paramSymbol);
        }

        for (const auto &statement : statBlock->children) {
            if (statement->kind != ASTNode::Kind::VarDecl)
                continue;

            auto *localSymbol = makeSymbol(SymbolTableNode::Kind::Local, statement->children[1]->lexeme, function);
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

                if (!functionSymbol) {
                    if (classSymbol) {
                        functionSymbol = makeSymbol(SymbolTableNode::Kind::Function, functionName, classSymbol);
                        functionSymbol->signature.type = returnType;
                        functionSymbol->signature.params = params;
                        classSymbol->table.emplace_back(functionSymbol);
                    }
                }

                populateFunctionTable(functionSymbol, paramList, statBlock);
                continue;
            }

            if (nameNode->kind == ASTNode::Kind::Id) {
                auto *functionSymbol = makeSymbol(SymbolTableNode::Kind::Function, nameNode->lexeme, globalTable);
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
        mainSymbol->signature.type = "";
        globalTable->table.emplace_back(mainSymbol);

        for (const auto &statement : programBlock->children) {
            if (statement->kind != ASTNode::Kind::VarDecl)
                continue;

            auto *localSymbol = makeSymbol(SymbolTableNode::Kind::Local, statement->children[1]->lexeme, mainSymbol);
            localSymbol->signature.type = varDeclType(statement);
            mainSymbol->table.emplace_back(localSymbol);
        }
    }

    SymbolTableNode *SemanticAnalyzer::generateSymbolTable(std::shared_ptr<const ASTNode> ast)
    {
        if (!ast || ast->kind != ASTNode::Kind::Prog || ast->children.size() < 3)
            return nullptr;

        auto *globalTable = makeSymbol(SymbolTableNode::Kind::Table, "global", nullptr);

        const auto &classList = ast->children[0];
        const auto &funcDefList = ast->children[1];
        const auto &programBlock = ast->children[2];

        buildClassTables(globalTable, classList);
        buildFunctionDefinitions(globalTable, funcDefList);
        buildMainFunction(globalTable, programBlock);

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

    void SemanticAnalyzer::renderRow(SymbolTableNode *node, tabulate::Table *table) const
    {
        std::vector<std::string> firstRow;

        if (node->kind == SymbolTableNode::Kind::Table) {
            firstRow.emplace_back(std::format("table: {}", node->name));
        } else {
            firstRow.emplace_back(SymbolTableNode::ToString(node->kind));
            firstRow.emplace_back(node->name);

            if (node->kind == SymbolTableNode::Kind::Function) {
                std::string params;
                for (auto &param : node->signature.params) {
                    params += param;
                    if (&param != &node->signature.params.back()) {
                        params += ", ";
                    }
                }
                std::string signature = std::format("({}): {}", params, node->signature.type);
                firstRow.emplace_back(signature);
            } else if (!node->signature.type.empty()) {
                firstRow.emplace_back(node->signature.type);
            }

            if (node->visibility != SymbolTableNode::Visibility::None) {
                firstRow.emplace_back(SymbolTableNode::ToString(node->visibility));
            }
        }

        table->add_row(tabulate::Table::Row_t(firstRow.begin(), firstRow.end()));
    }

    tabulate::Table SemanticAnalyzer::renderTable(SymbolTableNode *node) const
    {
        tabulate::Table table;

        table.add_row({ "table: global" });

        for (auto *node : node->table) {
            tabulate::Table inner;
            renderRow(node, &inner);
            table.add_row({ inner });
        }

        return table;
    }

    std::string SemanticAnalyzer::renderSymbolTable() const
    {
        if (!m_symbolTable)
            return "";

        tabulate::Table table = renderTable(m_symbolTable);

        return table.str() + "\n";
    }

    void SemanticAnalyzer::semanticChecks() {}
} // namespace lang
