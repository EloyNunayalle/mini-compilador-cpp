#ifndef GENCODE_H
#define GENCODE_H

#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "visitor.h"
#include "ast.h"
#include "layout.h"

using namespace std;

// Generador de ensamblador GNU AS (AT&T) para Linux x86-64, ABI System V.
// Sin optimizaciones. Lee las anotaciones del TypeChecker y no recalcula tipos ni layout.
//
// Convencion de resultados de expresiones:
//   int, char, bool: %eax (char con signo extendido, bool 0 o 1)
//   punteros y direcciones de arreglos, records y strings: %rax
//   float y double: %xmm0
// El Type que devuelve cada visit es solo la anotacion exp->inferredType.
class Gencode : public Visitor {
public:
    Gencode(const unordered_map<string, RecordDecl*>& records);

    void generate(Program* prog, ostream& out);

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

private:
    enum Cls { C_INT, C_FLOAT, C_DOUBLE, C_PTR, C_AGG, C_NONE };

    enum ArgKind { AK_VALUE, AK_THIS_ADDR, AK_THIS_PTR, AK_THIS_IMPLICIT };
    struct ArgSpec {
        ArgKind kind;
        Exp* exp;
        Type ptype;
        bool byAddr;
    };

    const unordered_map<string, RecordDecl*>& records;

    ostringstream text;
    ostringstream initCode;
    ostringstream bss;
    ostringstream rodata;
    ostream* cur;

    FunDecl* curFun;
    string endLabel;
    int pushDepth;                 // temporales de 8 bytes pendientes en la pila
    int labelCount;
    int constCount;
    vector<string> breakLabels;
    vector<string> continueLabels;
    unordered_map<string, string> stringLabels;

    // emision
    void emit(const string& s);
    void label(const string& name);
    string newLabel(const string& hint);
    string internString(const string& s);
    string escapeString(const string& s) const;

    // tipos de maquina
    static Cls cls(const Type& t);
    static int accessSize(const Type& t);
    string varMem(const VarInfo& v, int extra = 0) const;
    int thisOffset() const;

    // movimiento de datos
    void loadFrom(const Type& t, const string& mem);
    void storeTo(const Type& t, const string& mem);
    void pushTemp(const Type& t);
    void popToResult(const Type& t);
    void convert(const Type& from, const Type& to);
    void copyBytes(const string& src, const string& dst, int size);
    void emitCall(const string& target);

    // expresiones
    void genAddr(Exp* e);
    void genBool(Exp* e);
    void genLogic(BinaryExp* e);
    void genCompare(BinaryExp* e);
    void genArith(BinaryExp* e);
    void genStrCompare(BinaryExp* e);
    void genCout(BinaryExp* e);
    void genCin(BinaryExp* e);
    void genIncDec(Exp* target, bool inc, bool post, int ptrScale);
    void genAssignString(AssignExp* e);

    // llamadas
    void genCallExp(PostfixExp* call);
    void genCallArgs(vector<ArgSpec>& args, const string& target);
    void genBuiltin(PostfixExp* call);
    void callRuntime(const string& fn);

    // declaraciones
    void genFunction(FunDecl* fun);
    void genVarInit(VarInit* vi);
    void genInit(const Type& t, Exp* init, const VarInfo& v, int extra, bool top);
    void genGlobals(VarDecl* decl);
    vector<VarInfo> globals;
};

#endif // GENCODE_H
