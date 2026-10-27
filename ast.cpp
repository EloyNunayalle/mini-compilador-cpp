#include "ast.h"
#include "visitor.h"

using namespace std;

// =========================================================================
// Métodos de ayuda
// =========================================================================
string Exp::binopToString(BinaryOp op) {
    switch (op) {
        case PLUS_OP:    return "+";
        case MINUS_OP:   return "-";
        case MUL_OP:     return "*";
        case DIV_OP:     return "/";
        case MOD_OP:     return "%";
        case EQ_OP:      return "==";
        case NE_OP:      return "!=";
        case LT_OP:      return "<";
        case LE_OP:      return "<=";
        case GT_OP:      return ">";
        case GE_OP:      return ">=";
        case AND_OP:     return "&&";
        case OR_OP:      return "||";
        case BIT_AND_OP: return "&";
        case BIT_OR_OP:  return "|";
        case BIT_XOR_OP: return "^";
        case LSHIFT_OP:  return "<<";
        case RSHIFT_OP:  return ">>";
    }
    return "?";
}

string Exp::unaryopToString(UnaryOp op) {
    switch (op) {
        case PLUS_UNARY:    return "+";
        case MINUS_UNARY:   return "-";
        case NOT_UNARY:     return "!";
        case BIT_NOT_UNARY: return "~";
        case DEREF_UNARY:   return "*";
        case ADDR_UNARY:    return "&";
        case PRE_INC_UNARY: return "++";
        case PRE_DEC_UNARY: return "--";
    }
    return "?";
}

// =========================================================================
// Constructores y Destructores de Expresiones
// =========================================================================
BinaryExp::BinaryExp(Exp* l, Exp* r, BinaryOp op, int line, int col)
    : Exp(line, col), left(l), right(r), op(op) {}

BinaryExp::~BinaryExp() {
    delete left;
    delete right;
}

UnaryExp::UnaryExp(Exp* e, UnaryOp op, int line, int col)
    : Exp(line, col), exp(e), op(op) {}

UnaryExp::~UnaryExp() {
    delete exp;
}

AssignExp::AssignExp(Exp* target, Exp* value, int line, int col)
    : Exp(line, col), target(target), value(value) {}

AssignExp::~AssignExp() {
    delete target;
    delete value;
}

NumberExp::NumberExp(long long v, int line, int col)
    : Exp(line, col), value(v) {}

NumberExp::~NumberExp() {}

RealExp::RealExp(double v, int line, int col)
    : Exp(line, col), value(v) {}

RealExp::~RealExp() {}

BoolExp::BoolExp(bool v, int line, int col)
    : Exp(line, col), value(v) {}

BoolExp::~BoolExp() {}

CharExp::CharExp(char v, int line, int col)
    : Exp(line, col), value(v) {}

CharExp::~CharExp() {}

StringExp::StringExp(const string& v, int line, int col)
    : Exp(line, col), value(v) {}

StringExp::~StringExp() {}

NullptrExp::NullptrExp(int line, int col)
    : Exp(line, col) {}

NullptrExp::~NullptrExp() {}

IdExp::IdExp(const vector<string>& p, int line, int col)
    : Exp(line, col), parts(p) {}

IdExp::IdExp(const string& singleName, int line, int col)
    : Exp(line, col), parts({singleName}) {}

IdExp::~IdExp() {}

string IdExp::getFullName() const {
    string res = "";
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) res += "::";
        res += parts[i];
    }
    return res;
}

PostfixExp::PostfixExp(Exp* b, PostfixKind k, int line, int col)
    : Exp(line, col), base(b), kind(k), memberName(""), indexExp(nullptr) {}

PostfixExp::~PostfixExp() {
    delete base;
    delete indexExp;
    for (Exp* a : args) delete a;
}

NewExp::NewExp(const Type& t, const vector<Exp*>& dims, int line, int col)
    : Exp(line, col), allocType(t), arrayDims(dims) {}

NewExp::~NewExp() {
    for (Exp* d : arrayDims) delete d;
}

