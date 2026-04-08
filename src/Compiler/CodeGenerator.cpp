#include "CodeGenerator.hpp"

#include <cassert>
#include <cmath>
#include <format>
#include <sstream>
#include <stdexcept>

namespace lang
{
    CodeGenerator::CodeGenerator(std::shared_ptr<const ASTNode> ast, const SymbolTableNode *globalTable) : m_ast(ast), m_globalTable(globalTable)
    {
        for (int i = 12; i >= 1; --i) m_freeRegs.push_back(i);
    }

    int CodeGenerator::allocReg()
    {
        if (m_freeRegs.empty())
            throw std::runtime_error("code generator: register exhausted");
        int r = m_freeRegs.back();
        m_freeRegs.pop_back();
        return r;
    }

    void CodeGenerator::freeReg(int r)
    {
        m_freeRegs.push_back(r);
    }

    void CodeGenerator::emit(const std::string &line)
    {
        m_code << line << "\n";
    }

    void CodeGenerator::emitData(const std::string &line)
    {
        m_data << line << "\n";
    }

    std::string CodeGenerator::newLabel(const std::string &hint)
    {
        return std::format("{}{}", hint, m_labelCounter++);
    }


    int CodeGenerator::sizeOf(const std::string &type) const
    {
        size_t bracket = type.find('[');
        std::string base = (bracket != std::string::npos) ? type.substr(0, bracket) : type;

        if (base.empty())
            return 4;

        int baseSize;
        if (base == "int" || base == "float" || base == "void") {
            baseSize = 4;
        } else {
            const SymbolTableNode *cls = findClass(base);
            baseSize = cls ? sizeOfClass(cls) : 4;
        }

        if (bracket == std::string::npos)
            return baseSize;

        int product = 1;
        size_t pos = bracket;
        while (pos != std::string::npos) {
            size_t close = type.find(']', pos);
            std::string dimStr = type.substr(pos + 1, close - pos - 1);
            if (dimStr.empty()) {
                return 4;
            }
            product *= std::stoi(dimStr);
            pos = type.find('[', close);
        }
        return baseSize * product;
    }

    int CodeGenerator::sizeOfClass(const SymbolTableNode *cls) const
    {
        if (!cls)
            return 0;

        int total = 0;

        for (auto *entry : cls->table) {
            if (entry->kind == SymbolTableNode::Kind::Inherit && entry->name != "none") {
                std::string inherited = entry->name;
                size_t pos = 0;
                while (pos < inherited.size()) {
                    size_t comma = inherited.find(',', pos);
                    std::string baseName = (comma == std::string::npos) ? inherited.substr(pos) : inherited.substr(pos, comma - pos);
                    while (!baseName.empty() && baseName.front() == ' ') baseName.erase(0, 1);
                    while (!baseName.empty() && baseName.back() == ' ') baseName.pop_back();
                    const SymbolTableNode *base = findClass(baseName);

                    if (base)
                        total += sizeOfClass(base);

                    pos = (comma == std::string::npos) ? inherited.size() : comma + 1;
                }
            }
        }

        for (auto *entry : cls->table) {
            if (entry->kind == SymbolTableNode::Kind::Data)
                total += sizeOf(entry->signature.type);
        }

        return total;
    }

    bool CodeGenerator::isPointerType(const std::string &type) const
    {
        size_t bracket = type.find('[');

        if (bracket == std::string::npos)
            return false;

        size_t close = type.find(']', bracket);

        return (close == bracket + 1);
    }

    const SymbolTableNode *CodeGenerator::findClass(const std::string &name) const
    {
        for (auto *entry : m_globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Class && entry->name == name)
                return entry;
        }

        std::string lower = name;
        for (char &c : lower) c = (char)std::tolower((unsigned char)c);

