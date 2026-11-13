#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "visitor.h"
#include "ast.h"
#include "type_checker.h"

using namespace std;

// Pases de optimizacion sobre el AST. Son opcionales (solo con -O1), viven fuera del
// Gencode y solo aplican transformaciones que conservan el comportamiento observable.
// Despues de cada pase el AST se vuelve a verificar, asi que ninguna anotacion
// (tipos, offsets, ABI, constructores) queda desactualizada.

struct PassResult {
    string name;
    int count = 0;
    vector<string> details;   // transformaciones aplicadas y rechazos relevantes
};

// Analisis comunes
namespace opt {
    // Los hijos de una expresion, como direcciones de los punteros para poder reemplazarlos
    void childSlots(Exp* e, vector<Exp**>& out);
    // La expresion no tiene efectos secundarios ni puede fallar en ejecucion
    bool isPure(const Exp* e);
    bool isPlainInt(const Type& t);
    bool isLiteralInt(const Exp* e, long long& v);
}

// Recorrido que permite reemplazar expresiones: post() se invoca con cada expresion
// despues de reescribir sus hijos y devuelve el nodo que ocupa su lugar.
class RewriteVisitor : public Visitor {
public:
    void run(Program* prog);

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

    void visit(BlockStmt* stm) override;
    void visit(ExprStmt* stm) override;
    void visit(IfStmt* stm) override;
    void visit(WhileStmt* stm) override;
    void visit(ForStmt* stm) override;
    void visit(DoWhileStmt* stm) override;
    void visit(BreakStmt* stm) override;
    void visit(ContinueStmt* stm) override;
    void visit(ReturnStmt* stm) override;

    void visit(VarInit* init) override;
    void visit(VarDecl* decl) override;
    void visit(Param* param) override;
    void visit(FunDecl* fun) override;
    void visit(RecordMember* member) override;
    void visit(RecordDecl* record) override;
    void visit(IncludeDirective* inc) override;
    void visit(UsingDirective* usg) override;
    void visit(Program* prog) override;

protected:
    virtual Exp* post(Exp* e) { return e; }
    // Los constructores, destructores y asignaciones no se optimizan
    virtual bool wantsFunction(const FunDecl* f) const { return f->funKind == FunKind::NORMAL && f->body; }
    Exp* rw(Exp* e);
    void rwSlot(Exp*& slot);
    Program* program = nullptr;
};

// Plegado de constantes enteras, identidades algebraicas y reduccion de fuerza
class FoldVisitor : public RewriteVisitor {
public:
    PassResult result;
    FoldVisitor() { result.name = "fold"; }
protected:
    Exp* post(Exp* e) override;
private:
    Exp* foldBinary(BinaryExp* b);
    Exp* foldUnary(UnaryExp* u);
    void note(const Exp* e, const string& what);
};

// Sustituye llamadas a funciones libres pequenas y puras por su expresion
class InlineVisitor : public RewriteVisitor {
public:
    PassResult result;
    InlineVisitor() { result.name = "inline"; }
    void analyze(Program* prog);
protected:
    Exp* post(Exp* e) override;
private:
    struct Candidate {
        FunDecl* fun = nullptr;
        Exp* body = nullptr;
    };
    unordered_map<string, Candidate> candidates;
    string rejectReason(FunDecl* f, Exp*& body);
};

// Sethi-Ullman: en expresiones enteras puras evalua primero el subarbol mas pesado
// de los operadores conmutativos para reducir la profundidad de pila
class SethiVisitor : public RewriteVisitor {
public:
    PassResult result;
    SethiVisitor() { result.name = "sethi"; }
protected:
    Exp* post(Exp* e) override;
private:
    int label(const Exp* e) const;
};

struct OptReport {
    bool enabled = false;
    vector<string> passes;          // pases ejecutados, en orden
    vector<PassResult> results;
    vector<string> log;
};

// Ejecuta los pases pedidos (inline, fold, sethi; siempre en ese orden) sobre un AST ya
// verificado y lo vuelve a verificar despues de cada uno. Devuelve false si alguna
// verificacion posterior falla: el llamador debe descartar el AST y compilar sin optimizar.
bool runOptimizations(Program* ast, TypeChecker& checker, const vector<string>& passes, OptReport& rep);

#endif // OPTIMIZER_H
