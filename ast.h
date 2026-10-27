#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <ostream>
#include "type.h"

using namespace std;

class Visitor;

// =========================================================================
// Operadores Binarios y Unarios de C++
// =========================================================================
enum BinaryOp {
    PLUS_OP,        // +
    MINUS_OP,       // -
    MUL_OP,         // *
    DIV_OP,         // /
    MOD_OP,         // %
    EQ_OP,          // ==
    NE_OP,          // !=
    LT_OP,          // <
    LE_OP,          // <=
    GT_OP,          // >
    GE_OP,          // >=
    AND_OP,         // &&
    OR_OP,          // ||
    BIT_AND_OP,     // &
    BIT_OR_OP,      // |
    BIT_XOR_OP,     // ^
    LSHIFT_OP,      // <<
    RSHIFT_OP       // >>
};

enum UnaryOp {
    PLUS_UNARY,     // +
    MINUS_UNARY,    // -
    NOT_UNARY,      // !
    BIT_NOT_UNARY,  // ~
    DEREF_UNARY,    // * (desreferencia de puntero)
    ADDR_UNARY,     // & (dirección de variable)
    PRE_INC_UNARY,  // ++x
    PRE_DEC_UNARY   // --x
};

enum PostfixKind {
    POST_INC,       // x++
    POST_DEC,       // x--
    ARRAY_ACCESS,   // a[i]
    DOT_ACCESS,     // a.member
    ARROW_ACCESS,   // a->member
    CALL_ACCESS     // f(args)
};

// =========================================================================
// Clase Base de Nodos AST (Sentencias, Declaraciones, Directivas, Programa)
// =========================================================================
class ASTNode {
public:
    int line;
    int col;
    ASTNode(int l = 1, int c = 1) : line(l), col(c) {}
    virtual void accept(Visitor* visitor) = 0;
    virtual ~ASTNode() = default;
};

// =========================================================================
// Expresiones (Exp) - Producen un Type al visitarse (igual que en Sem11)
// =========================================================================
class Exp {
public:
    int line;
    int col;
    Exp(int l = 1, int c = 1) : line(l), col(c) {}
    virtual Type accept(Visitor* visitor) = 0;
    virtual ~Exp() = default;
    static string binopToString(BinaryOp op);
    static string unaryopToString(UnaryOp op);
};