        for (auto *entry : m_globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Class) {
                std::string eName = entry->name;
                for (char &c : eName) c = (char)std::tolower((unsigned char)c);
                if (eName == lower)
                    return entry;
            }
        }

        return nullptr;
    }

    const SymbolTableNode *CodeGenerator::findFreeFunction(const std::string &name) const
    {
        for (auto *entry : m_globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Function && entry->name == name)
                return entry;
        }

        std::string lower = name;
        for (char &c : lower) c = (char)std::tolower((unsigned char)c);

        for (auto *entry : m_globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Function) {
                std::string eName = entry->name;
                for (char &c : eName) c = (char)std::tolower((unsigned char)c);
                if (eName == lower)
                    return entry;
            }
        }

        return nullptr;
    }

    const SymbolTableNode *CodeGenerator::findMethod(const SymbolTableNode *cls, const std::string &name) const
    {
        if (!cls)
            return nullptr;

        for (auto *entry : cls->table) {
            if (entry->kind == SymbolTableNode::Kind::Function && entry->name == name)
                return entry;
        }

        return nullptr;
    }

    int CodeGenerator::memberOffset(const SymbolTableNode *cls, const std::string &name) const
    {
        if (!cls)
            return 0;

        int offset = 0;

        for (auto *entry : cls->table) {
            if (entry->kind == SymbolTableNode::Kind::Inherit && entry->name != "none") {
                std::string inherited = entry->name;
                size_t pos = 0;
                while (pos < inherited.size()) {
                    size_t comma = inherited.find(',', pos);
                    std::string baseName = (comma == std::string::npos) ? inherited.substr(pos) : inherited.substr(pos, comma - pos);
                    while (!baseName.empty() && baseName.front() == ' ') baseName.erase(0, 1);
                    while (!baseName.empty() && baseName.back() == ' ') baseName.pop_back();
                    const SymbolTableNode *base = findClass(baseName);
                    if (base)
                        offset += sizeOfClass(base);
                    pos = (comma == std::string::npos) ? inherited.size() : comma + 1;
                }
            }
        }

        for (auto *entry : cls->table) {
            if (entry->kind != SymbolTableNode::Kind::Data)
                continue;
            if (entry->name == name)
                return offset;
            offset += sizeOf(entry->signature.type);
        }

        return 0;
    }

    std::string CodeGenerator::getVarType(const std::string &name) const
    {
        if (m_currentFuncNode) {
            for (auto *entry : m_currentFuncNode->table) {
                if ((entry->kind == SymbolTableNode::Kind::Parameter || entry->kind == SymbolTableNode::Kind::Local) && entry->name == name)
                    return entry->signature.type;
            }
        }

        if (m_currentClassNode) {
            for (auto *entry : m_currentClassNode->table) {
                if (entry->kind == SymbolTableNode::Kind::Data && entry->name == name)
                    return entry->signature.type;
            }
        }

        auto it = m_globalLabels.find(name);
        if (it != m_globalLabels.end()) {
            for (auto *fentry : m_globalTable->table) {
                if (fentry->kind == SymbolTableNode::Kind::Function && fentry->name == "main") {
                    for (auto *entry : fentry->table) {
                        if (entry->name == name)
                            return entry->signature.type;
                    }
                }
            }
        }

        return "";
    }

    FrameInfo CodeGenerator::computeFrameInfo(const SymbolTableNode *funcNode, bool isMember) const
    {
        FrameInfo fi;
        fi.is_member = isMember;
        int cursor = 0;

        cursor += 4;
        fi.offsets["__link"] = -cursor;

        if (isMember) {
            cursor += 4;
            fi.offsets["__self"] = -cursor;
        }

        for (auto *entry : funcNode->table) {
            if (entry->kind != SymbolTableNode::Kind::Parameter)
                continue;
            int sz = sizeOf(entry->signature.type);
            if (sz <= 0)
                sz = 4;
            cursor += sz;
            fi.offsets[entry->name] = -cursor;
        }

        for (auto *entry : funcNode->table) {
            if (entry->kind != SymbolTableNode::Kind::Local)
                continue;
            int sz = sizeOf(entry->signature.type);
            if (sz <= 0)
                sz = 4;
            cursor += sz;
            fi.offsets[entry->name] = -cursor;
        }

        fi.frame_size = cursor;
        return fi;
    }

    std::unordered_map<std::string, std::string> CodeGenerator::allocateGlobals(const SymbolTableNode *mainNode)
    {
        std::unordered_map<std::string, std::string> labels;

        if (!mainNode)
            return labels;

        for (auto *entry : mainNode->table) {
            if (entry->kind != SymbolTableNode::Kind::Local)
                continue;
            std::string label = std::format("t{}_{}", 0, entry->name);
            int sz = sizeOf(entry->signature.type);
            if (sz <= 0)
                sz = 4;
            emitData(std::format("{:<12} res    {}   % {} {}", label, sz, entry->signature.type, entry->name));
            labels[entry->name] = label;
        }

        return labels;
    }

    std::string CodeGenerator::functionLabel(const SymbolTableNode *funcNode, const SymbolTableNode *classNode) const
    {
        if (classNode) {
            return classNode->name + "_" + funcNode->name;
        }

        return funcNode->name;
    }

    int CodeGenerator::loadVar(const std::string &name)
    {
        int r = allocReg();

        auto fit = m_currentFrame.offsets.find(name);
        if (fit != m_currentFrame.offsets.end()) {
            emit(std::format("         lw     r{},{}(r14)", r, fit->second));
            return r;
        }

        auto git = m_globalLabels.find(name);
        if (git != m_globalLabels.end()) {
            emit(std::format("         lw     r{},{}(r0)", r, git->second));
            return r;
        }

        emit(std::format("         add    r{},r0,r0   % var '{}' not found", r, name));
        return r;
    }

    void CodeGenerator::storeVar(const std::string &name, int valueReg)
    {
        auto fit = m_currentFrame.offsets.find(name);
        if (fit != m_currentFrame.offsets.end()) {
            emit(std::format("         sw     {}(r14),r{}", fit->second, valueReg));
            return;
        }

        auto git = m_globalLabels.find(name);
        if (git != m_globalLabels.end()) {
            emit(std::format("         sw     {}(r0),r{}", git->second, valueReg));
            return;
        }
    }

    int CodeGenerator::addrOfVar(const std::string &name)
    {
        int r = allocReg();

        auto fit = m_currentFrame.offsets.find(name);
        if (fit != m_currentFrame.offsets.end()) {
            emit(std::format("         addi   r{},r14,{}", r, fit->second));
            return r;
        }

        auto git = m_globalLabels.find(name);
        if (git != m_globalLabels.end()) {
            emit(std::format("         addi   r{},r0,{}", r, git->second));
            return r;
        }

        emit(std::format("         add    r{},r0,r0   % addr of '{}' not found", r, name));
        return r;
    }

    std::string CodeGenerator::generate()
    {
        m_freeRegs.clear();
        for (int i = 12; i >= 1; --i) m_freeRegs.push_back(i);
        m_code.str("");
        m_data.str("");
        m_labelCounter = 0;

        generateProg(m_ast);

        std::ostringstream out;
        out << m_code.str() << "\n";

        out << "         align\n";
        out << m_data.str();
        out << "         align\n\n";

        appendIOHelpers(out);

        return out.str();
    }

    void CodeGenerator::generateProg(std::shared_ptr<const ASTNode> prog)
    {
        if (!prog || prog->children.size() < 3)
            return;

        const SymbolTableNode *mainNode = nullptr;
        for (auto *entry : m_globalTable->table) {
            if (entry->kind == SymbolTableNode::Kind::Function && entry->name == "main") {
                mainNode = entry;
                break;
            }
        }

        if (mainNode)
            m_globalLabels = allocateGlobals(mainNode);

        emit("         align");
        emit("         entry");
        emit("         addi   r14,r0,topaddr   % initialize stack pointer");
        emit("");

        m_currentFuncNode = mainNode;
        m_currentClassNode = nullptr;
        m_currentFrame = {};
        generateStatBlock(prog->children[2]);

        emit("         hlt");
        emit("");

        for (auto &funcDef : prog->children[1]->children) {
            generateFuncDef(funcDef);
        }
    }

    void CodeGenerator::generateFuncDef(std::shared_ptr<const ASTNode> funcDef)
    {
        if (!funcDef || funcDef->children.size() < 4)
            return;

        auto &nameNode = funcDef->children[1];
        auto &statBlock = funcDef->children[3];

        const SymbolTableNode *funcSym = nullptr;
        const SymbolTableNode *classSym = nullptr;
        bool isMember = (nameNode->kind == ASTNode::Kind::MemberAccess);

        if (isMember) {
            std::string className = nameNode->children[0]->lexeme;
            std::string funcName = nameNode->children[1]->lexeme;
            classSym = findClass(className);
            if (classSym)
                funcSym = findMethod(classSym, funcName);
        } else {
            funcSym = findFreeFunction(nameNode->lexeme);
        }

        if (!funcSym)
            return;

        std::string label = functionLabel(funcSym, classSym);
        FrameInfo frame = computeFrameInfo(funcSym, isMember);

        emit(std::format("% ---- function: {} ----", label));
        emit(std::format("{:<12}sw     -4(r14),r15   % save link register", label));

        FrameInfo savedFrame = m_currentFrame;
        const SymbolTableNode *savedFunc = m_currentFuncNode;
        const SymbolTableNode *savedClass = m_currentClassNode;

        m_currentFrame = frame;
        m_currentFuncNode = funcSym;
        m_currentClassNode = classSym;

        generateStatBlock(statBlock);

        emit(std::format("         lw     r15,-4(r14)   % restore link register"));
        emit(std::format("         jr     r15"));
        emit("");

        m_currentFrame = savedFrame;
        m_currentFuncNode = savedFunc;
        m_currentClassNode = savedClass;
    }

    void CodeGenerator::generateStatBlock(std::shared_ptr<const ASTNode> node)
    {
        if (!node)
            return;
        for (auto &child : node->children) {
            if (!child)
                continue;
            if (child->kind == ASTNode::Kind::VarDecl)
                continue;
            generateStatement(child);
        }
    }

    void CodeGenerator::generateStatement(std::shared_ptr<const ASTNode> node)
    {
        if (!node)
            return;
        switch (node->kind) {
            case ASTNode::Kind::AssignStat:
                generateAssignStat(node);
                break;
            case ASTNode::Kind::IfStat:
                generateIfStat(node);
                break;
            case ASTNode::Kind::WhileStat:
                generateWhileStat(node);
                break;
            case ASTNode::Kind::PutStat:
                generatePutStat(node);
                break;
            case ASTNode::Kind::ReadStat:
                generateReadStat(node);
                break;
            case ASTNode::Kind::ReturnStat:
                generateReturnStat(node);
                break;
            case ASTNode::Kind::FuncCall:
                {
                    int r = generateFuncCallExpr(node);
                    freeReg(r);
                }
                break;
            default:
                break;
        }
    }

    void CodeGenerator::generateAssignStat(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.size() < 2)
            return;

        auto &lhs = node->children[0];
        auto &rhs = node->children[1];

        int rhsReg = generateExpr(rhs);

        // If the LHS is a float variable but the RHS is an integer expression, scale up
        if (isFloatExpr(lhs) && !isFloatExpr(rhs)) {
            emit(std::format("         muli   r{},r{},100   % promote int rhs to float x100", rhsReg, rhsReg));
        }

        if (lhs->kind == ASTNode::Kind::Id) {
            storeVar(lhs->lexeme, rhsReg);
        } else {
            int addrReg = generateLValue(lhs);
            emit(std::format("         sw     0(r{}),r{}", addrReg, rhsReg));
            freeReg(addrReg);
        }
        freeReg(rhsReg);
    }

    void CodeGenerator::generateIfStat(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.size() < 3)
            return;

        std::string elseLabel = newLabel("else");
        std::string endLabel = newLabel("endif");

        int condReg = generateExpr(node->children[0]);
        emit(std::format("         bz     r{},{}   % if false → else", condReg, elseLabel));
        freeReg(condReg);

        generateStatBlock(node->children[1]);
        emit(std::format("         j      {}", endLabel));

        emit(std::format("{:<12}add    r0,r0,r0   % else", elseLabel));
        generateStatBlock(node->children[2]);

        emit(std::format("{:<12}add    r0,r0,r0   % endif", endLabel));
    }

    void CodeGenerator::generateWhileStat(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.size() < 2)
            return;

        std::string loopLabel = newLabel("while");
        std::string endLabel = newLabel("endwhile");

        emit(std::format("{:<12}add    r0,r0,r0   % while loop start", loopLabel));

        int condReg = generateExpr(node->children[0]);
        emit(std::format("         bz     r{},{}   % while false → end", condReg, endLabel));
        freeReg(condReg);

        generateStatBlock(node->children[1]);
        emit(std::format("         j      {}", loopLabel));

        emit(std::format("{:<12}add    r0,r0,r0   % end while", endLabel));
    }

    void CodeGenerator::generatePutStat(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.empty())
            return;

        bool isFloat = isFloatExpr(node->children[0]);
        int valReg = generateExpr(node->children[0]);
        if (valReg != 1) {
            emit(std::format("         add    r1,r{},r0   % move to r1 for put", valReg));
            freeReg(valReg);
        }

        if (isFloat) {
            emit("         jl     r15,putfloat   % write float");
        } else {
            emit("         jl     r15,putint   % write integer");
        }
        if (valReg == 1)
            freeReg(1);

        emit("         addi   r1,r0,10");
        emit("         putc   r1           % newline");
    }

    void CodeGenerator::generateReadStat(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.empty())
            return;

        emit("         jl     r15,getint   % read integer → r1");

        auto &var = node->children[0];

        // If the target variable is float, scale the integer input by 100
        bool targetIsFloat = isFloatExpr(var);
        if (targetIsFloat) {
            emit("         muli   r1,r1,100   % scale int input to float x100");
        }

        if (var->kind == ASTNode::Kind::Id) {
            auto fit = m_currentFrame.offsets.find(var->lexeme);
            if (fit != m_currentFrame.offsets.end()) {
                emit(std::format("         sw     {}(r14),r1   % store read result", fit->second));
            } else {
                auto git = m_globalLabels.find(var->lexeme);
                if (git != m_globalLabels.end()) {
                    emit(std::format("         sw     {}(r0),r1   % store read result", git->second));
                }
            }
        } else {
            int addrReg = allocReg();
            emit(std::format("         add    r{},r1,r0   % save getint result", addrReg));
            int addr2 = generateLValue(var);
            emit(std::format("         sw     0(r{}),r{}", addr2, addrReg));
            freeReg(addr2);
            freeReg(addrReg);
        }
    }

    void CodeGenerator::generateReturnStat(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.empty()) {
            emit("         add    r13,r0,r0   % return void");
        } else {
            int valReg = generateExpr(node->children[0]);
            // If the function returns float but the expression is integer, scale up
            bool funcReturnsFloat = m_currentFuncNode && m_currentFuncNode->signature.type == "float";
            bool exprIsFloat = isFloatExpr(node->children[0]);
            if (funcReturnsFloat && !exprIsFloat) {
                emit(std::format("         muli   r{},r{},100   % promote int return to float x100", valReg, valReg));
            }
            emit(std::format("         add    r13,r{},r0   % set return value", valReg));
            freeReg(valReg);
        }
        emit("         lw     r15,-4(r14)   % restore link");
        emit("         jr     r15           % return");
    }

    int CodeGenerator::generateExpr(std::shared_ptr<const ASTNode> node)
    {
        if (!node) {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0   % null expr", r));
            return r;
        }
        switch (node->kind) {
            case ASTNode::Kind::AddOp:
                return generateBinaryOp(node, "");
            case ASTNode::Kind::MultOp:
                return generateBinaryOp(node, "");
            case ASTNode::Kind::RelOp:
                return generateRelOp(node);
            case ASTNode::Kind::NotExpr:
                return generateNotExpr(node);
            case ASTNode::Kind::SignExpr:
                return generateSignExpr(node);
            case ASTNode::Kind::Num:
                return generateNum(node);
            case ASTNode::Kind::Id:
                return generateIdExpr(node);
            case ASTNode::Kind::FuncCall:
                return generateFuncCallExpr(node);
            case ASTNode::Kind::IndexedVar:
                return generateIndexedVarExpr(node);
            case ASTNode::Kind::MemberAccess:
                return generateMemberAccessExpr(node);
            default:
                {
                    int r = allocReg();
                    emit(std::format("         add    r{},r0,r0   % unknown expr", r));
                    return r;
                }
        }
    }

    int CodeGenerator::generateBinaryOp(std::shared_ptr<const ASTNode> node, const std::string & /*unused*/)
    {
        if (!node || node->children.size() < 2) {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0", r));
            return r;
        }

        bool leftIsFloat  = isFloatExpr(node->children[0]);
        bool rightIsFloat = isFloatExpr(node->children[1]);
        bool floatCtx = leftIsFloat || rightIsFloat;

        int lReg = generateExpr(node->children[0]);
        int rReg = generateExpr(node->children[1]);

        // Promote integer operand to fixed-point if in float context
        if (floatCtx && !leftIsFloat) {
            emit(std::format("         muli   r{},r{},100   % promote int lhs to float x100", lReg, lReg));
        }
        if (floatCtx && !rightIsFloat) {
            emit(std::format("         muli   r{},r{},100   % promote int rhs to float x100", rReg, rReg));
        }

        int res = allocReg();

        const std::string &op = node->lexeme;
        if (op == "+") {
            emit(std::format("         add    r{},r{},r{}", res, lReg, rReg));
        } else if (op == "-") {
            emit(std::format("         sub    r{},r{},r{}", res, lReg, rReg));
        } else if (op == "or") {
            emit(std::format("         or     r{},r{},r{}", res, lReg, rReg));
        } else if (op == "*") {
            if (floatCtx) {
                // Before multiply, scale lhs up so result stays in x100 range
                emit(std::format("         muli   r{},r{},100   % pre-scale for float mul", lReg, lReg));
            }
            emit(std::format("         mul    r{},r{},r{}", res, lReg, rReg));
            if (floatCtx) {
                emit(std::format("         divi   r{},r{},100   % post-scale for float mul", res, res));
            }
        } else if (op == "/") {
            if (floatCtx) {
                // Scale lhs up before dividing so we preserve fraction
                emit(std::format("         muli   r{},r{},100   % pre-scale for float div", lReg, lReg));
            }
            emit(std::format("         div    r{},r{},r{}", res, lReg, rReg));
        } else if (op == "and") {
            emit(std::format("         and    r{},r{},r{}", res, lReg, rReg));
        } else {
            emit(std::format("         add    r{},r{},r{}", res, lReg, rReg));
        }

        freeReg(lReg);
        freeReg(rReg);
        return res;
    }

    int CodeGenerator::generateRelOp(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.size() < 2) {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0", r));
            return r;
        }

        bool leftIsFloat  = isFloatExpr(node->children[0]);
        bool rightIsFloat = isFloatExpr(node->children[1]);
        bool floatCtx = leftIsFloat || rightIsFloat;

        int lReg = generateExpr(node->children[0]);
        int rReg = generateExpr(node->children[1]);

        if (floatCtx && !leftIsFloat) {
            emit(std::format("         muli   r{},r{},100   % promote int lhs for float relop", lReg, lReg));
        }
        if (floatCtx && !rightIsFloat) {
            emit(std::format("         muli   r{},r{},100   % promote int rhs for float relop", rReg, rReg));
        }

        int res = allocReg();

        const std::string &op = node->lexeme;
        if (op == "==") {
            emit(std::format("         ceq    r{},r{},r{}", res, lReg, rReg));
        } else if (op == "!=") {
            emit(std::format("         cne    r{},r{},r{}", res, lReg, rReg));
        } else if (op == "<") {
            emit(std::format("         clt    r{},r{},r{}", res, lReg, rReg));
        } else if (op == ">") {
            emit(std::format("         cgt    r{},r{},r{}", res, lReg, rReg));
        } else if (op == "<=") {
            emit(std::format("         cle    r{},r{},r{}", res, lReg, rReg));
        } else if (op == ">=") {
            emit(std::format("         cge    r{},r{},r{}", res, lReg, rReg));
        } else {
            emit(std::format("         ceq    r{},r{},r{}", res, lReg, rReg));
        }

        freeReg(lReg);
        freeReg(rReg);
        return res;
    }

    int CodeGenerator::generateNotExpr(std::shared_ptr<const ASTNode> node)
    {
        int operand = generateExpr(node->children[0]);
        int res = allocReg();
        emit(std::format("         ceqi   r{},r{},0   % logical not", res, operand));
        freeReg(operand);
        return res;
    }

    int CodeGenerator::generateSignExpr(std::shared_ptr<const ASTNode> node)
    {
        int operand = generateExpr(node->children[0]);
        if (node->lexeme == "-") {
            int res = allocReg();
            emit(std::format("         sub    r{},r0,r{}   % negate", res, operand));
            freeReg(operand);
            return res;
        }
        return operand;
    }

    int CodeGenerator::generateNum(std::shared_ptr<const ASTNode> node)
    {
        int r = allocReg();
        const std::string &lex = node->lexeme;
        if (lex.find('.') != std::string::npos) {
            long scaled = std::lround(std::stod(lex) * 100.0);
            emit(std::format("         addi   r{},r0,{}   % float literal {} scaled x100", r, scaled, lex));
        } else {
            emit(std::format("         addi   r{},r0,{}   % integer literal", r, lex));
        }
        return r;
    }

    int CodeGenerator::generateIdExpr(std::shared_ptr<const ASTNode> node)
    {
        return loadVar(node->lexeme);
    }

    int CodeGenerator::generateFuncCallExpr(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.size() < 2) {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0   % null func call", r));
            return r;
        }

        auto &calleeNode = node->children[0];
        auto &paramListNode = node->children[1];

        if (calleeNode->kind == ASTNode::Kind::Id) {
            const SymbolTableNode *funcSym = findFreeFunction(calleeNode->lexeme);
            if (!funcSym) {
                int r = allocReg();
                emit(std::format("         add    r{},r0,r0   % unknown func {}", r, calleeNode->lexeme));
                return r;
            }
            return callFunction(funcSym, paramListNode->children, nullptr, -1);
        } else if (calleeNode->kind == ASTNode::Kind::MemberAccess) {
            auto &objNode = calleeNode->children[0];
            auto &methodNode = calleeNode->children[1];
            std::string objType = getVarType(objNode->lexeme);
            const SymbolTableNode *classSym = findClass(objType);
            const SymbolTableNode *methodSym = classSym ? findMethod(classSym, methodNode->lexeme) : nullptr;
            if (!methodSym) {
                int r = allocReg();
                emit(std::format("         add    r{},r0,r0   % unknown method", r));
                return r;
            }
            int selfReg = generateLValue(objNode);
            int result = callFunction(methodSym, paramListNode->children, classSym, selfReg);
            freeReg(selfReg);
            return result;
        }

        int r = allocReg();
        emit(std::format("         add    r{},r0,r0   % unhandled call node kind", r));
        return r;
    }

    int CodeGenerator::generateIndexedVarExpr(std::shared_ptr<const ASTNode> node)
    {
        int addrReg = generateIndexedVarAddr(node);
        int valReg = allocReg();
        emit(std::format("         lw     r{},0(r{})   % load array element", valReg, addrReg));
        freeReg(addrReg);
        return valReg;
    }

    int CodeGenerator::generateMemberAccessExpr(std::shared_ptr<const ASTNode> node)
    {
        int addrReg = generateMemberAccessAddr(node);
        int valReg = allocReg();
        emit(std::format("         lw     r{},0(r{})   % load member", valReg, addrReg));
        freeReg(addrReg);
        return valReg;
    }

    bool CodeGenerator::isFloatExpr(std::shared_ptr<const ASTNode> node) const
    {
        if (!node)
            return false;
        switch (node->kind) {
            case ASTNode::Kind::Num:
                return node->lexeme.find('.') != std::string::npos;
            case ASTNode::Kind::Id:
                return getVarType(node->lexeme) == "float";
            case ASTNode::Kind::AddOp:
            case ASTNode::Kind::MultOp:
                if (node->children.size() < 2)
                    return false;
                return isFloatExpr(node->children[0]) || isFloatExpr(node->children[1]);
            case ASTNode::Kind::SignExpr:
                if (node->children.empty())
                    return false;
                return isFloatExpr(node->children[0]);
            case ASTNode::Kind::NotExpr:
            case ASTNode::Kind::RelOp:
                return false;
            case ASTNode::Kind::FuncCall:
                {
                    if (node->children.empty())
                        return false;
                    auto &calleeNode = node->children[0];
                    if (calleeNode->kind == ASTNode::Kind::Id) {
                        const SymbolTableNode *fn = findFreeFunction(calleeNode->lexeme);
                        return fn && fn->signature.type == "float";
                    } else if (calleeNode->kind == ASTNode::Kind::MemberAccess && calleeNode->children.size() >= 2) {
                        std::string objType = getVarType(calleeNode->children[0]->lexeme);
                        const SymbolTableNode *cls = findClass(objType);
                        const SymbolTableNode *method = cls ? findMethod(cls, calleeNode->children[1]->lexeme) : nullptr;
                        return method && method->signature.type == "float";
                    }
                    return false;
                }
            case ASTNode::Kind::MemberAccess:
                {
                    if (node->children.size() < 2)
                        return false;
                    std::string objType = getVarType(node->children[0]->lexeme);
                    const SymbolTableNode *cls = findClass(objType);
                    if (!cls)
                        return false;
                    const std::string &memberName = node->children[1]->lexeme;
                    for (auto *entry : cls->table) {
                        if (entry->kind == SymbolTableNode::Kind::Data && entry->name == memberName) {
                            std::string t = entry->signature.type;
                            size_t b = t.find('[');
                            if (b != std::string::npos)
                                t = t.substr(0, b);
                            return t == "float";
                        }
                    }
                    return false;
                }
            case ASTNode::Kind::IndexedVar:
                {
                    if (node->children.empty())
                        return false;
                    auto &baseNode = node->children[0];
                    if (baseNode->kind != ASTNode::Kind::Id)
                        return false;
                    std::string varType = getVarType(baseNode->lexeme);
                    // Strip one array dimension
                    size_t bracket = varType.find('[');
                    if (bracket == std::string::npos)
                        return false;
                    size_t close = varType.find(']', bracket);
                    if (close == std::string::npos)
                        return false;
                    std::string elemType = varType.substr(0, bracket) + varType.substr(close + 1);
                    // Get base type
                    size_t b2 = elemType.find('[');
                    std::string baseType = (b2 != std::string::npos) ? elemType.substr(0, b2) : elemType;
                    return baseType == "float";
                }
            default:
                return false;
        }
    }

    int CodeGenerator::generateLValue(std::shared_ptr<const ASTNode> node)
    {
        if (!node)
            return allocReg();
        switch (node->kind) {
            case ASTNode::Kind::Id:
                return addrOfVar(node->lexeme);
            case ASTNode::Kind::IndexedVar:
                return generateIndexedVarAddr(node);
            case ASTNode::Kind::MemberAccess:
                return generateMemberAccessAddr(node);
            default:
                {
                    int r = allocReg();
                    emit(std::format("         add    r{},r0,r0   % lvalue unknown kind", r));
                    return r;
                }
        }
    }

    int CodeGenerator::generateIndexedVarAddr(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.size() < 2)
            return allocReg();

        auto &baseNode = node->children[0];
        auto &indexNode = node->children[1];

        int baseReg;
        int elemSize = 4;

        if (baseNode->kind == ASTNode::Kind::Id) {
            const std::string &arrName = baseNode->lexeme;
            std::string varType = getVarType(arrName);

            if (!varType.empty()) {
                size_t bracket = varType.find('[');
                if (bracket != std::string::npos) {
                    size_t close = varType.find(']', bracket);
                    std::string elemType = varType.substr(0, bracket) + varType.substr(close + 1);
                    elemSize = sizeOf(elemType.empty() ? varType.substr(0, bracket) : elemType);
                    if (elemSize <= 0)
                        elemSize = 4;
                }
            }

            bool isPointer = isPointerType(varType);
            if (isPointer) {
                baseReg = allocReg();
                auto fit = m_currentFrame.offsets.find(arrName);
                if (fit != m_currentFrame.offsets.end()) {
                    emit(std::format("         lw     r{},{}(r14)   % load array pointer param", baseReg, fit->second));
                } else {
                    auto git = m_globalLabels.find(arrName);
                    if (git != m_globalLabels.end()) {
                        emit(std::format("         lw     r{},{}(r0)   % load array pointer global", baseReg, git->second));
                    } else {
                        emit(std::format("         add    r{},r0,r0   % pointer base not found", baseReg));
                    }
                }
            } else {
                baseReg = addrOfVar(arrName);
            }
        } else {
            baseReg = generateIndexedVarAddr(baseNode);
            elemSize = 4;
        }

        int idxReg = generateExpr(indexNode);

        int offReg = allocReg();
        if (elemSize == 4) {
            emit(std::format("         muli   r{},r{},4   % offset = index * 4", offReg, idxReg));
        } else {
            emit(std::format("         muli   r{},r{},{}   % offset = index * elemSize", offReg, idxReg, elemSize));
        }
        freeReg(idxReg);

        emit(std::format("         add    r{},r{},r{}   % element address", baseReg, baseReg, offReg));
        freeReg(offReg);

        return baseReg;
    }

    int CodeGenerator::generateMemberAccessAddr(std::shared_ptr<const ASTNode> node)
    {
        if (!node || node->children.size() < 2)
            return allocReg();

        auto &objNode = node->children[0];
        auto &memberNode = node->children[1];

        int objAddrReg;
        std::string objType;

        if (objNode->kind == ASTNode::Kind::Id) {
            objType = getVarType(objNode->lexeme);
            objAddrReg = addrOfVar(objNode->lexeme);
        } else {
            objAddrReg = generateLValue(objNode);
        }

        const SymbolTableNode *cls = findClass(objType);
        int offset = cls ? memberOffset(cls, memberNode->lexeme) : 0;

        if (offset == 0)
            return objAddrReg;

        int addrReg = allocReg();
        emit(std::format("         addi   r{},r{},{}   % member offset", addrReg, objAddrReg, offset));
        freeReg(objAddrReg);
        return addrReg;
    }

    int CodeGenerator::callFunction(
        const SymbolTableNode *funcNode, const std::vector<std::shared_ptr<ASTNode>> &args, const SymbolTableNode *classNode, int selfAddrReg)
    {
        if (!funcNode) {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0   % null func call", r));
            return r;
        }

        bool isMember = (classNode != nullptr);
        FrameInfo calleeFrame = computeFrameInfo(funcNode, isMember);

        std::vector<const SymbolTableNode *> params;
        for (auto *entry : funcNode->table) {
            if (entry->kind == SymbolTableNode::Kind::Parameter)
                params.push_back(entry);
        }

        std::vector<int> argRegs;
        for (size_t i = 0; i < args.size(); i++) {
            bool passAsPointer = (i < params.size()) && isPointerType(params[i]->signature.type);
            int reg = passAsPointer ? generateLValue(args[i]) : generateExpr(args[i]);
            // If the parameter expects float but the argument is an integer expression, scale up
            if (i < params.size() && params[i]->signature.type == "float" && !isFloatExpr(args[i])) {
                emit(std::format("         muli   r{},r{},100   % promote int arg to float x100 for param '{}'", reg, reg, params[i]->name));
            }
            argRegs.push_back(reg);
        }

        if (isMember && selfAddrReg >= 0) {
            auto selfIt = calleeFrame.offsets.find("__self");
            if (selfIt != calleeFrame.offsets.end()) {
                int placementOffset = selfIt->second - m_currentFrame.frame_size;
                emit(std::format("         sw     {}(r14),r{}   % pass self pointer", placementOffset, selfAddrReg));
            }
        }

        for (size_t i = 0; i < argRegs.size() && i < params.size(); i++) {
            auto pit = calleeFrame.offsets.find(params[i]->name);
            if (pit != calleeFrame.offsets.end()) {
                int placementOffset = pit->second - m_currentFrame.frame_size;
                emit(std::format("         sw     {}(r14),r{}   % pass arg '{}'", placementOffset, argRegs[i], params[i]->name));
            }
        }

        for (int r : argRegs) freeReg(r);

        if (m_currentFrame.frame_size > 0) {
            emit(std::format("         subi   r14,r14,{}", m_currentFrame.frame_size));
        }

        emit(std::format("         jl     r15,{}   % call {}", functionLabel(funcNode, classNode), funcNode->name));

        if (m_currentFrame.frame_size > 0) {
            emit(std::format("         addi   r14,r14,{}", m_currentFrame.frame_size));
        }

        int resultReg = allocReg();
        emit(std::format("         add    r{},r13,r0   % copy return value", resultReg));
        return resultReg;
    }

    void CodeGenerator::appendIOHelpers(std::ostringstream &out)
    {
        out << R"(         align
% Write an integer to the output.
% Entry:  r1 contains the integer.
% Uses: r1, r2, r3, r4, r5.
% Link: r15.
putint   add    r2,r0,r0         % c := 0
         add    r3,r0,r0         % s := 0 (sign)
         addi   r4,r0,endbuf     % p is the buffer pointer
         cge    r5,r1,r0
         bnz    r5,putint1       % branch if n >= 0
         addi   r3,r0,1          % s := 1
         sub    r1,r0,r1         % n := -n
putint1  modi   r2,r1,10         % c := n mod 10
         addi   r2,r2,48         % c := c + '0'
         subi   r4,r4,1          % p := p - 1
         sb     0(r4),r2         % buf[p] := c
         divi   r1,r1,10         % n := n div 10
         bnz    r1,putint1       % do next digit
         bz     r3,putint2       % branch if n >= 0
         addi   r2,r0,45         % c := '-'
         subi   r4,r4,1          % p := p - 1
         sb     0(r4),r2         % buf[p] := c
putint2  lb     r2,0(r4)         % c := buf[p]
         putc   r2               % write c
         addi   r4,r4,1          % p := p + 1
         cgei   r5,r4,endbuf
         bz     r5,putint2       % branch if more digits
         jr     r15              % return

         res    20               % digit buffer
endbuf

% Read an integer.
% Exit: r1 contains value of integer read.
% Uses: r1, r2, r3, r4.
% Link: r15.
getint   add    r1,r0,r0         % n := 0
         add    r2,r0,r0         % c := 0
         add    r3,r0,r0         % s := 0 (sign)
getint1  getc   r2               % read c
         ceqi   r4,r2,32
         bnz    r4,getint1       % skip blanks
         ceqi   r4,r2,43
         bnz    r4,getint2       % branch if c is '+'
         ceqi   r4,r2,45
         bz     r4,getint3       % branch if c is not '-'
         addi   r3,r0,1          % s := 1 (number is negative)
getint2  getc   r2               % read c
getint3  ceqi   r4,r2,10
         bnz    r4,getint5       % branch if c is newline
         cgei   r4,r2,48
         bz     r4,getint4       % c < '0'
         clei   r4,r2,57
         bz     r4,getint4       % c > '9'
         muli   r1,r1,10         % n := 10 * n
         add    r1,r1,r2         % n := n + c
         subi   r1,r1,48         % n := n - '0'
         j      getint2
getint4  addi   r2,r0,63         % c := '?'
         putc   r2               % write c
         j      getint           % try again
getint5  bz     r3,getint6       % branch if s = 0 (positive)
         sub    r1,r0,r1         % n := -n
getint6  jr     r15              % return

         align
% Write a fixed-point float (scaled x100) to output as D.FF
% Entry:  r1 = value * 100 (signed)
% Uses:   r1, r2, r3, r4, r5.
% Link:   r15.
% Does NOT print newline (caller does it).
putfloat add    r2,r0,r0         % sign flag := 0
         cge    r3,r1,r0
         bnz    r3,pflt1         % branch if value >= 0
         addi   r2,r0,1          % sign flag := 1
         sub    r1,r0,r1         % value := -value
pflt1    add    r3,r0,r0         % frac := value mod 100
         modi   r3,r1,100
         divi   r1,r1,100        % int_part := value / 100

% Build fractional digits (always 2) into buffer, low digit first
         addi   r4,r0,endbuf     % p := endbuf
         modi   r5,r3,10         % low digit of frac
         addi   r5,r5,48
         subi   r4,r4,1
         sb     0(r4),r5
         divi   r3,r3,10
         modi   r5,r3,10         % high digit of frac
         addi   r5,r5,48
         subi   r4,r4,1
         sb     0(r4),r5

% Store the decimal point
         addi   r5,r0,46         % '.'
         subi   r4,r4,1
         sb     0(r4),r5

% Build integer part digits (at least one digit)
pflt2    modi   r5,r1,10
         addi   r5,r5,48
         subi   r4,r4,1
         sb     0(r4),r5
         divi   r1,r1,10
         bnz    r1,pflt2         % loop while int_part != 0

% Prepend minus sign if needed
         bz     r2,pflt3
         addi   r5,r0,45         % '-'
         subi   r4,r4,1
         sb     0(r4),r5

% Print all characters from p to endbuf
pflt3    lb     r5,0(r4)
         putc   r5
         addi   r4,r4,1
         cgei   r5,r4,endbuf
         bz     r5,pflt3
         jr     r15              % return
)";
    }
} // namespace lang
