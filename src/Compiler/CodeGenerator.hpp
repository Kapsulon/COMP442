#pragma once

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST/ASTNode.hpp"
#include "SemanticAnalyzer/SemanticAnalyzer.hpp"

namespace lang {

// Per-function stack frame layout.
// After callee does: sub r14, r14, frameSize
//   offset 0             : saved link register (R15)
//   offset 4             : self pointer (member functions only)
//   offset 4 or 8        : parameter[0]
//   ...
//   offset after params  : local[0]
//   ...
struct FrameInfo {
    int frameSize = 0;
    std::unordered_map<std::string, int> offsets; // name → byte offset from R14
    bool isMember = false;
};

class CodeGenerator
{
public:
    CodeGenerator(std::shared_ptr<const ASTNode> ast, const SymbolTableNode *globalTable);
    std::string generate();

private:
    std::shared_ptr<const ASTNode> m_ast;
    const SymbolTableNode *m_globalTable;

    std::ostringstream m_code;
    std::ostringstream m_data;
    int m_labelCounter = 0;

    FrameInfo m_currentFrame;
    const SymbolTableNode *m_currentFuncNode = nullptr;
    const SymbolTableNode *m_currentClassNode = nullptr;
    std::unordered_map<std::string, std::string> m_globalLabels; // varName → data label

    // Register free list: r1-r12
    std::vector<int> m_freeRegs;
    int allocReg();
    void freeReg(int r);

    // Emitters
    void emit(const std::string &line);
    void emitData(const std::string &line);

    // Label generation
    std::string newLabel(const std::string &hint = "L");

    // ---- Size / layout ------------------------------------------------
    int sizeOf(const std::string &type) const;
    int sizeOfClass(const SymbolTableNode *cls) const;
    bool isPointerType(const std::string &type) const; // "int[]" → true
    const SymbolTableNode *findClass(const std::string &name) const;
    const SymbolTableNode *findFreeFunction(const std::string &name) const;
    const SymbolTableNode *findMethod(const SymbolTableNode *cls,
                                      const std::string &name) const;
    int memberOffset(const SymbolTableNode *cls, const std::string &name) const;
    std::string getVarType(const std::string &name) const;

    FrameInfo computeFrameInfo(const SymbolTableNode *funcNode, bool isMember = false) const;
    std::unordered_map<std::string, std::string> allocateGlobals(const SymbolTableNode *mainNode);

    std::string functionLabel(const SymbolTableNode *funcNode,
                               const SymbolTableNode *classNode = nullptr) const;

    // ---- Variable access helpers --------------------------------------
    // Returns register holding the loaded value; caller must freeReg.
    int loadVar(const std::string &name);
    // Stores valueReg into the named variable.
    void storeVar(const std::string &name, int valueReg);
    // Returns register holding the address of the named variable; caller must freeReg.
    int addrOfVar(const std::string &name);

    // ---- Top-level code gen ------------------------------------------
    void generateProg(std::shared_ptr<const ASTNode> prog);
    void generateFuncDef(std::shared_ptr<const ASTNode> funcDef);

    // ---- Statement gen -----------------------------------------------
    void generateStatBlock(std::shared_ptr<const ASTNode> node);
    void generateStatement(std::shared_ptr<const ASTNode> node);
    void generateAssignStat(std::shared_ptr<const ASTNode> node);
    void generateIfStat(std::shared_ptr<const ASTNode> node);
    void generateWhileStat(std::shared_ptr<const ASTNode> node);
    void generatePutStat(std::shared_ptr<const ASTNode> node);
    void generateReadStat(std::shared_ptr<const ASTNode> node);
    void generateReturnStat(std::shared_ptr<const ASTNode> node);

    // ---- Expression gen (returns reg holding value; caller frees) ----
    int generateExpr(std::shared_ptr<const ASTNode> node);
    int generateBinaryOp(std::shared_ptr<const ASTNode> node, const std::string &moonInstr);
    int generateRelOp(std::shared_ptr<const ASTNode> node);
    int generateNotExpr(std::shared_ptr<const ASTNode> node);
    int generateSignExpr(std::shared_ptr<const ASTNode> node);
    int generateNum(std::shared_ptr<const ASTNode> node);
    int generateIdExpr(std::shared_ptr<const ASTNode> node);
    int generateFuncCallExpr(std::shared_ptr<const ASTNode> node);
    int generateIndexedVarExpr(std::shared_ptr<const ASTNode> node);
    int generateMemberAccessExpr(std::shared_ptr<const ASTNode> node);

    // ---- L-value address (returns reg holding addr; caller frees) ----
    int generateLValue(std::shared_ptr<const ASTNode> node);
    int generateIndexedVarAddr(std::shared_ptr<const ASTNode> node);
    int generateMemberAccessAddr(std::shared_ptr<const ASTNode> node);

    // ---- Function call -----------------------------------------------
    // Generates a call, stores result in R13, returns a new reg with R13's value.
    int callFunction(const SymbolTableNode *funcNode,
                     const std::vector<std::shared_ptr<ASTNode>> &args,
                     const SymbolTableNode *classNode = nullptr,
                     int selfAddrReg = -1);

    // ---- I/O ---------------------------------------------------------
    void appendIOHelpers(std::ostringstream &out);
};

} // namespace lang
