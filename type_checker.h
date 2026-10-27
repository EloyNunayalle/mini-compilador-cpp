#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "visitor.h"
#include "ast.h"
#include "type.h"
#include "environment.h"

using namespace std;

// Visitor de análisis semántico y comprobación de tipos
// Diseñado siguiendo el patrón TypeChecker de la Semana 11 del curso
class TypeChecker : public Visitor {
public:
    Environment<Type> env;                       // Tabla de símbolos con scopes jerárquicos
    unordered_map<string, FunDecl*> functions;  // Firmas de funciones
    unordered_map<string, RecordDecl*> records;  // Tipos struct / class registrados
    FunDecl* currentFunction;                   // Función actualmente en verificación
    int loopDepth;                              // Profundidad de bucles (para break/continue)

    vector<string> errors;                      // Lista de errores semánticos detectados
    vector<string> warnings;                    // Lista de advertencias semánticas

    TypeChecker();

    void check(Program* prog);
    bool hasErrors() const;
    void printReport(ostream& out = cout) const;

    void addError(const string& msg, int line = 0, int col = 0);
    void addWarning(const string& msg, int line = 0, int col = 0);

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

#endif // TYPE_CHECKER_H
