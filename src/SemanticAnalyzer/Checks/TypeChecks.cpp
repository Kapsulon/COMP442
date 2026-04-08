#include "../SemanticAnalyzer.hpp"

#include <format>
#include <vector>

namespace lang
{
    int SemanticAnalyzer::CountArrayDimensions(const std::string &type)
    {
        int count = 0;
        for (char c : type) {
            if (c == '[')
                ++count;
        }
        return count;
    }

    std::string SemanticAnalyzer::StripAllDimensions(const std::string &type)
    {
        const size_t pos = type.find('[');
        return pos == std::string::npos ? type : type.substr(0, pos);
    }

    std::string SemanticAnalyzer::StripOneDimension(const std::string &type)
    {
        const size_t pos = type.find('[');
        if (pos == std::string::npos)
            return type;
        const size_t end = type.find(']', pos);
        return end == std::string::npos ? type : type.substr(0, pos) + type.substr(end + 1);
    }

    bool SemanticAnalyzer::TypesCompatible(const std::string &a, const std::string &b)
    {
        if (a.empty() || b.empty())
            return true;
        if (a == b)
            return true;
        if (a == "int" && b == "float")
            return true;
        return false;
    }

    std::vector<SymbolTableNode *> SemanticAnalyzer::collectInheritedClasses(SymbolTableNode *classNode) const
    {
        std::vector<SymbolTableNode *> result;
        if (!classNode)
            return result;

        for (const auto *entry : classNode->table) {
            if (entry->kind != SymbolTableNode::Kind::Inherit)
                continue;
            if (entry->name == "none" || entry->name.empty())
                break;

            std::string names = entry->name;
            size_t pos = 0;
            while (pos < names.size()) {
                size_t comma = names.find(',', pos);
                std::string parentName = (comma == std::string::npos) ? names.substr(pos) : names.substr(pos, comma - pos);
                while (!parentName.empty() && parentName.front() == ' ') parentName.erase(0, 1);
                while (!parentName.empty() && parentName.back() == ' ') parentName.pop_back();
                if (!parentName.empty()) {
                    SymbolTableNode *parentClass = findClassByName(parentName);
                    if (parentClass)
                        result.push_back(parentClass);
                }
                if (comma == std::string::npos)
                    break;
                pos = comma + 1;
            }
            break;
        }
        return result;
    }

