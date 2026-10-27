#ifndef VISITOR_H
#define VISITOR_H

#include "ast.h"
#include "type.h"

// =========================================================================
// Interfaz Abstracta Visitor
// Expresiones devuelven Type; Sentencias, Declaraciones y Programa devuelven void.
// =========================================================================
class Visitor {
public:
    virtual ~Visitor() = default;

    // Expresiones
    virtual Type visit(BinaryExp* exp) = 0;
    virtual Type visit(UnaryExp* exp) = 0;
    virtual Type visit(AssignExp* exp) = 0;
    virtual Type visit(NumberExp* exp) = 0;
    virtual Type visit(RealExp* exp) = 0;
    virtual Type visit(BoolExp* exp) = 0;
    virtual Type visit(CharExp* exp) = 0;
    virtual Type visit(StringExp* exp) = 0;
    virtual Type visit(NullptrExp* exp) = 0;
    virtual Type visit(IdExp* exp) = 0;
    virtual Type visit(PostfixExp* exp) = 0;
    virtual Type visit(NewExp* exp) = 0;
    virtual Type visit(DeleteExp* exp) = 0;
    virtual Type visit(BracedInitList* exp) = 0;

    // Sentencias
    virtual void visit(BlockStmt* stm) = 0;
    virtual void visit(ExprStmt* stm) = 0;
    virtual void visit(IfStmt* stm) = 0;
    virtual void visit(WhileStmt* stm) = 0;
    virtual void visit(ForStmt* stm) = 0;
    virtual void visit(DoWhileStmt* stm) = 0;
    virtual void visit(BreakStmt* stm) = 0;
    virtual void visit(ContinueStmt* stm) = 0;
    virtual void visit(ReturnStmt* stm) = 0;

    // Declaraciones y Directivas
    virtual void visit(VarInit* init) = 0;
    virtual void visit(VarDecl* decl) = 0;
    virtual void visit(Param* param) = 0;
    virtual void visit(FunDecl* fun) = 0;
    virtual void visit(RecordMember* member) = 0;
    virtual void visit(RecordDecl* record) = 0;
    virtual void visit(IncludeDirective* inc) = 0;
    virtual void visit(UsingDirective* usg) = 0;
    virtual void visit(Program* prog) = 0;
};

#endif // VISITOR_H