// Expresión binaria: left op right
class BinaryExp : public Exp {
public:
    Exp* left;
    Exp* right;
    BinaryOp op;
    BinaryExp(Exp* l, Exp* r, BinaryOp op, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~BinaryExp() override;
};

// Expresión unaria: op expr
class UnaryExp : public Exp {
public:
    Exp* exp;
    UnaryOp op;
    UnaryExp(Exp* e, UnaryOp op, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~UnaryExp() override;
};

// Expresión de asignación: target = value
class AssignExp : public Exp {
public:
    Exp* target;
    Exp* value;
    AssignExp(Exp* target, Exp* value, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~AssignExp() override;
};

// Literal entero
class NumberExp : public Exp {
public:
    long long value;
    NumberExp(long long v, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~NumberExp() override;
};

// Literal real (punto flotante / double)
class RealExp : public Exp {
public:
    double value;
    RealExp(double v, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~RealExp() override;
};

// Literal booleano (true / false)
class BoolExp : public Exp {
public:
    bool value;
    BoolExp(bool v, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~BoolExp() override;
};

// Literal de carácter ('c')
class CharExp : public Exp {
public:
    char value;
    CharExp(char v, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~CharExp() override;
};

// Literal de cadena ("...")
class StringExp : public Exp {
public:
    string value;
    StringExp(const string& v, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~StringExp() override;
};

// nullptr
class NullptrExp : public Exp {
public:
    NullptrExp(int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~NullptrExp() override;
};

// Identificador calificado: id (o ns::id o std::cout)
class IdExp : public Exp {
public:
    vector<string> parts;
    string getFullName() const;
    IdExp(const vector<string>& p, int line = 1, int col = 1);
    IdExp(const string& singleName, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~IdExp() override;
};

// Expresión Postfija (acceso a arreglo, miembro, llamada a función, sufijo ++/--)
class PostfixExp : public Exp {
public:
    Exp* base;
    PostfixKind kind;
    string memberName;         // Para .member y ->member
    Exp* indexExp;             // Para [index]
    vector<Exp*> args;         // Para (arg1, arg2, ...)
    PostfixExp(Exp* base, PostfixKind kind, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~PostfixExp() override;
};

// Expresión new: new Type o new Type[dims]
class NewExp : public Exp {
public:
    Type allocType;
    vector<Exp*> arrayDims;
    NewExp(const Type& t, const vector<Exp*>& dims, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~NewExp() override;
};

// Expresión delete: delete expr o delete[] expr
class DeleteExp : public Exp {
public:
    bool isArray;
    Exp* exp;
    DeleteExp(bool isArray, Exp* exp, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~DeleteExp() override;
};

// Lista de inicialización entre llaves: { e1, e2, ... }
class BracedInitList : public Exp {
public:
    vector<Exp*> elements;
    BracedInitList(const vector<Exp*>& elems, int line = 1, int col = 1);
    Type accept(Visitor* visitor) override;
    ~BracedInitList() override;
};

// =========================================================================
// Sentencias (Stmt) - Ejecutan acciones y devuelven void al visitarse
// =========================================================================
class Stmt : public ASTNode {
public:
    Stmt(int l = 1, int c = 1) : ASTNode(l, c) {}
    virtual void accept(Visitor* visitor) = 0;
    virtual ~Stmt() = default;
};

// Bloque de código: { decls / stmts }
class BlockStmt : public Stmt {
public:
    vector<ASTNode*> items; // Puede contener VarDecl* y Stmt*
    BlockStmt(int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~BlockStmt() override;
};

// Expresión evaluada como sentencia: expr;
class ExprStmt : public Stmt {
public:
    Exp* expr;
    ExprStmt(Exp* e, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~ExprStmt() override;
};

// Sentencia if: if (cond) thenStmt [else elseStmt]
class IfStmt : public Stmt {
public:
    Exp* condition;
    Stmt* thenStmt;
    Stmt* elseStmt; // Puede ser nullptr si no hay else
    IfStmt(Exp* cond, Stmt* thenS, Stmt* elseS = nullptr, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~IfStmt() override;
};

// Sentencia while: while (cond) body
class WhileStmt : public Stmt {
public:
    Exp* condition;
    Stmt* body;
    WhileStmt(Exp* cond, Stmt* body, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~WhileStmt() override;
};

// Declarador de inicialización de variable
class VarInit : public ASTNode {
public:
    string name;
    int ptrCount;
    bool isRef;
    vector<Exp*> arrayDims;
    Exp* initializer; // Puede ser nullptr
    VarInit(const string& n, int ptrs, bool ref, const vector<Exp*>& dims, Exp* init, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~VarInit() override;
};

// Declaración de variables: BaseType var1, *var2 = 5, arr[10];
class VarDecl : public ASTNode {
public:
    Type baseType;
    vector<VarInit*> inits;
    VarDecl(const Type& bt, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~VarDecl() override;
};

// Sentencia for: for (init; cond; update) body
class ForStmt : public Stmt {
public:
    VarDecl* initDecl;          // Puede ser nullptr si se usa expresión
    vector<Exp*> initExprs;     // Usado si no es declaración
    Exp* condition;             // Puede ser nullptr si es infinito
    vector<Exp*> updateExprs;
    Stmt* body;
    ForStmt(VarDecl* iDecl, const vector<Exp*>& iExprs, Exp* cond,
            const vector<Exp*>& uExprs, Stmt* body, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~ForStmt() override;
};

// Sentencia do-while: do body while (cond);
class DoWhileStmt : public Stmt {
public:
    Stmt* body;
    Exp* condition;
    DoWhileStmt(Stmt* body, Exp* cond, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~DoWhileStmt() override;
};

// Sentencia break;
class BreakStmt : public Stmt {
public:
    BreakStmt(int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~BreakStmt() override;
};

// Sentencia continue;
class ContinueStmt : public Stmt {
public:
    ContinueStmt(int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~ContinueStmt() override;
};

// Sentencia return [expr];
class ReturnStmt : public Stmt {
public:
    Exp* expr; // nullptr para funciones void
    ReturnStmt(Exp* e = nullptr, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~ReturnStmt() override;
};

// =========================================================================
// Declaraciones de Funciones, Clases y Directivas
// =========================================================================

// Parámetro formal de función: BaseType declarator
class Param : public ASTNode {
public:
    Type type;
    string name;
    int ptrCount;
    bool isRef;
    vector<Exp*> arrayDims;
    Param(const Type& t, const string& n, int ptrs, bool ref, const vector<Exp*>& dims, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~Param() override;
};

// Declaración de función: ReturnType id ( params ) Block
class FunDecl : public ASTNode {
public:
    Type returnType;
    string name;
    vector<Param*> params;
    BlockStmt* body;
    FunDecl(const Type& retT, const string& n, const vector<Param*>& ps, BlockStmt* b, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~FunDecl() override;
};

// Miembro de clase / struct
enum AccessSpecifier {
    ACCESS_PUBLIC,
    ACCESS_PRIVATE
};

class RecordMember : public ASTNode {
public:
    AccessSpecifier access;
    VarDecl* varDecl; // No nulo si es variable miembro
    FunDecl* funDecl; // No nulo si es método
    RecordMember(AccessSpecifier acc, VarDecl* v, FunDecl* f = nullptr, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~RecordMember() override;
};

// Declaración de clase o struct: class/struct id { members };
enum RecordKind {
    RECORD_CLASS,
    RECORD_STRUCT
};

class RecordDecl : public ASTNode {
public:
    RecordKind kind;
    string name;
    vector<RecordMember*> members;
    RecordDecl(RecordKind k, const string& n, const vector<RecordMember*>& mems, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~RecordDecl() override;
};

// Directiva #include <Library>
class IncludeDirective : public ASTNode {
public:
    string library;
    IncludeDirective(const string& lib, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~IncludeDirective() override;
};

// Directiva using [namespace] id;
class UsingDirective : public ASTNode {
public:
    bool isNamespace;
    vector<string> qualifiedId;
    string getFullName() const;
    UsingDirective(bool isNs, const vector<string>& qid, int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~UsingDirective() override;
};

// =========================================================================
// Raíz del Programa: Program ::= [Includes] [Usings] Decl*
// =========================================================================
class Program : public ASTNode {
public:
    vector<IncludeDirective*> includes;
    vector<UsingDirective*> usings;
    vector<ASTNode*> declarations; // VarDecl*, FunDecl*, RecordDecl*
    Program(int line = 1, int col = 1);
    void accept(Visitor* visitor) override;
    ~Program() override;
};

#endif // AST_H
