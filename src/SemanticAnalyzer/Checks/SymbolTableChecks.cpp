#include "../SemanticAnalyzer.hpp"

#include <format>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

namespace lang
{
    void SemanticAnalyzer::semanticChecks()
    {
        if (!m_symbolTable)
            return;

        checkMultiplyDeclared(m_symbolTable);
        checkFunctionDeclarationDefinitionParity(m_symbolTable);
        checkOverloadsAndOverrides(m_symbolTable);
        checkShadowing(m_symbolTable);
        checkCircularDependencies(m_symbolTable);
        checkUndeclaredClassTypes(m_symbolTable);

        checkAllFunctionBodies(m_symbolTable);
    }

    void SemanticAnalyzer::checkMultiplyDeclared(SymbolTableNode *globalTable)
    {
        std::unordered_map<std::string, SymbolTableNode *> seenClasses;
        for (auto *entry : globalTable->table) {
            if (entry->kind != SymbolTableNode::Kind::Class)
                continue;
            const std::string key = lowercase(entry->name);
            if (seenClasses.count(key))
                m_problems.error("8.1 multiply declared class", std::format("class '{}' was already declared", entry->name), { entry->token });
            else
                seenClasses[key] = entry;
        }

        {
            std::unordered_map<std::string, std::vector<SymbolTableNode *>> freeFuncs;
            for (auto *entry : globalTable->table) {
                if (entry->kind == SymbolTableNode::Kind::Function)
                    freeFuncs[lowercase(entry->name)].push_back(entry);
            }
            for (auto &[name, funcs] : freeFuncs) {
                for (size_t i = 0; i < funcs.size(); ++i) {
                    for (size_t j = i + 1; j < funcs.size(); ++j) {
                        if (funcs[i]->signature.params == funcs[j]->signature.params && funcs[i]->signature.type == funcs[j]->signature.type) {
                            m_problems.error(
                                "8.2 multiply declared free function",
                                std::format("free function '{}' with this signature was already declared", funcs[j]->name),
                                { funcs[j]->token });
                        }
                    }
                }
            }
        }

        for (auto *entry : globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Class) {
                std::unordered_map<std::string, SymbolTableNode *> seenData;
                for (auto *member : entry->table) {
                    if (member->kind != SymbolTableNode::Kind::Data)
                        continue;
                    const std::string key = lowercase(member->name);
                    if (seenData.count(key))
                        m_problems.error(
                            "8.3 multiply declared data member in class",
                            std::format("data member '{}' in class '{}' was already declared", member->name, entry->name),
                            { member->token });
                    else
                        seenData[key] = member;
                }
                for (auto *member : entry->table) {
                    if (member->kind == SymbolTableNode::Kind::Function)
                        checkMultiplyDeclaredInFunction(member);
                }
            }
            if (entry->kind == SymbolTableNode::Kind::Function)
                checkMultiplyDeclaredInFunction(entry);
        }
    }

    void SemanticAnalyzer::checkMultiplyDeclaredInFunction(SymbolTableNode *funcNode)
    {
        std::unordered_map<std::string, SymbolTableNode *> seen;
        for (auto *entry : funcNode->table) {
            if (entry->kind != SymbolTableNode::Kind::Parameter && entry->kind != SymbolTableNode::Kind::Local)
                continue;
            const std::string key = lowercase(entry->name);
            if (seen.count(key))
                m_problems.error(
                    "8.4 multiply declared variable in function",
                    std::format("variable '{}' in function '{}' was already declared", entry->name, funcNode->name),
                    { entry->token });
            else
                seen[key] = entry;
        }
    }

    void SemanticAnalyzer::checkFunctionDeclarationDefinitionParity(SymbolTableNode *globalTable)
    {
        if (!m_ast || m_ast->children.size() < 2)
            return;

        const auto &funcDefList = m_ast->children[1];
        if (!funcDefList || funcDefList->kind != ASTNode::Kind::FuncDefList)
            return;

        using FuncKey = std::tuple<std::string, std::string, std::vector<std::string>, std::string>;
        struct FuncKeyHash {
            size_t operator()(const FuncKey &k) const
            {
                size_t h = std::hash<std::string>{}(std::get<0>(k));
                h ^= std::hash<std::string>{}(std::get<1>(k)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                for (const auto &p : std::get<2>(k)) h ^= std::hash<std::string>{}(p) + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<std::string>{}(std::get<3>(k)) + 0x9e3779b9 + (h << 6) + (h >> 2);
                return h;
            }
        };

        std::unordered_map<FuncKey, std::shared_ptr<const ASTNode>, FuncKeyHash> definitions;

        for (const auto &funcDef : funcDefList->children) {
            if (!funcDef || funcDef->kind != ASTNode::Kind::FuncDef || funcDef->children.size() < 4)
                continue;
            const auto &nameNode = funcDef->children[1];
            if (nameNode->kind != ASTNode::Kind::MemberAccess || nameNode->children.size() < 2)
                continue;
            const std::string className = normalizeType(nameNode->children[0]->lexeme);
            const std::string funcName = nameNode->children[1]->lexeme;
            const std::string retType = normalizeType(funcDef->children[0]->lexeme);
            const std::vector<std::string> params = parameterTypes(funcDef->children[2]);
            definitions[{ lowercase(className), lowercase(funcName), params, retType }] = funcDef;
        }

        for (auto *classEntry : globalTable->table) {
            if (classEntry->kind != SymbolTableNode::Kind::Class)
                continue;
            for (auto *funcEntry : classEntry->table) {
                if (funcEntry->kind != SymbolTableNode::Kind::Function)
                    continue;
                FuncKey key{ lowercase(classEntry->name), lowercase(funcEntry->name), funcEntry->signature.params, funcEntry->signature.type };
                if (!definitions.count(key))
                    m_problems.error(
                        "6.2 undefined member function declaration",
                        std::format("member function '{}::{}' is declared but has no definition", classEntry->name, funcEntry->name),
                        { funcEntry->token });
            }
        }

        const auto &classList = m_ast->children[0];
        for (const auto &funcDef : funcDefList->children) {
            if (!funcDef || funcDef->kind != ASTNode::Kind::FuncDef || funcDef->children.size() < 4)
                continue;
            const auto &nameNode = funcDef->children[1];
            if (nameNode->kind != ASTNode::Kind::MemberAccess || nameNode->children.size() < 2)
                continue;
            const std::string className = normalizeType(nameNode->children[0]->lexeme);
            const std::string funcName = nameNode->children[1]->lexeme;
            const std::string retType = normalizeType(funcDef->children[0]->lexeme);
            const std::vector<std::string> params = parameterTypes(funcDef->children[2]);

            auto *classSymbol = findClassSymbol(globalTable, className);
            if (!classSymbol)
                continue;

            bool hasDeclAST = false;
            if (classList && classList->kind == ASTNode::Kind::ClassList) {
                for (const auto &classNode : classList->children) {
                    if (!classNode || classNode->children.empty())
                        continue;
                    if (lowercase(classNode->children[0]->lexeme) != lowercase(className))
                        continue;
                    for (const auto &member : classNode->children) {
                        if (member->kind != ASTNode::Kind::FuncDecl || member->children.size() < 3)
                            continue;
                        if (lowercase(member->children[1]->lexeme) == lowercase(funcName) && normalizeType(member->children[0]->lexeme) == retType &&
                            parameterTypes(member->children[2]) == params) {
                            hasDeclAST = true;
                            break;
                        }
                    }
                    if (hasDeclAST)
                        break;
                }
            }

            if (!hasDeclAST)
                m_problems.error(
                    "6.1 undeclared member function definition",
                    std::format("definition provided for undeclared member function '{}::{}'", className, funcName),
                    { nameNode->children[1]->token });
        }
    }

    void SemanticAnalyzer::checkOverloadsAndOverrides(SymbolTableNode *globalTable)
    {
        std::unordered_map<std::string, std::vector<SymbolTableNode *>> freeFuncsByName;
        for (auto *entry : globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Function)
                freeFuncsByName[lowercase(entry->name)].push_back(entry);
        }
        for (auto &[name, funcs] : freeFuncsByName) {
            if (funcs.size() <= 1)
                continue;
            std::set<std::vector<std::string>> seenParams;
            for (auto *f : funcs) seenParams.insert(f->signature.params);
            if (seenParams.size() <= 1)
                continue;
            for (size_t i = 1; i < funcs.size(); ++i) {
                bool isExact = false;
                for (size_t j = 0; j < i; ++j) {
                    if (funcs[j]->signature.params == funcs[i]->signature.params && funcs[j]->signature.type == funcs[i]->signature.type) {
                        isExact = true;
                        break;
                    }
                }
                if (!isExact)
                    m_problems.warn("9.1 overloaded free function", std::format("free function '{}' is overloaded", funcs[i]->name), { funcs[i]->token });
            }
        }

        for (auto *classEntry : globalTable->table) {
            if (classEntry->kind != SymbolTableNode::Kind::Class)
                continue;

            std::unordered_map<std::string, std::vector<SymbolTableNode *>> memberFuncsByName;
            for (auto *member : classEntry->table) {
                if (member->kind == SymbolTableNode::Kind::Function)
                    memberFuncsByName[lowercase(member->name)].push_back(member);
            }
            for (auto &[name, funcs] : memberFuncsByName) {
                if (funcs.size() <= 1)
                    continue;
                std::set<std::vector<std::string>> seenParams;
                for (auto *f : funcs) seenParams.insert(f->signature.params);
                if (seenParams.size() <= 1)
                    continue;
                for (size_t i = 1; i < funcs.size(); ++i) {
                    bool isExact = false;
                    for (size_t j = 0; j < i; ++j) {
                        if (funcs[j]->signature.params == funcs[i]->signature.params && funcs[j]->signature.type == funcs[i]->signature.type) {
                            isExact = true;
                            break;
                        }
                    }
                    if (!isExact)
                        m_problems.warn(
                            "9.2 overloaded member function",
                            std::format("member function '{}::{}' is overloaded", classEntry->name, funcs[i]->name),
                            { funcs[i]->token });
                }
            }

            for (auto *parentClass : collectInheritedClasses(classEntry)) {
                for (auto *member : classEntry->table) {
                    if (member->kind != SymbolTableNode::Kind::Function)
                        continue;
                    for (auto *parentMember : parentClass->table) {
                        if (parentMember->kind == SymbolTableNode::Kind::Function && lowercase(member->name) == lowercase(parentMember->name) &&
                            member->signature.params == parentMember->signature.params) {
                            m_problems.warn(
                                "9.3 overridden member function",
                                std::format(
                                    "member function '{}::{}' overrides '{}::{}'", classEntry->name, member->name, parentClass->name, parentMember->name),
                                { member->token });
                        }
                    }
                }
            }
        }
    }

    void SemanticAnalyzer::checkShadowing(SymbolTableNode *globalTable)
    {
        for (auto *classEntry : globalTable->table) {
            if (classEntry->kind != SymbolTableNode::Kind::Class)
                continue;

            auto parents = collectInheritedClasses(classEntry);

            for (auto *member : classEntry->table) {
                if (member->kind != SymbolTableNode::Kind::Data)
                    continue;
                for (auto *parentClass : parents) {
                    for (auto *parentMember : parentClass->table) {
                        if (parentMember->kind == SymbolTableNode::Kind::Data && lowercase(parentMember->name) == lowercase(member->name)) {
                            m_problems.warn(
                                "8.5 shadowed inherited data member",
                                std::format(
                                    "data member '{}' in class '{}' shadows inherited member from '{}'", member->name, classEntry->name, parentClass->name),
                                { member->token });
                        }
                    }
                }
            }

            for (auto *funcEntry : classEntry->table) {
                if (funcEntry->kind != SymbolTableNode::Kind::Function)
                    continue;
                for (auto *local : funcEntry->table) {
                    if (local->kind != SymbolTableNode::Kind::Local)
                        continue;
                    for (auto *dataMember : classEntry->table) {
                        if (dataMember->kind == SymbolTableNode::Kind::Data && lowercase(dataMember->name) == lowercase(local->name)) {
                            m_problems.warn(
                                "8.6 local variable shadows data member",
                                std::format(
                                    "local variable '{}' in '{}::{}' shadows a data member of class '{}'",
                                    local->name,
                                    classEntry->name,
                                    funcEntry->name,
                                    classEntry->name),
                                { local->token });
                        }
                    }
                }
            }
        }
    }

    void SemanticAnalyzer::checkCircularDependencies(SymbolTableNode *globalTable)
    {
        enum class Color {
            White,
            Gray,
            Black
        };
        std::unordered_map<std::string, Color> color;

        for (auto *entry : globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Class)
                color[lowercase(entry->name)] = Color::White;
        }

        std::function<bool(SymbolTableNode *, std::vector<std::string> &)> dfs = [&](SymbolTableNode *node, std::vector<std::string> &path) -> bool {
            const std::string key = lowercase(node->name);
            color[key] = Color::Gray;
            path.push_back(node->name);

            for (auto *parentClass : collectInheritedClasses(node)) {
                const std::string parentKey = lowercase(parentClass->name);
                if (color[parentKey] == Color::Gray) {
                    m_problems.error(
                        "14.1 circular class dependency", std::format("circular class dependency detected involving class '{}'", node->name), { node->token });
                    path.pop_back();
                    return true;
                }
                if (color[parentKey] == Color::White && dfs(parentClass, path)) {
                    path.pop_back();
                    return true;
                }
            }

            color[key] = Color::Black;
            path.pop_back();
            return false;
        };

        for (auto *entry : globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Class && color[lowercase(entry->name)] == Color::White) {
                std::vector<std::string> path;
                dfs(entry, path);
            }
        }
    }

    void SemanticAnalyzer::checkUndeclaredClassTypes(SymbolTableNode *globalTable)
    {
        auto check = [&](SymbolTableNode *node, const std::string &typeStr) {
            const std::string base = StripAllDimensions(typeStr);
            if (base.empty() || base == "int" || base == "float" || base == "void")
                return;
            const std::string key = lowercase(base);
            if (key == "int" || key == "integer" || key == "float" || key == "void")
                return;
            if (!m_classTypeNames.count(key))
                m_problems.error("11.5 undeclared class", std::format("type '{}' is not a declared class", base), { node->token });
        };

        for (auto *entry : globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Class) {
                for (auto *member : entry->table) {
                    if (member->kind == SymbolTableNode::Kind::Data)
                        check(member, member->signature.type);
                    if (member->kind == SymbolTableNode::Kind::Function) {
                        for (auto *local : member->table) {
                            if (local->kind == SymbolTableNode::Kind::Parameter || local->kind == SymbolTableNode::Kind::Local)
                                check(local, local->signature.type);
                        }
                    }
                }
            }
            if (entry->kind == SymbolTableNode::Kind::Function) {
                for (auto *local : entry->table) {
                    if (local->kind == SymbolTableNode::Kind::Parameter || local->kind == SymbolTableNode::Kind::Local)
                        check(local, local->signature.type);
                }
            }
        }
    }

} // namespace lang
