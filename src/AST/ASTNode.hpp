#pragma once

#include <memory>
#include <string>
#include <vector>

namespace lang
{
    struct ASTNode {
        enum class Kind {
            Prog,
            ClassList,
            Class,
            InheritList,
            FuncDefList,
            FuncDef,
            FuncDecl,
            ProgramBlock,
            ParamList,
            StatBlock,
            VarDecl,
            DimList,
            AssignStat,
            PutStat,
            ReturnStat,
            IfStat,
            WhileStat,
            ReadStat,
            FuncCall,
            AddOp,
            MultOp,
            RelOp,
            NotExpr,
            SignExpr,
            IndexedVar,
            MemberAccess,
            Id,
            Type,
            Dim,
            Num,
            Marker,
        };

        Kind kind;
        std::string lexeme;
        std::vector<std::shared_ptr<ASTNode>> children;
        ASTNode *parent = nullptr;
    };

    using ASTNodePtr = std::shared_ptr<ASTNode>;
} // namespace lang
