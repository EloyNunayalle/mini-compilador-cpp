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
// No optimiza nada: emite todas las copias, constructores y destructores que exige la
// semantica. Lee las anotaciones del TypeChecker (tipos, offsets, clasificacion ABI,
// operaciones especiales de cada record) y no recalcula nada de eso.
//
// Convencion de resultados de expresiones:
//   int, char, bool: %eax (char con signo extendido, bool 0 o 1)
//   punteros y direcciones de arreglos, records y strings: %rax
//   float y double: %xmm0
// Un record o string devuelto por valor vive en un temporal del marco y la expresion
// devuelve su direccion. El Type que devuelve cada visit es solo exp->inferredType.
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

    // Ubicacion de un objeto en memoria
    struct Place {
        enum Kind { FRAME, GLOBAL, THIS_FIELD, SLOT_PTR } kind = FRAME;
        int off = 0;          // FRAME: offset de %rbp; SLOT_PTR: slot que guarda el puntero
        string label;         // GLOBAL
        int extra = 0;        // desplazamiento dentro del objeto
    };

    // Objeto que hay que destruir al salir de un scope o al terminar la expresion completa
    struct CleanupItem {
        Place place;
        Type type;
    };

    enum OpKind { OP_DEFAULT, OP_COPY, OP_ASSIGN, OP_DESTROY };

    enum ArgKind {
        AK_VALUE,        // argumento del usuario, segun su ArgInfo
        AK_THIS_ADDR,    // direccion del objeto de una expresion
        AK_THIS_PTR,     // valor de una expresion puntero
        AK_THIS_IMPLICIT,
        AK_PLACE,        // direccion de un Place
        AK_TEMP_ADDR     // direccion de un temporal del marco (puntero oculto de retorno)
    };
    struct ArgSpec {
        ArgKind kind;
        Exp* exp = nullptr;
        ArgInfo info;
        Place place;
        int tempOffset = 0;
        const AbiLoc* abi = nullptr;
    };

    struct LoopCtx {
        string brk;
        string cont;
        size_t scopeDepth;
    };

    const unordered_map<string, RecordDecl*>& records;

    ostringstream text;
    ostringstream initCode;
    ostringstream bss;
    ostringstream rodata;
    ostream* cur;

    FunDecl* curFun;
    Program* program;
    string endLabel;
    int pushDepth;                 // temporales de 8 bytes pendientes en la pila
    int labelCount;
    int constCount;
    vector<LoopCtx> loops;
    vector<vector<CleanupItem>> scopes;
    vector<CleanupItem> tempStack;
    vector<CleanupItem> globalDtors;
    unordered_map<string, string> stringLabels;
    vector<VarInfo> globals;

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
    RecordLayout* layoutOf(const Type& t) const;
    bool needsDtor(const Type& t) const;
    int objSize(const Type& t) const;

    // lugares de memoria
    Place placeOf(const VarInfo& v) const;
    Place frameAt(int off) const;
    Place thisPlace(int extra) const;
    Place plus(const Place& p, int delta) const;
    bool isDirect(const Place& p) const;
    string placeMem(const Place& p) const;
    void loadAddr(const Place& p, const string& reg);
    void storeToPlace(const Type& t, const Place& p);

    // movimiento de datos
    void loadFrom(const Type& t, const string& mem);
    void storeTo(const Type& t, const string& mem);
    void pushTemp(const Type& t);
    void popToResult(const Type& t);
    void convert(const Type& from, const Type& to);
    void copyBytes(const string& src, const string& dst, int size);
    void zeroFill(const Place& p, int size);
    void loadPartial(const string& base, int off, int n);
    void storePartial(const string& src64, const string& base, int off, int n);
    void emitCall(const string& target);
    void callRuntime(const string& fn);

    // ciclo de vida
    void genOp(OpKind op, const Type& t);
    void genInitObject(const Type& t, const InitSpec& sp, const Place& pl);
    void genInitFromExpr(const Type& t, Exp* e, const Place& pl);
    void genBraced(const Type& t, BracedInitList* bl, const Place& pl, bool top);
    void genConstructTemp(PostfixExp* call);
    void registerTemp(int offset, const Type& t);
    void flushTemps(size_t mark, const Type* live);
    void moveTempsToScope(size_t mark);
    void emitScopeCleanup(size_t downTo, const Type* live);
    void emitCleanupItems(const vector<CleanupItem>& items);
    void saveLive(const Type* live);
    void restoreLive(const Type* live);
    void pushScope();
    void popScope();

    // expresiones
    void genAddr(Exp* e);
    void genObjAddr(Exp* e);
    void genBool(Exp* e);
    void genCond(Exp* e);
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
    void genCallWith(vector<ArgSpec>& args, int stackArgBytes, const string& target);
    void genBuiltin(PostfixExp* call);
    void genRuntimeCall(const string& fn, vector<ArgSpec>& args);
    void storeReturnedRegs(const AbiLoc& ra, int tempOffset);

    // declaraciones
    void genFunction(FunDecl* fun, RecordDecl* owner);
    void genParamSpill(FunDecl* fun);
    void genFieldInits(FunDecl* fun, RecordDecl* owner);
    void genImplicitAssign(FunDecl* fun, RecordDecl* owner);
    void genFieldDtors(RecordDecl* owner);
    void genVarInit(VarInit* vi);
    void genGlobals(VarDecl* decl);
};

#endif // GENCODE_H
