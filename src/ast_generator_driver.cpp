#include <fstream>
#include <functional>
#include <string>

#include "AST/ASTNode.hpp"
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

namespace lang
{
    inline std::string to_string(ASTNode::Kind kind)
    {
        switch (kind) {
            case ASTNode::Kind::Prog:
                return "Prog";
            case ASTNode::Kind::ClassList:
                return "ClassList";
            case ASTNode::Kind::Class:
                return "Class";
            case ASTNode::Kind::FuncDefList:
                return "FuncDefList";
            case ASTNode::Kind::FuncDef:
                return "FuncDef";
            case ASTNode::Kind::ProgramBlock:
                return "ProgramBlock";
            case ASTNode::Kind::ParamList:
                return "ParamList";
            case ASTNode::Kind::StatBlock:
                return "StatBlock";
            case ASTNode::Kind::VarDecl:
                return "VarDecl";
            case ASTNode::Kind::DimList:
                return "DimList";
            case ASTNode::Kind::AssignStat:
                return "AssignStat";
            case ASTNode::Kind::PutStat:
                return "PutStat";
            case ASTNode::Kind::ReturnStat:
                return "ReturnStat";
            case ASTNode::Kind::IfStat:
                return "IfStat";
            case ASTNode::Kind::WhileStat:
                return "WhileStat";
            case ASTNode::Kind::ReadStat:
                return "ReadStat";
            case ASTNode::Kind::FuncCall:
                return "FuncCall";
            case ASTNode::Kind::AddOp:
                return "AddOp";
            case ASTNode::Kind::MultOp:
                return "MultOp";
            case ASTNode::Kind::RelOp:
                return "RelOp";
            case ASTNode::Kind::NotExpr:
                return "NotExpr";
            case ASTNode::Kind::SignExpr:
                return "SignExpr";
            case ASTNode::Kind::IndexedVar:
                return "IndexedVar";
            case ASTNode::Kind::MemberAccess:
                return "MemberAccess";
            case ASTNode::Kind::Id:
                return "Id";
            case ASTNode::Kind::Type:
                return "Type";
            case ASTNode::Kind::Dim:
                return "Dim";
            case ASTNode::Kind::Num:
                return "Num";
            case ASTNode::Kind::Marker:
                return "Marker";
        }
        return "?";
    }
} // namespace lang

static void writeDotAST(const lang::ASTNodePtr &root, std::ostream &out)
{
    out << "digraph AST {\n";
    out << "node [shape=record];\n";
    out << " node [fontname=Sans];charset=\"UTF-8\" splines=true splines=spline rankdir =LR\n";

    std::uint64_t counter = 0;
    std::function<void(const lang::ASTNodePtr &)> assign;
    std::unordered_map<lang::ASTNode *, std::uint64_t> ids;

    assign = [&](const lang::ASTNodePtr &n) {
        if (!n)
            return;
        ids[n.get()] = counter++;
        for (auto &c : n->children) assign(c);
    };
    assign(root);

    std::function<void(const lang::ASTNodePtr &)> emit;
    emit = [&](const lang::ASTNodePtr &n) {
        if (!n)
            return;
        auto id = ids[n.get()];
        std::string label = lang::to_string(n->kind);
        if (!n->lexeme.empty())
            label += " | \\" + n->lexeme;
        out << id << "[label=\"" << label << "\"];\n";

        if (n->children.empty()) {
            if (n->kind == lang::ASTNode::Kind::DimList) {
                out << "none" << id << "[shape=point];\n";
                out << id << "->none" << id << ";\n";
            }
        } else {
            for (auto &c : n->children) {
                out << id << "->" << ids[c.get()] << ";\n";
                emit(c);
            }
        }
    };
    emit(root);

    out << "}\n";
}

int main(int argc, char **argv)
{
    spdlog::set_pattern("[%^%-7l%$] %v");

    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }

    lang::SyntacticAnalyzer syntacticAnalyzer(true);

    for (std::uint64_t idx = 1; idx != static_cast<std::uint64_t>(argc); idx++) {
        syntacticAnalyzer.openFile(argv[idx]);
        syntacticAnalyzer.parse();

        auto ast = syntacticAnalyzer.getAST();
        if (!ast) {
            spdlog::warn("{}: No AST produced (parse errors?)", argv[idx]);
            continue;
        }

        std::string dotPath = std::string(argv[idx]) + ".dot.outast";
        std::ofstream dotFile(dotPath);
        if (!dotFile.is_open()) {
            spdlog::error("Failed to open DOT output file: {}", dotPath);
        } else {
            writeDotAST(ast, dotFile);
            spdlog::info("{}: DOT AST written to {}", argv[idx], dotPath);
        }
    }

    return 0;
}
