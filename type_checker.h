#ifndef TYPE_CHECKER_H
#define TYPE_CHECKER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "visitor.h"
#include "ast.h"
#include "type.h"
#include "layout.h"
#include "environment.h"

using namespace std;

// Diagnostico estructurado, usado por el modo API
struct Diagnostic {
    string phase;
    string message;
    int line = 0;
    int col = 0;
};

// Analisis semantico, comprobacion de tipos y layout.
// Deja anotado el AST (tipos, offsets, simbolos resueltos, ABI, operaciones especiales de
// cada record) para el Gencode. Se puede ejecutar mas de una vez sobre el mismo AST.
class TypeChecker : public Visitor {
public:
    Environment<VarInfo> env;                    // tabla de simbolos con scopes jerarquicos
    unordered_map<string, FunDecl*> functions;   // funciones libres
    unordered_map<string, RecordDecl*> records;  // structs y classes, con su RecordLayout
    FunDecl* currentFunction;
    RecordDecl* currentRecord;                   // record cuyo metodo se esta verificando
    int loopDepth;

    vector<string> errors;
    vector<string> warnings;
    vector<Diagnostic> errorList;
    vector<Diagnostic> warningList;

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

private:
    bool hasIostream;
    bool hasString;
    bool hasCmath;
    bool usingNamespaceStd;
    unordered_set<string> usingNames;

    int frameUsed;                 // bytes de locales asignados en la funcion actual
    int initFrameUsed;             // bytes del marco de la rutina de globales
    size_t paramIndex;
    Program* program;
    unordered_map<string, long long> globalConsts;   // constantes enteras globales

    Type annotate(Exp* e, const Type& t, bool lvalue = false);

    // tamanos y layout
    int sizeOf(const Type& t);
    int alignOf(const Type& t);
    int pointeeSize(const Type& ptr, int line, int col);

    // propiedades de ciclo de vida de un tipo (los arreglos se miden por su elemento)
    bool isClassObj(const Type& t) const;       // record o string por valor
    bool isByValueClass(const Type& t) const;   // idem, sin arreglos
    bool dtorTrivial(const Type& t);
    bool copyTrivial(const Type& t);
    bool assignTrivial(const Type& t);
    bool defaultTrivial(const Type& t);
    bool hasStringIn(const Type& t);
    const RecordAbi& abiOf(const Type& t);
    RecordLayout* layoutOf(const Type& t);

    // tipos
    Type resolveType(Type t, int line, int col);
    Type declaredType(const Type& base, int ptrs, bool ref, unsigned ptrConst,
                      const vector<Exp*>& dims, int line, int col);
    Type decay(const Type& t) const;
    bool stdNameVisible(const IdExp* id, const string& name) const;
    bool assignable(const Type& to, Exp* from);
    bool qualConvertible(const Type& to, const Type& from) const;
    bool refBindable(const Type& target, const Type& from) const;
    bool boolConvertible(const Type& t) const;
    bool isNullLiteral(const Exp* e) const;
    bool constEval(Exp* e, long long& out);
    bool isConstantInit(Exp* e) const;
    bool printable(const Type& t) const;
    bool isRecordRvalueOf(Exp* e, const Type& rec) const;

    // declaraciones
    void declareRecord(RecordDecl* rd);
    void analyzeSpecialMembers(RecordDecl* rd);
    void classifyRecord(RecordDecl* rd);
    void flattenAbi(const Type& t, int off, AbiClass cls[2], bool& bad);
    FunDecl* makeImplicit(RecordDecl* rd, FunKind kind, bool copyParam);
    void declareFunction(FunDecl* fd, RecordDecl* owner);
    void computeAbi(FunDecl* fd);
    bool isCopyCtorSig(const FunDecl* fd, const RecordDecl* rd) const;
    VarInfo allocLocal(const Type& t);
    int allocTemp(const Type& t);
    VarInfo makeVarInfo(const Type& t, bool global, const string& name);
    void checkInitializer(const Type& target, Exp* init, int line, int col);
    void checkVarInit(VarInit* vi, const Type& t);
    void checkCondition(Exp* cond, int line, int col);
    Type checkCall(PostfixExp* call);
    Type checkMemberAccess(PostfixExp* exp);
    bool checkArgList(vector<Exp*>& args, vector<ArgInfo>& info, const vector<VarInfo>& params,
                      size_t offset, const string& name, int line, int col, bool evaluate = true);
    bool resolveCtorCall(const Type& rec, vector<Exp*>& args, vector<ArgInfo>& info, InitSpec& spec,
                         int line, int col, bool argsEvaluated);
    bool bindRefInit(const Type& refType, Exp* init, InitSpec& spec, int line, int col, bool evaluate = true,
                     const string& what = "la referencia");
    void planCtor(FunDecl* fun, RecordDecl* rec);
    Type callResult(PostfixExp* call, FunDecl* fd);
    bool needsDefaultInit(const Type& t, const string& what, int line, int col);
};

#endif // TYPE_CHECKER_H
