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

// Analisis semantico, comprobacion de tipos y layout.
// Deja anotado el AST (tipos, offsets, simbolos resueltos) para el Gencode.
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
    vector<int> curStringSlots;
    size_t paramIndex;

    Type annotate(Exp* e, const Type& t, bool lvalue = false);

    // tamanos y layout
    int sizeOf(const Type& t);
    int alignOf(const Type& t);
    bool hasStrings(const Type& t);
    void collectStringOffsets(const Type& t, vector<int>& out);
    int pointeeSize(const Type& ptr, int line, int col);

    // tipos
    Type resolveType(Type t, int line, int col);
    Type declaredType(const Type& base, int ptrs, bool ref, const vector<Exp*>& dims, int line, int col);
    Type decay(const Type& t) const;
    bool stdNameVisible(const IdExp* id, const string& name) const;
    bool assignable(const Type& to, Exp* from);
    bool boolConvertible(const Type& t) const;
    bool isNullLiteral(const Exp* e) const;
    bool constEval(Exp* e, long long& out);
    bool printable(const Type& t) const;

    // declaraciones
    void declareRecord(RecordDecl* rd);
    void declareFunction(FunDecl* fd, RecordDecl* owner);
    VarInfo allocLocal(const Type& t);
    VarInfo makeVarInfo(const Type& t, bool global, const string& name);
    void checkInitializer(const Type& target, Exp* init, int line, int col);
    void checkCondition(Exp* cond, int line, int col);
    Type checkCall(PostfixExp* call);
    Type checkMemberAccess(PostfixExp* exp);
    void checkArgs(PostfixExp* call, FunDecl* fd, const string& name);
    Type callResult(PostfixExp* call, FunDecl* fd);
};

#endif // TYPE_CHECKER_H
