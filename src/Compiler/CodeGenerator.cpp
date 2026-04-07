#include "CodeGenerator.hpp"

#include <cassert>
#include <format>
#include <sstream>
#include <stdexcept>

namespace lang {

// ============================================================
// Constructor
// ============================================================

CodeGenerator::CodeGenerator(std::shared_ptr<const ASTNode> ast,
                             const SymbolTableNode *globalTable)
    : m_ast(ast), m_globalTable(globalTable)
{
    for (int i = 12; i >= 1; --i) m_freeRegs.push_back(i);
}

// ============================================================
// Register allocator
// ============================================================

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

// ============================================================
// Emitters and label generation
// ============================================================

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

// ============================================================
// Size computation
// ============================================================

int CodeGenerator::sizeOf(const std::string &type) const
{
    size_t bracket = type.find('[');
    std::string base = (bracket != std::string::npos) ? type.substr(0, bracket) : type;

    if (base.empty()) return 4;

    int baseSize;
    if (base == "int" || base == "float" || base == "void") {
        baseSize = 4;
    } else {
        const SymbolTableNode *cls = findClass(base);
        baseSize = cls ? sizeOfClass(cls) : 4;
    }

    if (bracket == std::string::npos) return baseSize;

    // Parse dimension product: "int[5][3]" → 5*3 = 15 elements
    int product = 1;
    size_t pos = bracket;
    while (pos != std::string::npos) {
        size_t close = type.find(']', pos);
        std::string dimStr = type.substr(pos + 1, close - pos - 1);
        if (dimStr.empty()) {
            // Empty brackets: array pointer parameter → 4 bytes
            return 4;
        }
        product *= std::stoi(dimStr);
        pos = type.find('[', close);
    }
    return baseSize * product;
}

int CodeGenerator::sizeOfClass(const SymbolTableNode *cls) const
{
    if (!cls) return 0;
    int total = 0;
    // Inherited classes first
    for (auto *entry : cls->table) {
        if (entry->kind == SymbolTableNode::Kind::Inherit && entry->name != "none") {
            // "Base1, Base2, ..."
            std::string inherited = entry->name;
            size_t pos = 0;
            while (pos < inherited.size()) {
                size_t comma = inherited.find(',', pos);
                std::string baseName = (comma == std::string::npos)
                    ? inherited.substr(pos)
                    : inherited.substr(pos, comma - pos);
                // trim whitespace
                while (!baseName.empty() && baseName.front() == ' ') baseName.erase(0, 1);
                while (!baseName.empty() && baseName.back() == ' ') baseName.pop_back();
                const SymbolTableNode *base = findClass(baseName);
                if (base) total += sizeOfClass(base);
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
    if (bracket == std::string::npos) return false;
    size_t close = type.find(']', bracket);
    return (close == bracket + 1); // empty brackets
}

const SymbolTableNode *CodeGenerator::findClass(const std::string &name) const
{
    for (auto *entry : m_globalTable->table) {
        if (entry->kind == SymbolTableNode::Kind::Class && entry->name == name)
            return entry;
    }
    // Case-insensitive fallback
    std::string lower = name;
    for (char &c : lower) c = (char)std::tolower((unsigned char)c);
    for (auto *entry : m_globalTable->table) {
        if (entry->kind == SymbolTableNode::Kind::Class) {
            std::string eName = entry->name;
            for (char &c : eName) c = (char)std::tolower((unsigned char)c);
            if (eName == lower) return entry;
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
    // Case-insensitive fallback
    std::string lower = name;
    for (char &c : lower) c = (char)std::tolower((unsigned char)c);
    for (auto *entry : m_globalTable->table) {
        if (entry->kind == SymbolTableNode::Kind::Function) {
            std::string eName = entry->name;
            for (char &c : eName) c = (char)std::tolower((unsigned char)c);
            if (eName == lower) return entry;
        }
    }
    return nullptr;
}

const SymbolTableNode *CodeGenerator::findMethod(const SymbolTableNode *cls,
                                                   const std::string &name) const
{
    if (!cls) return nullptr;
    for (auto *entry : cls->table) {
        if (entry->kind == SymbolTableNode::Kind::Function && entry->name == name)
            return entry;
    }
    return nullptr;
}

int CodeGenerator::memberOffset(const SymbolTableNode *cls, const std::string &name) const
{
    if (!cls) return 0;
    int offset = 0;
    // Inherited classes first
    for (auto *entry : cls->table) {
        if (entry->kind == SymbolTableNode::Kind::Inherit && entry->name != "none") {
            std::string inherited = entry->name;
            size_t pos = 0;
            while (pos < inherited.size()) {
                size_t comma = inherited.find(',', pos);
                std::string baseName = (comma == std::string::npos)
                    ? inherited.substr(pos)
                    : inherited.substr(pos, comma - pos);
                while (!baseName.empty() && baseName.front() == ' ') baseName.erase(0, 1);
                while (!baseName.empty() && baseName.back() == ' ') baseName.pop_back();
                const SymbolTableNode *base = findClass(baseName);
                if (base) offset += sizeOfClass(base);
                pos = (comma == std::string::npos) ? inherited.size() : comma + 1;
            }
        }
    }
    for (auto *entry : cls->table) {
        if (entry->kind != SymbolTableNode::Kind::Data) continue;
        if (entry->name == name) return offset;
        offset += sizeOf(entry->signature.type);
    }
    return 0;
}

std::string CodeGenerator::getVarType(const std::string &name) const
{
    // Search current function's params/locals
    if (m_currentFuncNode) {
        for (auto *entry : m_currentFuncNode->table) {
            if ((entry->kind == SymbolTableNode::Kind::Parameter ||
                 entry->kind == SymbolTableNode::Kind::Local) &&
                entry->name == name)
                return entry->signature.type;
        }
    }
    // Search current class's data members
    if (m_currentClassNode) {
        for (auto *entry : m_currentClassNode->table) {
            if (entry->kind == SymbolTableNode::Kind::Data && entry->name == name)
                return entry->signature.type;
        }
    }
    // Search main globals
    auto it = m_globalLabels.find(name);
    if (it != m_globalLabels.end()) {
        // Find in main's symbol table
        for (auto *fentry : m_globalTable->table) {
            if (fentry->kind == SymbolTableNode::Kind::Function && fentry->name == "main") {
                for (auto *entry : fentry->table) {
                    if (entry->name == name) return entry->signature.type;
                }
            }
        }
    }
    return "";
}

// ============================================================
// Frame layout
// ============================================================

FrameInfo CodeGenerator::computeFrameInfo(const SymbolTableNode *funcNode,
                                           bool isMember) const
{
    FrameInfo fi;
    fi.isMember = isMember;
    int cursor = 0;

    // Link register slot
    cursor += 4;
    fi.offsets["__link"] = -cursor; // = -4

    // Self pointer for member functions
    if (isMember) {
        cursor += 4;
        fi.offsets["__self"] = -cursor; // = -8
    }

    // Parameters
    for (auto *entry : funcNode->table) {
        if (entry->kind != SymbolTableNode::Kind::Parameter) continue;
        int sz = sizeOf(entry->signature.type);
        if (sz <= 0) sz = 4;
        cursor += sz;
        fi.offsets[entry->name] = -cursor;
    }

    // Locals
    for (auto *entry : funcNode->table) {
        if (entry->kind != SymbolTableNode::Kind::Local) continue;
        int sz = sizeOf(entry->signature.type);
        if (sz <= 0) sz = 4;
        cursor += sz;
        fi.offsets[entry->name] = -cursor;
    }

    fi.frameSize = cursor;
    return fi;
}

std::unordered_map<std::string, std::string>
CodeGenerator::allocateGlobals(const SymbolTableNode *mainNode)
{
    std::unordered_map<std::string, std::string> labels;
    if (!mainNode) return labels;
    for (auto *entry : mainNode->table) {
        if (entry->kind != SymbolTableNode::Kind::Local) continue;
        std::string label = std::format("t{}_{}", 0, entry->name);
        int sz = sizeOf(entry->signature.type);
        if (sz <= 0) sz = 4;
        emitData(std::format("{:<12} res    {}   % {} {}", label, sz,
                              entry->signature.type, entry->name));
        labels[entry->name] = label;
    }
    return labels;
}

std::string CodeGenerator::functionLabel(const SymbolTableNode *funcNode,
                                          const SymbolTableNode *classNode) const
{
    if (classNode) {
        return classNode->name + "_" + funcNode->name;
    }
    return funcNode->name;
}

// ============================================================
// Variable access helpers
// ============================================================

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
    // Not found — emit 0
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

// ============================================================
// Top-level generation
// ============================================================

std::string CodeGenerator::generate()
{
    // Re-init register pool
    m_freeRegs.clear();
    for (int i = 12; i >= 1; --i) m_freeRegs.push_back(i);
    m_code.str("");
    m_data.str("");
    m_labelCounter = 0;

    generateProg(m_ast);

    std::ostringstream out;
    out << m_code.str() << "\n";

    // Data section: global variables
    out << "         align\n";
    out << m_data.str();
    out << "         align\n\n";

    appendIOHelpers(out);

    return out.str();
}

void CodeGenerator::generateProg(std::shared_ptr<const ASTNode> prog)
{
    // prog->children = [classList, funcDefList, programBlock]
    if (!prog || prog->children.size() < 3) return;

    // Find main symbol node
    const SymbolTableNode *mainNode = nullptr;
    for (auto *entry : m_globalTable->table) {
        if (entry->kind == SymbolTableNode::Kind::Function && entry->name == "main") {
            mainNode = entry;
            break;
        }
    }

    // Allocate global (main-block) variables as res directives
    if (mainNode) m_globalLabels = allocateGlobals(mainNode);

    // Entry and stack pointer init
    emit("         align");
    emit("         entry");
    emit("         addi   r14,r0,topaddr   % initialize stack pointer");
    emit("");

    // Generate main block
    m_currentFuncNode = mainNode;
    m_currentClassNode = nullptr;
    m_currentFrame = {}; // main uses label-based globals, no stack frame
    generateStatBlock(prog->children[2]);

    emit("         hlt");
    emit("");

    // Generate free function and member function bodies
    for (auto &funcDef : prog->children[1]->children) {
        generateFuncDef(funcDef);
    }
}

void CodeGenerator::generateFuncDef(std::shared_ptr<const ASTNode> funcDef)
{
    // FuncDef::children = [type, id_or_MemberAccess, paramList, statBlock]
    if (!funcDef || funcDef->children.size() < 4) return;

    auto &nameNode = funcDef->children[1];
    auto &statBlock = funcDef->children[3];

    const SymbolTableNode *funcSym = nullptr;
    const SymbolTableNode *classSym = nullptr;
    bool isMember = (nameNode->kind == ASTNode::Kind::MemberAccess);

    if (isMember) {
        // MemberAccess: children[0] = class name Id, children[1] = func name Id
        std::string className = nameNode->children[0]->lexeme;
        std::string funcName = nameNode->children[1]->lexeme;
        classSym = findClass(className);
        if (classSym) funcSym = findMethod(classSym, funcName);
    } else {
        funcSym = findFreeFunction(nameNode->lexeme);
    }

    if (!funcSym) return; // symbol not found; skip

    std::string label = functionLabel(funcSym, classSym);
    FrameInfo frame = computeFrameInfo(funcSym, isMember);

    emit(std::format("% ---- function: {} ----", label));
    emit(std::format("{:<12}sw     -4(r14),r15   % save link register", label));

    // Set context for body generation
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

    // Restore context
    m_currentFrame = savedFrame;
    m_currentFuncNode = savedFunc;
    m_currentClassNode = savedClass;
}

// ============================================================
// Statement generation
// ============================================================

void CodeGenerator::generateStatBlock(std::shared_ptr<const ASTNode> node)
{
    if (!node) return;
    for (auto &child : node->children) {
        if (!child) continue;
        if (child->kind == ASTNode::Kind::VarDecl) continue; // handled by allocateGlobals/computeFrameInfo
        generateStatement(child);
    }
}

void CodeGenerator::generateStatement(std::shared_ptr<const ASTNode> node)
{
    if (!node) return;
    switch (node->kind) {
        case ASTNode::Kind::AssignStat:  generateAssignStat(node); break;
        case ASTNode::Kind::IfStat:      generateIfStat(node);     break;
        case ASTNode::Kind::WhileStat:   generateWhileStat(node);  break;
        case ASTNode::Kind::PutStat:     generatePutStat(node);    break;
        case ASTNode::Kind::ReadStat:    generateReadStat(node);   break;
        case ASTNode::Kind::ReturnStat:  generateReturnStat(node); break;
        case ASTNode::Kind::FuncCall:
            // Standalone function call: generate and discard result
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
    // AssignStat::children = [lhs, rhs]
    if (!node || node->children.size() < 2) return;

    int rhsReg = generateExpr(node->children[1]);

    auto &lhs = node->children[0];
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
    // IfStat::children = [cond, thenBlock, elseBlock]
    if (!node || node->children.size() < 3) return;

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
    // WhileStat::children = [cond, body]
    if (!node || node->children.size() < 2) return;

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
    // PutStat::children = [expr]
    if (!node || node->children.empty()) return;

    int valReg = generateExpr(node->children[0]);
    if (valReg != 1) {
        emit(std::format("         add    r1,r{},r0   % move to r1 for putint", valReg));
        freeReg(valReg);
    }
    // Save/restore r14 context around putint if needed (putint uses r1-r5, r15)
    emit("         jl     r15,putint   % write integer");
    if (valReg == 1) freeReg(1);
    // Emit newline character
    emit("         addi   r1,r0,10");
    emit("         putc   r1           % newline");
}

void CodeGenerator::generateReadStat(std::shared_ptr<const ASTNode> node)
{
    // ReadStat::children = [variable]
    if (!node || node->children.empty()) return;

    emit("         jl     r15,getint   % read integer → r1");

    auto &var = node->children[0];
    if (var->kind == ASTNode::Kind::Id) {
        // r1 holds the result; storeVar using r1
        // We need to use r1 directly without going through allocReg
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
        // Complex lvalue (indexed var, member access)
        // Temporarily borrow r2 for the address since r1 has the value
        int addrReg = allocReg();
        // But we need to not use r1... we'll save r1, compute addr, store, restore
        emit(std::format("         add    r{},r1,r0   % save getint result", addrReg));
        int addr2 = generateLValue(var);
        emit(std::format("         sw     0(r{}),r{}", addr2, addrReg));
        freeReg(addr2);
        freeReg(addrReg);
    }
}

void CodeGenerator::generateReturnStat(std::shared_ptr<const ASTNode> node)
{
    // ReturnStat::children = [expr]
    if (!node || node->children.empty()) {
        emit("         add    r13,r0,r0   % return void");
    } else {
        int valReg = generateExpr(node->children[0]);
        emit(std::format("         add    r13,r{},r0   % set return value", valReg));
        freeReg(valReg);
    }
    emit("         lw     r15,-4(r14)   % restore link");
    emit("         jr     r15           % return");
}

// ============================================================
// Expression generation
// ============================================================

int CodeGenerator::generateExpr(std::shared_ptr<const ASTNode> node)
{
    if (!node) {
        int r = allocReg();
        emit(std::format("         add    r{},r0,r0   % null expr", r));
        return r;
    }
    switch (node->kind) {
        case ASTNode::Kind::AddOp:    return generateBinaryOp(node, "");
        case ASTNode::Kind::MultOp:   return generateBinaryOp(node, "");
        case ASTNode::Kind::RelOp:    return generateRelOp(node);
        case ASTNode::Kind::NotExpr:  return generateNotExpr(node);
        case ASTNode::Kind::SignExpr: return generateSignExpr(node);
        case ASTNode::Kind::Num:      return generateNum(node);
        case ASTNode::Kind::Id:       return generateIdExpr(node);
        case ASTNode::Kind::FuncCall: return generateFuncCallExpr(node);
        case ASTNode::Kind::IndexedVar:   return generateIndexedVarExpr(node);
        case ASTNode::Kind::MemberAccess: return generateMemberAccessExpr(node);
        default: {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0   % unknown expr", r));
            return r;
        }
    }
}

int CodeGenerator::generateBinaryOp(std::shared_ptr<const ASTNode> node,
                                     const std::string & /*unused*/)
{
    // AddOp/MultOp::lexeme = "+", "-", "or", "*", "/", "and"
    // children = [left, right]
    if (!node || node->children.size() < 2) {
        int r = allocReg();
        emit(std::format("         add    r{},r0,r0", r));
        return r;
    }
    int lReg = generateExpr(node->children[0]);
    int rReg = generateExpr(node->children[1]);
    int res = allocReg();

    const std::string &op = node->lexeme;
    if (op == "+") {
        emit(std::format("         add    r{},r{},r{}", res, lReg, rReg));
    } else if (op == "-") {
        emit(std::format("         sub    r{},r{},r{}", res, lReg, rReg));
    } else if (op == "or") {
        emit(std::format("         or     r{},r{},r{}", res, lReg, rReg));
    } else if (op == "*") {
        emit(std::format("         mul    r{},r{},r{}", res, lReg, rReg));
    } else if (op == "/") {
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
    // RelOp::lexeme = "==", "!=", "<", ">", "<=", ">="
    // children = [left, right]
    if (!node || node->children.size() < 2) {
        int r = allocReg();
        emit(std::format("         add    r{},r0,r0", r));
        return r;
    }
    int lReg = generateExpr(node->children[0]);
    int rReg = generateExpr(node->children[1]);
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
    // not expr → logical NOT: result = (expr == 0)
    int operand = generateExpr(node->children[0]);
    int res = allocReg();
    emit(std::format("         ceqi   r{},r{},0   % logical not", res, operand));
    freeReg(operand);
    return res;
}

int CodeGenerator::generateSignExpr(std::shared_ptr<const ASTNode> node)
{
    // SignExpr::lexeme = "+" or "-"
    int operand = generateExpr(node->children[0]);
    if (node->lexeme == "-") {
        int res = allocReg();
        emit(std::format("         sub    r{},r0,r{}   % negate", res, operand));
        freeReg(operand);
        return res;
    }
    return operand; // unary + is a no-op
}

int CodeGenerator::generateNum(std::shared_ptr<const ASTNode> node)
{
    int r = allocReg();
    const std::string &lex = node->lexeme;
    if (lex.find('.') != std::string::npos) {
        // Float literal: truncate to integer for now
        int val = static_cast<int>(std::stof(lex));
        emit(std::format("         addi   r{},r0,{}   % float literal (truncated)", r, val));
    } else {
        emit(std::format("         addi   r{},r0,{}   % integer literal", r, lex));
    }
    return r;
}

int CodeGenerator::generateIdExpr(std::shared_ptr<const ASTNode> node)
{
    // Check if this is a local array (not pointer) — load value or address?
    // For plain scalar access: load value
    // For local array (non-pointer param) appearing in non-indexed context: load first element
    // In practice, arrays without [] only appear as function args → handled by callFunction
    return loadVar(node->lexeme);
}

int CodeGenerator::generateFuncCallExpr(std::shared_ptr<const ASTNode> node)
{
    // FuncCall::children = [Id_or_MemberAccess, ParamList]
    if (!node || node->children.size() < 2) {
        int r = allocReg();
        emit(std::format("         add    r{},r0,r0   % null func call", r));
        return r;
    }

    auto &calleeNode = node->children[0];
    auto &paramListNode = node->children[1];

    if (calleeNode->kind == ASTNode::Kind::Id) {
        // Free function call
        const SymbolTableNode *funcSym = findFreeFunction(calleeNode->lexeme);
        if (!funcSym) {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0   % unknown func {}", r, calleeNode->lexeme));
            return r;
        }
        return callFunction(funcSym, paramListNode->children, nullptr, -1);
    } else if (calleeNode->kind == ASTNode::Kind::MemberAccess) {
        // Method call: calleeNode->children[0] = object, [1] = method name
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
    // IndexedVar: load from computed address
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

// ============================================================
// L-value address computation
// ============================================================

int CodeGenerator::generateLValue(std::shared_ptr<const ASTNode> node)
{
    if (!node) return allocReg();
    switch (node->kind) {
        case ASTNode::Kind::Id:
            return addrOfVar(node->lexeme);
        case ASTNode::Kind::IndexedVar:
            return generateIndexedVarAddr(node);
        case ASTNode::Kind::MemberAccess:
            return generateMemberAccessAddr(node);
        default: {
            int r = allocReg();
            emit(std::format("         add    r{},r0,r0   % lvalue unknown kind", r));
            return r;
        }
    }
}

int CodeGenerator::generateIndexedVarAddr(std::shared_ptr<const ASTNode> node)
{
    // IndexedVar::children = [base, indexExpr]
    // Base can be: Id, or another IndexedVar (multi-dimensional chaining)
    if (!node || node->children.size() < 2) return allocReg();

    auto &baseNode = node->children[0];
    auto &indexNode = node->children[1];

    int baseReg;
    int elemSize = 4; // default: int/float element

    if (baseNode->kind == ASTNode::Kind::Id) {
        const std::string &arrName = baseNode->lexeme;
        std::string varType = getVarType(arrName);

        // Determine element size from type (strip outermost dimension)
        if (!varType.empty()) {
            // "int[5][3]" → element type is "int[3]", size = 4*3 = 12
            // "int[5]"   → element type is "int",    size = 4
            size_t bracket = varType.find('[');
            if (bracket != std::string::npos) {
                size_t close = varType.find(']', bracket);
                // Strip first dimension: get the remaining type
                std::string elemType = varType.substr(0, bracket) + varType.substr(close + 1);
                elemSize = sizeOf(elemType.empty() ? varType.substr(0, bracket) : elemType);
                if (elemSize <= 0) elemSize = 4;
            }
        }

        // Is it a pointer parameter?
        bool isPointer = isPointerType(varType);
        if (isPointer) {
            // Load the stored pointer value
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
            // Compute base address of array
            baseReg = addrOfVar(arrName);
        }
    } else {
        // Chained IndexedVar: base is itself an IndexedVar (multi-dim array)
        // The inner IndexedVar gives the address of a sub-array
        baseReg = generateIndexedVarAddr(baseNode);
        // elemSize for the outer indexing...
        // For now, use 4 (approximate for nested arrays)
        elemSize = 4;
    }

    // Compute index
    int idxReg = generateExpr(indexNode);

    // Compute offset = index * elemSize
    int offReg = allocReg();
    if (elemSize == 4) {
        emit(std::format("         muli   r{},r{},4   % offset = index * 4", offReg, idxReg));
    } else {
        emit(std::format("         muli   r{},r{},{}   % offset = index * elemSize", offReg, idxReg, elemSize));
    }
    freeReg(idxReg);

    // addr = base + offset
    emit(std::format("         add    r{},r{},r{}   % element address", baseReg, baseReg, offReg));
    freeReg(offReg);

    return baseReg;
}

int CodeGenerator::generateMemberAccessAddr(std::shared_ptr<const ASTNode> node)
{
    // MemberAccess::children = [object, member_name_Id]
    if (!node || node->children.size() < 2) return allocReg();

    auto &objNode = node->children[0];
    auto &memberNode = node->children[1];

    // Get base address of object
    int objAddrReg;
    std::string objType;

    if (objNode->kind == ASTNode::Kind::Id) {
        objType = getVarType(objNode->lexeme);
        objAddrReg = addrOfVar(objNode->lexeme);
    } else {
        // Chained member access or other expression
        objAddrReg = generateLValue(objNode);
    }

    // Find the class and member offset
    const SymbolTableNode *cls = findClass(objType);
    int offset = cls ? memberOffset(cls, memberNode->lexeme) : 0;

    if (offset == 0) return objAddrReg;

    int addrReg = allocReg();
    emit(std::format("         addi   r{},r{},{}   % member offset", addrReg, objAddrReg, offset));
    freeReg(objAddrReg);
    return addrReg;
}

// ============================================================
// Function call
// ============================================================

int CodeGenerator::callFunction(const SymbolTableNode *funcNode,
                                 const std::vector<std::shared_ptr<ASTNode>> &args,
                                 const SymbolTableNode *classNode,
                                 int selfAddrReg)
{
    if (!funcNode) {
        int r = allocReg();
        emit(std::format("         add    r{},r0,r0   % null func call", r));
        return r;
    }

    bool isMember = (classNode != nullptr);
    FrameInfo calleeFrame = computeFrameInfo(funcNode, isMember);

    // Collect callee's params in order
    std::vector<const SymbolTableNode *> params;
    for (auto *entry : funcNode->table) {
        if (entry->kind == SymbolTableNode::Kind::Parameter) params.push_back(entry);
    }

    // Evaluate all arguments into registers (BEFORE adjusting r14)
    std::vector<int> argRegs;
    for (size_t i = 0; i < args.size(); i++) {
        bool passAsPointer = (i < params.size()) && isPointerType(params[i]->signature.type);
        int reg = passAsPointer ? generateLValue(args[i]) : generateExpr(args[i]);
        argRegs.push_back(reg);
    }

    // Place self pointer if member call
    if (isMember && selfAddrReg >= 0) {
        auto selfIt = calleeFrame.offsets.find("__self");
        if (selfIt != calleeFrame.offsets.end()) {
            int placementOffset = selfIt->second - m_currentFrame.frameSize;
            emit(std::format("         sw     {}(r14),r{}   % pass self pointer", placementOffset, selfAddrReg));
        }
    }

    // Place args: callee's param at offset OP_callee → place at (OP_callee - current_frame_size)(r14)
    for (size_t i = 0; i < argRegs.size() && i < params.size(); i++) {
        auto pit = calleeFrame.offsets.find(params[i]->name);
        if (pit != calleeFrame.offsets.end()) {
            int placementOffset = pit->second - m_currentFrame.frameSize;
            emit(std::format("         sw     {}(r14),r{}   % pass arg '{}'",
                              placementOffset, argRegs[i], params[i]->name));
        }
    }

    for (int r : argRegs) freeReg(r);

    // Adjust r14 (allocate callee's frame by moving r14 down)
    if (m_currentFrame.frameSize > 0) {
        emit(std::format("         subi   r14,r14,{}", m_currentFrame.frameSize));
    }

    // Call
    emit(std::format("         jl     r15,{}   % call {}", functionLabel(funcNode, classNode), funcNode->name));

    // Restore r14
    if (m_currentFrame.frameSize > 0) {
        emit(std::format("         addi   r14,r14,{}", m_currentFrame.frameSize));
    }

    // Copy return value from r13
    int resultReg = allocReg();
    emit(std::format("         add    r{},r13,r0   % copy return value", resultReg));
    return resultReg;
}

// ============================================================
// I/O helpers (putint / getint from newlib.m)
// ============================================================

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
)";
}

} // namespace lang