    SymbolTableNode *SemanticAnalyzer::findClassByName(const std::string &name) const
    {
        if (!m_symbolTable)
            return nullptr;
        const std::string key = lowercase(name);
        for (auto *entry : m_symbolTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Class && lowercase(entry->name) == key)
                return entry;
        }
        return nullptr;
    }

    SymbolTableNode *SemanticAnalyzer::findFreeFunctionByName(const std::string &name) const
    {
        if (!m_symbolTable)
            return nullptr;
        const std::string key = lowercase(name);
        for (auto *entry : m_symbolTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Function && lowercase(entry->name) == key)
                return entry;
        }
        return nullptr;
    }

    SymbolTableNode *SemanticAnalyzer::findFreeFunctionByNameAndArgs(const std::string &name, const std::vector<std::string> &argTypes) const
    {
        if (!m_symbolTable)
            return nullptr;
        const std::string key = lowercase(name);
        SymbolTableNode *bestMatch = nullptr;
        for (auto *entry : m_symbolTable->table) {
            if (entry->kind != SymbolTableNode::Kind::Function || lowercase(entry->name) != key)
                continue;
            if (entry->signature.params.size() == argTypes.size()) {
                bool allMatch = true;
                for (size_t i = 0; i < argTypes.size(); ++i) {
                    if (!argTypes[i].empty() && !entry->signature.params[i].empty() && argTypes[i] != entry->signature.params[i]) {
                        allMatch = false;
                        break;
                    }
                }
                if (allMatch)
                    return entry;
                if (!bestMatch)
                    bestMatch = entry;
            }
            if (!bestMatch)
                bestMatch = entry;
        }
        return bestMatch;
    }

    SymbolTableNode *SemanticAnalyzer::lookupInScope(const std::string &name, const ScopeContext &ctx) const
    {
        const std::string key = lowercase(name);

        if (ctx.function_node) {
            for (auto *entry : ctx.function_node->table) {
                if ((entry->kind == SymbolTableNode::Kind::Local || entry->kind == SymbolTableNode::Kind::Parameter) && lowercase(entry->name) == key)
                    return entry;
            }
        }

        if (ctx.class_node) {
            auto *found = lookupInClass(ctx.class_node, name);
            if (found)
                return found;
        }

        if (ctx.global_table) {
            for (auto *entry : ctx.global_table->table) {
                if (entry->kind == SymbolTableNode::Kind::Function && lowercase(entry->name) == key)
                    return entry;
            }
        }

        return nullptr;
    }

    SymbolTableNode *SemanticAnalyzer::lookupInClass(SymbolTableNode *classNode, const std::string &name) const
    {
        if (!classNode)
            return nullptr;
        const std::string key = lowercase(name);

        for (auto *entry : classNode->table) {
            if ((entry->kind == SymbolTableNode::Kind::Data || entry->kind == SymbolTableNode::Kind::Function) && lowercase(entry->name) == key)
                return entry;
        }

        for (auto *parentClass : collectInheritedClasses(classNode)) {
            auto *found = lookupInClass(parentClass, name);
            if (found)
                return found;
        }

        return nullptr;
    }

    void SemanticAnalyzer::checkAllFunctionBodies(SymbolTableNode *globalTable)
    {
        if (!m_ast || m_ast->children.size() < 3)
            return;

        const auto &funcDefList = m_ast->children[1];
        const auto &programBlock = m_ast->children[2];

        if (funcDefList && funcDefList->kind == ASTNode::Kind::FuncDefList) {
            for (const auto &funcDef : funcDefList->children) {
                if (!funcDef || funcDef->kind != ASTNode::Kind::FuncDef || funcDef->children.size() < 4)
                    continue;

                const auto &nameNode = funcDef->children[1];
                const auto &paramListNode = funcDef->children[2];
                const auto &statBlockNode = funcDef->children[3];
                const auto &retTypeNode = funcDef->children[0];

                const std::string retType = normalizeType(retTypeNode->lexeme);
                const std::vector<std::string> params = parameterTypes(paramListNode);

                ScopeContext ctx;
                ctx.global_table = globalTable;

                if (nameNode->kind == ASTNode::Kind::MemberAccess && nameNode->children.size() == 2) {
                    const std::string className = normalizeType(nameNode->children[0]->lexeme);
                    const std::string funcName = nameNode->children[1]->lexeme;
                    ctx.class_node = findClassByName(className);
                    if (ctx.class_node)
                        ctx.function_node = findMemberFunctionSymbol(ctx.class_node, funcName, params, retType);
                } else if (nameNode->kind == ASTNode::Kind::Id) {
                    ctx.function_node = findFreeFunctionByNameAndArgs(nameNode->lexeme, params);
                }

                checkStatBlock(statBlockNode, ctx);
            }
        }

        {
            ScopeContext ctx;
            ctx.global_table = globalTable;
            for (auto *entry : globalTable->table) {
                if (entry->kind == SymbolTableNode::Kind::Function && lowercase(entry->name) == "main") {
                    ctx.function_node = entry;
                    break;
                }
            }
            checkStatBlock(programBlock, ctx);
        }
    }

    void SemanticAnalyzer::checkStatBlock(std::shared_ptr<const ASTNode> statBlock, const ScopeContext &ctx)
    {
        if (!statBlock)
            return;
        for (const auto &stmt : statBlock->children) checkStatement(stmt, ctx);
    }

    void SemanticAnalyzer::checkStatement(std::shared_ptr<const ASTNode> stmt, const ScopeContext &ctx)
    {
        if (!stmt)
            return;

        switch (stmt->kind) {
            case ASTNode::Kind::AssignStat:
                checkAssignStat(stmt, ctx);
                break;
            case ASTNode::Kind::ReturnStat:
                checkReturnStat(stmt, ctx);
                break;
            case ASTNode::Kind::FuncCall:
                inferType(stmt, ctx);
                break;
            case ASTNode::Kind::IfStat:
                if (stmt->children.size() >= 3) {
                    inferType(stmt->children[0], ctx);
                    checkStatBlock(stmt->children[1], ctx);
                    checkStatBlock(stmt->children[2], ctx);
                }
                break;
            case ASTNode::Kind::WhileStat:
                if (stmt->children.size() >= 2) {
                    inferType(stmt->children[0], ctx);
                    checkStatBlock(stmt->children[1], ctx);
                }
                break;
            case ASTNode::Kind::PutStat:
            case ASTNode::Kind::ReadStat:
                if (!stmt->children.empty())
                    inferType(stmt->children[0], ctx);
                break;
            case ASTNode::Kind::VarDecl:
                break;
            case ASTNode::Kind::MemberAccess:
                inferType(stmt, ctx);
                break;
            default:
                break;
        }
    }

    void SemanticAnalyzer::checkAssignStat(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx)
    {
        if (!node || node->children.size() < 2)
            return;

        const std::string lhsType = inferType(node->children[0], ctx);
        const std::string rhsType = inferType(node->children[1], ctx);

        if (!lhsType.empty() && !rhsType.empty() && lhsType != rhsType)
            m_problems.error(
                "10.2 type error in assignment statement",
                std::format("cannot assign '{}' to '{}'", rhsType, lhsType),
                { node->children[0]->token.line > 0 ? node->children[0]->token : node->children[1]->token });
    }

    void SemanticAnalyzer::checkReturnStat(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx)
    {
        if (!node || node->children.empty())
            return;

        const std::string retType = inferType(node->children[0], ctx);

        if (ctx.function_node && !ctx.function_node->signature.type.empty() && ctx.function_node->signature.type != "void" && !retType.empty()) {
            if (!TypesCompatible(retType, ctx.function_node->signature.type))
                m_problems.error(
                    "10.3 type error in return statement",
                    std::format("returning '{}' from function declared to return '{}'", retType, ctx.function_node->signature.type),
                    { node->children[0]->token.line > 0 ? node->children[0]->token : node->token });
        }
    }

    std::string SemanticAnalyzer::inferType(std::shared_ptr<const ASTNode> expr, const ScopeContext &ctx)
    {
        if (!expr)
            return "";

        switch (expr->kind) {
            case ASTNode::Kind::Num:
                return (expr->lexeme.find('.') != std::string::npos || expr->lexeme.find('e') != std::string::npos) ? "float" : "int";

            case ASTNode::Kind::Id:
                return inferTypeId(expr, ctx);

            case ASTNode::Kind::MemberAccess:
                return inferTypeMemberAccess(expr, ctx);

            case ASTNode::Kind::IndexedVar:
                return inferTypeIndexedVar(expr, ctx);

            case ASTNode::Kind::FuncCall:
                return inferTypeFuncCall(expr, ctx);

            case ASTNode::Kind::AddOp:
            case ASTNode::Kind::MultOp:
                {
                    if (expr->children.size() < 2)
                        return "";
                    const std::string leftType = inferType(expr->children[0], ctx);
                    const std::string rightType = inferType(expr->children[1], ctx);
                    if (!leftType.empty() && !rightType.empty() && leftType != rightType) {
                        m_problems.error(
                            "10.1 type error in expression",
                            std::format("operands of '{}' have incompatible types '{}' and '{}'", expr->lexeme, leftType, rightType),
                            { expr->token.line > 0 ? expr->token : expr->children[0]->token });
                        return "";
                    }
                    return leftType.empty() ? rightType : leftType;
                }

            case ASTNode::Kind::RelOp:
                {
                    if (expr->children.size() < 2)
                        return "";
                    const std::string leftType = inferType(expr->children[0], ctx);
                    const std::string rightType = inferType(expr->children[1], ctx);
                    if (!leftType.empty() && !rightType.empty() && leftType != rightType)
                        m_problems.error(
                            "10.1 type error in expression",
                            std::format("operands of '{}' have incompatible types '{}' and '{}'", expr->lexeme, leftType, rightType),
                            { expr->token.line > 0 ? expr->token : expr->children[0]->token });
                    return "int";
                }

            case ASTNode::Kind::NotExpr:
            case ASTNode::Kind::SignExpr:
                return expr->children.empty() ? "" : inferType(expr->children[0], ctx);

            default:
                return "";
        }
    }

    std::string SemanticAnalyzer::inferTypeId(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx)
    {
        auto *sym = lookupInScope(node->lexeme, ctx);
        if (!sym) {
            m_problems.error("11.1 undeclared local variable", std::format("'{}' is undeclared", node->lexeme), { node->token });
            return "";
        }
        return sym->signature.type;
    }

    std::string SemanticAnalyzer::inferTypeMemberAccess(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx)
    {
        if (!node || node->children.size() < 2)
            return "";

        const std::string objectType = inferType(node->children[0], ctx);
        if (objectType.empty())
            return "";

        const std::string baseType = StripAllDimensions(objectType);
        SymbolTableNode *classNode = findClassByName(baseType);
        if (!classNode) {
            m_problems.error(
                "15.1 \".\" operator used on non-class type",
                std::format("'.' operator used on variable of type '{}' which is not a class type", baseType),
                { node->children[0]->token.line > 0 ? node->children[0]->token : node->token });
            return "";
        }

        const auto &memberNode = node->children[1];

        if (memberNode->kind == ASTNode::Kind::FuncCall) {
            const std::string memberName = memberNode->children[0]->lexeme;
            std::vector<std::string> argTypes;
            if (memberNode->children.size() >= 2) {
                for (const auto &arg : memberNode->children[1]->children) argTypes.push_back(inferType(arg, ctx));
            }

            SymbolTableNode *funcSym = nullptr;
            for (auto *entry : classNode->table) {
                if (entry->kind != SymbolTableNode::Kind::Function || lowercase(entry->name) != lowercase(memberName))
                    continue;
                if (entry->signature.params.size() == argTypes.size()) {
                    funcSym = entry;
                    bool wrongType = false;
                    for (size_t i = 0; i < argTypes.size(); ++i) {
                        if (!argTypes[i].empty() && !entry->signature.params[i].empty() && argTypes[i] != entry->signature.params[i])
                            wrongType = true;
                    }
                    if (!wrongType)
                        break;
                } else if (!funcSym) {
                    funcSym = entry;
                }
            }
            if (!funcSym) {
                for (auto *parentClass : collectInheritedClasses(classNode)) {
                    for (auto *entry : parentClass->table) {
                        if (entry->kind == SymbolTableNode::Kind::Function && lowercase(entry->name) == lowercase(memberName)) {
                            funcSym = entry;
                            break;
                        }
                    }
                    if (funcSym)
                        break;
                }
            }

            if (!funcSym) {
                m_problems.error(
                    "11.3 undeclared member function",
                    std::format("class '{}' has no member function '{}'", classNode->name, memberName),
                    { memberNode->children[0]->token.line > 0 ? memberNode->children[0]->token : node->token });
                return "";
            }

            const Token &errTok = memberNode->children[0]->token.line > 0 ? memberNode->children[0]->token : node->token;
            if (funcSym->signature.params.size() != argTypes.size()) {
                m_problems.error(
                    "12.1 function call with wrong number of parameters",
                    std::format("'{}::{}' expects {} parameter(s), got {}", classNode->name, funcSym->name, funcSym->signature.params.size(), argTypes.size()),
                    { errTok });
            } else {
                for (size_t i = 0; i < argTypes.size(); ++i) {
                    if (!argTypes[i].empty() && !funcSym->signature.params[i].empty() && argTypes[i] != funcSym->signature.params[i])
                        m_problems.error(
                            "12.2 function call with wrong type of parameters",
                            std::format(
                                "'{}::{}' parameter {} expects '{}', got '{}'",
                                classNode->name,
                                funcSym->name,
                                i + 1,
                                funcSym->signature.params[i],
                                argTypes[i]),
                            { errTok });
                }
            }

            return funcSym->signature.type;
        }

        const std::string memberName = memberNode->lexeme;
        SymbolTableNode *memberSym = lookupInClass(classNode, memberName);
        if (!memberSym) {
            m_problems.error(
                "11.2 undeclared member variable",
                std::format("class '{}' has no data member '{}'", classNode->name, memberName),
                { memberNode->token.line > 0 ? memberNode->token : node->token });
            return "";
        }
        return memberSym->signature.type;
    }

    std::string SemanticAnalyzer::inferTypeIndexedVar(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx)
    {
        if (!node || node->children.size() < 2)
            return "";

        std::string baseType;
        if (node->children[0]->kind == ASTNode::Kind::Id) {
            auto *sym = lookupInScope(node->children[0]->lexeme, ctx);
            if (!sym) {
                m_problems.error("11.1 undeclared local variable", std::format("'{}' is undeclared", node->children[0]->lexeme), { node->children[0]->token });
                return "";
            }
            if (CountArrayDimensions(sym->signature.type) == 0) {
                const Token errToken =
                    node->children[0]->token.line > 0 ? node->children[0]->token : (node->children[1]->token.line > 0 ? node->children[1]->token : node->token);
                m_problems.error(
                    "13.1 use of array with wrong number of dimensions",
                    std::format("'{}' is not an array but is indexed", node->children[0]->lexeme),
                    { errToken });
                return StripAllDimensions(sym->signature.type);
            }
            baseType = StripOneDimension(sym->signature.type);
        } else {
            baseType = inferType(node->children[0], ctx);
            if (baseType.empty())
                return "";
            if (CountArrayDimensions(baseType) > 0) {
                baseType = StripOneDimension(baseType);
            } else {
                const Token errToken =
                    node->children[1]->token.line > 0 ? node->children[1]->token : (node->children[0]->token.line > 0 ? node->children[0]->token : node->token);
                m_problems.error(
                    "13.1 use of array with wrong number of dimensions",
                    std::format("expression of type '{}' is not an array but is indexed", StripAllDimensions(baseType)),
                    { errToken });
                baseType = StripAllDimensions(baseType);
            }
        }

        const std::string indexType = inferType(node->children[1], ctx);
        if (!indexType.empty() && indexType != "int")
            m_problems.error(
                "13.2 array index is not an integer",
                std::format("array index must be integer, got '{}'", indexType),
                { node->children[1]->token.line > 0 ? node->children[1]->token : node->token });

        return StripAllDimensions(baseType);
    }

    std::string SemanticAnalyzer::inferTypeFuncCall(std::shared_ptr<const ASTNode> node, const ScopeContext &ctx)
    {
        if (!node || node->children.size() < 2)
            return "";

        const auto &idNode = node->children[0];
        const auto &paramListNode = node->children[1];

        if (idNode->kind == ASTNode::Kind::MemberAccess) {
            if (idNode->children.size() < 2)
                return "";
            const std::string objectType = inferType(idNode->children[0], ctx);
            if (objectType.empty())
                return "";

            const std::string baseType = StripAllDimensions(objectType);
            SymbolTableNode *classNode = findClassByName(baseType);
            if (!classNode) {
                m_problems.error(
                    "15.1 \".\" operator used on non-class type",
                    std::format("'.' operator used on variable of type '{}' which is not a class type", baseType),
                    { idNode->children[0]->token.line > 0 ? idNode->children[0]->token : node->token });
                return "";
            }

            const std::string memberName = idNode->children[1]->lexeme;
            std::vector<std::string> argTypes;
            for (const auto &arg : paramListNode->children) argTypes.push_back(inferType(arg, ctx));

            SymbolTableNode *funcSym = nullptr;
            for (auto *entry : classNode->table) {
                if (entry->kind != SymbolTableNode::Kind::Function || lowercase(entry->name) != lowercase(memberName))
                    continue;
                if (entry->signature.params.size() == argTypes.size()) {
                    funcSym = entry;
                    break;
                }
                if (!funcSym)
                    funcSym = entry;
            }
            if (!funcSym) {
                for (auto *parentClass : collectInheritedClasses(classNode)) {
                    for (auto *entry : parentClass->table) {
                        if (entry->kind == SymbolTableNode::Kind::Function && lowercase(entry->name) == lowercase(memberName)) {
                            funcSym = entry;
                            break;
                        }
                    }
                    if (funcSym)
                        break;
                }
            }

            if (!funcSym) {
                m_problems.error(
                    "11.3 undeclared member function",
                    std::format("class '{}' has no member function '{}'", classNode->name, memberName),
                    { idNode->children[1]->token.line > 0 ? idNode->children[1]->token : node->token });
                return "";
            }

            const Token &errTok = idNode->children[1]->token.line > 0 ? idNode->children[1]->token : node->token;
            if (funcSym->signature.params.size() != argTypes.size()) {
                m_problems.error(
                    "12.1 function call with wrong number of parameters",
                    std::format("'{}::{}' expects {} parameter(s), got {}", classNode->name, funcSym->name, funcSym->signature.params.size(), argTypes.size()),
                    { errTok });
            } else {
                for (size_t i = 0; i < argTypes.size(); ++i) {
                    const std::string &paramType = funcSym->signature.params[i];
                    const std::string &argType = argTypes[i];
                    if (argType.empty() || paramType.empty())
                        continue;
                    const int paramDims = CountArrayDimensions(paramType);
                    const int argDims = CountArrayDimensions(argType);
                    if (paramDims > 0 || argDims > 0) {
                        if (paramDims != argDims)
                            m_problems.error(
                                "13.3 array parameter using wrong number of dimensions",
                                std::format(
                                    "parameter {} of '{}::{}' expects {} dimension(s), got {}", i + 1, classNode->name, funcSym->name, paramDims, argDims),
                                { errTok });
                        continue;
                    }
                    if (!TypesCompatible(StripAllDimensions(argType), StripAllDimensions(paramType)))
                        m_problems.error(
                            "12.2 function call with wrong type of parameters",
                            std::format("'{}::{}' parameter {} expects '{}', got '{}'", classNode->name, funcSym->name, i + 1, paramType, argType),
                            { errTok });
                }
            }
            return funcSym->signature.type;
        }

        const std::string funcName = idNode->lexeme;
        std::vector<std::string> argTypes;
        for (const auto &arg : paramListNode->children) argTypes.push_back(inferType(arg, ctx));

        SymbolTableNode *funcSym = nullptr;
        if (ctx.class_node) {
            for (auto *entry : ctx.class_node->table) {
                if (entry->kind != SymbolTableNode::Kind::Function || lowercase(entry->name) != lowercase(funcName))
                    continue;
                if (entry->signature.params.size() == argTypes.size()) {
                    funcSym = entry;
                    break;
                }
                if (!funcSym)
                    funcSym = entry;
            }
        }
        if (!funcSym)
            funcSym = findFreeFunctionByNameAndArgs(funcName, argTypes);

        if (!funcSym) {
            m_problems.error(
                "11.4 undeclared/undefined free function",
                std::format("function '{}' is not declared", funcName),
                { idNode->token.line > 0 ? idNode->token : node->token });
            return "";
        }

        const Token &errTok = idNode->token.line > 0 ? idNode->token : node->token;
        if (funcSym->signature.params.size() != argTypes.size()) {
            m_problems.error(
                "12.1 function call with wrong number of parameters",
                std::format("'{}' expects {} parameter(s), got {}", funcName, funcSym->signature.params.size(), argTypes.size()),
                { errTok });
        } else {
            for (size_t i = 0; i < argTypes.size(); ++i) {
                const std::string &paramType = funcSym->signature.params[i];
                const std::string &argType = argTypes[i];
                if (argType.empty() || paramType.empty())
                    continue;
                const int paramDims = CountArrayDimensions(paramType);
                const int argDims = CountArrayDimensions(argType);
                if (paramDims > 0 || argDims > 0) {
                    if (paramDims != argDims)
                        m_problems.error(
                            "13.3 array parameter using wrong number of dimensions",
                            std::format("parameter {} of '{}' expects array of {} dimension(s), got {}", i + 1, funcName, paramDims, argDims),
                            { errTok });
                    continue;
                }
                if (!TypesCompatible(StripAllDimensions(argType), StripAllDimensions(paramType)))
                    m_problems.error(
                        "12.2 function call with wrong type of parameters",
                        std::format("'{}' parameter {} expects '{}', got '{}'", funcName, i + 1, paramType, argType),
                        { errTok });
            }
        }

        return funcSym->signature.type;
    }

} // namespace lang