DeleteExp::DeleteExp(bool isArr, Exp* e, int line, int col)
    : Exp(line, col), isArray(isArr), exp(e) {}

DeleteExp::~DeleteExp() {
    delete exp;
}

BracedInitList::BracedInitList(const vector<Exp*>& elems, int line, int col)
    : Exp(line, col), elements(elems) {}

BracedInitList::~BracedInitList() {
    for (Exp* e : elements) delete e;
}

// =========================================================================
// Constructores y Destructores de Sentencias
// =========================================================================
BlockStmt::BlockStmt(int line, int col)
    : Stmt(line, col) {}

BlockStmt::~BlockStmt() {
    for (ASTNode* item : items) delete item;
}

ExprStmt::ExprStmt(Exp* e, int line, int col)
    : Stmt(line, col), expr(e) {}

ExprStmt::~ExprStmt() {
    delete expr;
}

IfStmt::IfStmt(Exp* cond, Stmt* thenS, Stmt* elseS, int line, int col)
    : Stmt(line, col), condition(cond), thenStmt(thenS), elseStmt(elseS) {}

IfStmt::~IfStmt() {
    delete condition;
    delete thenStmt;
    delete elseStmt;
}

WhileStmt::WhileStmt(Exp* cond, Stmt* b, int line, int col)
    : Stmt(line, col), condition(cond), body(b) {}

WhileStmt::~WhileStmt() {
    delete condition;
    delete body;
}

ForStmt::ForStmt(VarDecl* iDecl, const vector<Exp*>& iExprs, Exp* cond,
                 const vector<Exp*>& uExprs, Stmt* b, int line, int col)
    : Stmt(line, col), initDecl(iDecl), initExprs(iExprs), condition(cond),
      updateExprs(uExprs), body(b) {}

ForStmt::~ForStmt() {
    delete initDecl;
    for (Exp* e : initExprs) delete e;
    delete condition;
    for (Exp* e : updateExprs) delete e;
    delete body;
}

DoWhileStmt::DoWhileStmt(Stmt* b, Exp* cond, int line, int col)
    : Stmt(line, col), body(b), condition(cond) {}

DoWhileStmt::~DoWhileStmt() {
    delete body;
    delete condition;
}

BreakStmt::BreakStmt(int line, int col) : Stmt(line, col) {}
BreakStmt::~BreakStmt() {}

ContinueStmt::ContinueStmt(int line, int col) : Stmt(line, col) {}
ContinueStmt::~ContinueStmt() {}

ReturnStmt::ReturnStmt(Exp* e, int line, int col)
    : Stmt(line, col), expr(e) {}

ReturnStmt::~ReturnStmt() {
    delete expr;
}

// =========================================================================
// Constructores y Destructores de Declaraciones
// =========================================================================
VarInit::VarInit(const string& n, int ptrs, bool ref, const vector<Exp*>& dims, Exp* init, int line, int col)
    : ASTNode(line, col), name(n), ptrCount(ptrs), isRef(ref), arrayDims(dims), initializer(init) {}

VarInit::~VarInit() {
    for (Exp* d : arrayDims) delete d;
    delete initializer;
}

VarDecl::VarDecl(const Type& bt, int line, int col)
    : ASTNode(line, col), baseType(bt) {}

VarDecl::~VarDecl() {
    for (VarInit* v : inits) delete v;
}

Param::Param(const Type& t, const string& n, int ptrs, bool ref, const vector<Exp*>& dims, int line, int col)
    : ASTNode(line, col), type(t), name(n), ptrCount(ptrs), isRef(ref), arrayDims(dims) {}

Param::~Param() {
    for (Exp* d : arrayDims) delete d;
}

FunDecl::FunDecl(const Type& retT, const string& n, const vector<Param*>& ps, BlockStmt* b, int line, int col)
    : ASTNode(line, col), returnType(retT), name(n), params(ps), body(b) {}

FunDecl::~FunDecl() {
    for (Param* p : params) delete p;
    delete body;
}

RecordMember::RecordMember(AccessSpecifier acc, VarDecl* v, FunDecl* f, int line, int col)
    : ASTNode(line, col), access(acc), varDecl(v), funDecl(f) {}

