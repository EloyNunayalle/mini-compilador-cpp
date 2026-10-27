#ifndef PRINT_VISITOR_H
#define PRINT_VISITOR_H

#include <iostream>
#include <string>
#include <vector>
#include "visitor.h"
#include "ast.h"

using namespace std;

// Visitor para imprimir y visualizar el AST con sangría jerárquica
class ASTPrinter : public Visitor {
private:
    ostream& out;
    int indentLevel;
    vector<bool> isLastStack;

    void printIndent();
    void pushNode(bool isLast);
    void popNode();

public:
    ASTPrinter(ostream& out = cout);
    void print(ASTNode* node);

    // Expresiones
    Type visit(BinaryExp* exp) override;
    Type visit(UnaryExp* exp) override;
    Type visit(AssignExp* exp) override;
    Type visit(NumberExp* exp) override;
    Type visit(RealExp* exp) override;
    Type visit(BoolExp* exp) override;
    Type visit(CharExp* exp) override;
    Type visit(StringExp* exp) override;
    Type visit(NullptrExp* exp) override;
    Type visit(IdExp* exp) override;
    Type visit(PostfixExp* exp) override;
    Type visit(NewExp* exp) override;
    Type visit(DeleteExp* exp) override;
    Type visit(BracedInitList* exp) override;

    // Sentencias
    void visit(BlockStmt* stm) override;
    void visit(ExprStmt* stm) override;
    void visit(IfStmt* stm) override;
    void visit(WhileStmt* stm) override;
    void visit(ForStmt* stm) override;
    void visit(DoWhileStmt* stm) override;
    void visit(BreakStmt* stm) override;
    void visit(ContinueStmt* stm) override;
    void visit(ReturnStmt* stm) override;

    // Declaraciones y directivas
    void visit(VarInit* init) override;
    void visit(VarDecl* decl) override;
    void visit(Param* param) override;
    void visit(FunDecl* fun) override;
    void visit(RecordMember* member) override;
    void visit(RecordDecl* record) override;
    void visit(IncludeDirective* inc) override;
    void visit(UsingDirective* usg) override;
    void visit(Program* prog) override;
};

#endif // PRINT_VISITOR_H
