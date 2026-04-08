#pragma once

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "AST/ASTNode.hpp"
#include "SemanticAnalyzer/SemanticAnalyzer.hpp"

namespace lang
{
    struct FrameInfo {
        int frame_size = 0;
        std::unordered_map<std::string, int> offsets;
        bool is_member = false;
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
        std::unordered_map<std::string, std::string> m_globalLabels;

        std::vector<int> m_freeRegs;
        int allocReg();
        void freeReg(int r);

        void emit(const std::string &line);
        void emitData(const std::string &line);

        std::string newLabel(const std::string &hint = "L");


        int sizeOf(const std::string &type) const;
        int sizeOfClass(const SymbolTableNode *cls) const;
        bool isPointerType(const std::string &type) const;
        const SymbolTableNode *findClass(const std::string &name) const;
        const SymbolTableNode *findFreeFunction(const std::string &name) const;
        const SymbolTableNode *findMethod(const SymbolTableNode *cls, const std::string &name) const;
        int memberOffset(const SymbolTableNode *cls, const std::string &name) const;
        std::string getVarType(const std::string &name) const;

        FrameInfo computeFrameInfo(const SymbolTableNode *funcNode, bool isMember = false) const;
        std::unordered_map<std::string, std::string> allocateGlobals(const SymbolTableNode *mainNode);

        std::string functionLabel(const SymbolTableNode *funcNode, const SymbolTableNode *classNode = nullptr) const;


        int loadVar(const std::string &name);
        void storeVar(const std::string &name, int valueReg);
        int addrOfVar(const std::string &name);

        void generateProg(std::shared_ptr<const ASTNode> prog);
        void generateFuncDef(std::shared_ptr<const ASTNode> funcDef);

        void generateStatBlock(std::shared_ptr<const ASTNode> node);
        void generateStatement(std::shared_ptr<const ASTNode> node);
        void generateAssignStat(std::shared_ptr<const ASTNode> node);
        void generateIfStat(std::shared_ptr<const ASTNode> node);
        void generateWhileStat(std::shared_ptr<const ASTNode> node);
        void generatePutStat(std::shared_ptr<const ASTNode> node);
        void generateReadStat(std::shared_ptr<const ASTNode> node);
        void generateReturnStat(std::shared_ptr<const ASTNode> node);

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

        int generateLValue(std::shared_ptr<const ASTNode> node);
        int generateIndexedVarAddr(std::shared_ptr<const ASTNode> node);
        int generateMemberAccessAddr(std::shared_ptr<const ASTNode> node);

        int callFunction(
            const SymbolTableNode *funcNode, const std::vector<std::shared_ptr<ASTNode>> &args, const SymbolTableNode *classNode = nullptr,
            int selfAddrReg = -1);

        void appendIOHelpers(std::ostringstream &out);
    };

} // namespace lang