RecordMember::~RecordMember() {
    delete varDecl;
    delete funDecl;
}

RecordDecl::RecordDecl(RecordKind k, const string& n, const vector<RecordMember*>& mems, int line, int col)
    : ASTNode(line, col), kind(k), name(n), members(mems) {}

RecordDecl::~RecordDecl() {
    for (RecordMember* m : members) delete m;
}

IncludeDirective::IncludeDirective(const string& lib, int line, int col)
    : ASTNode(line, col), library(lib) {}

IncludeDirective::~IncludeDirective() {}

UsingDirective::UsingDirective(bool isNs, const vector<string>& qid, int line, int col)
    : ASTNode(line, col), isNamespace(isNs), qualifiedId(qid) {}

UsingDirective::~UsingDirective() {}

string UsingDirective::getFullName() const {
    string res = "";
    for (size_t i = 0; i < qualifiedId.size(); i++) {
        if (i > 0) res += "::";
        res += qualifiedId[i];
    }
    return res;
}

Program::Program(int line, int col)
    : ASTNode(line, col) {}

Program::~Program() {
    for (IncludeDirective* inc : includes) delete inc;
    for (UsingDirective* usg : usings) delete usg;
    for (ASTNode* decl : declarations) delete decl;
}

// =========================================================================
// Métodos accept (Visitor Pattern)
// =========================================================================
Type BinaryExp::accept(Visitor* visitor)       { return visitor->visit(this); }
Type UnaryExp::accept(Visitor* visitor)        { return visitor->visit(this); }
Type AssignExp::accept(Visitor* visitor)       { return visitor->visit(this); }
Type NumberExp::accept(Visitor* visitor)       { return visitor->visit(this); }
Type RealExp::accept(Visitor* visitor)         { return visitor->visit(this); }
Type BoolExp::accept(Visitor* visitor)         { return visitor->visit(this); }
Type CharExp::accept(Visitor* visitor)         { return visitor->visit(this); }
Type StringExp::accept(Visitor* visitor)       { return visitor->visit(this); }
Type NullptrExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type IdExp::accept(Visitor* visitor)           { return visitor->visit(this); }
Type PostfixExp::accept(Visitor* visitor)      { return visitor->visit(this); }
Type NewExp::accept(Visitor* visitor)          { return visitor->visit(this); }
Type DeleteExp::accept(Visitor* visitor)       { return visitor->visit(this); }
Type BracedInitList::accept(Visitor* visitor)  { return visitor->visit(this); }

void BlockStmt::accept(Visitor* visitor)       { visitor->visit(this); }
void ExprStmt::accept(Visitor* visitor)        { visitor->visit(this); }
void IfStmt::accept(Visitor* visitor)          { visitor->visit(this); }
void WhileStmt::accept(Visitor* visitor)       { visitor->visit(this); }
void ForStmt::accept(Visitor* visitor)         { visitor->visit(this); }
void DoWhileStmt::accept(Visitor* visitor)     { visitor->visit(this); }
void BreakStmt::accept(Visitor* visitor)       { visitor->visit(this); }
void ContinueStmt::accept(Visitor* visitor)    { visitor->visit(this); }
void ReturnStmt::accept(Visitor* visitor)      { visitor->visit(this); }

void VarInit::accept(Visitor* visitor)         { visitor->visit(this); }
void VarDecl::accept(Visitor* visitor)         { visitor->visit(this); }
void Param::accept(Visitor* visitor)           { visitor->visit(this); }
void FunDecl::accept(Visitor* visitor)         { visitor->visit(this); }
void RecordMember::accept(Visitor* visitor)    { visitor->visit(this); }
void RecordDecl::accept(Visitor* visitor)      { visitor->visit(this); }
void IncludeDirective::accept(Visitor* visitor){ visitor->visit(this); }
void UsingDirective::accept(Visitor* visitor)  { visitor->visit(this); }
void Program::accept(Visitor* visitor)         { visitor->visit(this); }
