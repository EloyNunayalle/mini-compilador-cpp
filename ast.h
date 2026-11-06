#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <ostream>
#include "type.h"
#include "layout.h"

using namespace std;

class Visitor;

// Identificador de clase de nodo, evita RTTI y dynamic_cast
enum class NodeKind {
    BINARY_EXP, UNARY_EXP, ASSIGN_EXP, NUMBER_EXP, REAL_EXP, BOOL_EXP, CHAR_EXP,
    STRING_EXP, NULLPTR_EXP, ID_EXP, POSTFIX_EXP, NEW_EXP, DELETE_EXP, BRACED_INIT,
    BLOCK_STMT, EXPR_STMT, IF_STMT, WHILE_STMT, FOR_STMT, DO_WHILE_STMT,
    BREAK_STMT, CONTINUE_STMT, RETURN_STMT,
    VAR_INIT, VAR_DECL, PARAM, FUN_DECL, RECORD_MEMBER, RECORD_DECL,
    INCLUDE_DIRECTIVE, USING_DIRECTIVE, PROGRAM
};

// Anotaciones que el TypeChecker deja en el AST para el Gencode
enum IdKind {
    ID_NONE,
    ID_VAR,     // local, parametro o global segun VarInfo
    ID_FIELD,   // campo accedido via this implicito
    ID_COUT,
    ID_CIN,
    ID_ENDL
};

enum CallKind {
    CALL_NONE,
    CALL_FUNCTION,
    CALL_METHOD,          // obj.m() o ptr->m()
    CALL_METHOD_IMPLICIT, // m() dentro de otro metodo del mismo record
    CALL_BUILTIN
};

enum Builtin {
    BI_NONE,
    BI_SQRT,
    BI_STR_SIZE,
    BI_STR_EMPTY,
    BI_STR_CLEAR,
    BI_STR_C_STR,
    BI_STR_APPEND_STR,
    BI_STR_APPEND_CSTR,
    BI_STR_APPEND_CHAR
};

enum BinaryMode {
    BM_NORMAL,
    BM_COUT,           // stream << valor
    BM_CIN,            // stream >> lvalue
    BM_PTR_PLUS_INT,
    BM_INT_PLUS_PTR,
    BM_PTR_MINUS_INT,
    BM_PTR_DIFF,
    BM_STR_STR,        // ==, != entre strings
    BM_STR_CSTR,
    BM_CSTR_STR
};

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
    virtual NodeKind nodeKind() const = 0;
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
    Type inferredType;     // lo fija el TypeChecker
    bool isLValue;
    Exp(int l = 1, int c = 1) : line(l), col(c), isLValue(false) {}
    virtual NodeKind nodeKind() const = 0;
    virtual Type accept(Visitor* visitor) = 0;
    virtual ~Exp() = default;
    static string binopToString(BinaryOp op);
    static string unaryopToString(UnaryOp op);
};

// Expresión binaria: left op right
class BinaryExp : public Exp {
public:
    Type operandType;          // tipo comun de los operandos tras las conversiones
    BinaryMode mode = BM_NORMAL;
    int ptrScale = 0;          // tamano del elemento apuntado en aritmetica de punteros
    Exp* left;
    Exp* right;
    BinaryOp op;
    BinaryExp(Exp* l, Exp* r, BinaryOp op, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::BINARY_EXP; }
    Type accept(Visitor* visitor) override;
    ~BinaryExp() override;
};

// Expresión unaria: op expr
class UnaryExp : public Exp {
public:
    int ptrScale = 0;
    Exp* exp;
    UnaryOp op;
    UnaryExp(Exp* e, UnaryOp op, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::UNARY_EXP; }
    Type accept(Visitor* visitor) override;
    ~UnaryExp() override;
};

// Expresión de asignación: target = value
class AssignExp : public Exp {
public:
    Exp* target;
    Exp* value;
    AssignExp(Exp* target, Exp* value, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::ASSIGN_EXP; }
    Type accept(Visitor* visitor) override;
    ~AssignExp() override;
};

// Literal entero
class NumberExp : public Exp {
public:
    long long value;
    NumberExp(long long v, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::NUMBER_EXP; }
    Type accept(Visitor* visitor) override;
    ~NumberExp() override;
};

// Literal real (punto flotante / double)
class RealExp : public Exp {
public:
    double value;
    RealExp(double v, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::REAL_EXP; }
    Type accept(Visitor* visitor) override;
    ~RealExp() override;
};

// Literal booleano (true / false)
class BoolExp : public Exp {
public:
    bool value;
    BoolExp(bool v, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::BOOL_EXP; }
    Type accept(Visitor* visitor) override;
    ~BoolExp() override;
};

// Literal de carácter ('c')
class CharExp : public Exp {
public:
    char value;
    CharExp(char v, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::CHAR_EXP; }
    Type accept(Visitor* visitor) override;
    ~CharExp() override;
};

// Literal de cadena ("...")
class StringExp : public Exp {
public:
    string value;
    StringExp(const string& v, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::STRING_EXP; }
    Type accept(Visitor* visitor) override;
    ~StringExp() override;
};

// nullptr
class NullptrExp : public Exp {
public:
    NullptrExp(int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::NULLPTR_EXP; }
    Type accept(Visitor* visitor) override;
    ~NullptrExp() override;
};

// Identificador calificado: id (o ns::id o std::cout)
class IdExp : public Exp {
public:
    IdKind idKind = ID_NONE;
    VarInfo var;               // variable resuelta (offset o etiqueta)
    bool isRefVar = false;     // la variable es una referencia
    vector<string> parts;
    string getFullName() const;
    IdExp(const vector<string>& p, int line = 1, int col = 1);
    IdExp(const string& singleName, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::ID_EXP; }
    Type accept(Visitor* visitor) override;
    ~IdExp() override;
};

// Expresión Postfija (acceso a arreglo, miembro, llamada a función, sufijo ++/--)
class PostfixExp : public Exp {
public:
    int elemSize = 0;          // ARRAY_ACCESS: tamano del elemento resultante
    int fieldOffset = 0;       // DOT/ARROW: offset del campo
    int ptrScale = 0;          // POST_INC/POST_DEC sobre punteros
    CallKind callKind = CALL_NONE;
    FunDecl* callee = nullptr;
    Builtin builtin = BI_NONE;
    Exp* base;
    PostfixKind kind;
    string memberName;         // Para .member y ->member
    Exp* indexExp;             // Para [index]
    vector<Exp*> args;         // Para (arg1, arg2, ...)
    PostfixExp(Exp* base, PostfixKind kind, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::POSTFIX_EXP; }
    Type accept(Visitor* visitor) override;
    ~PostfixExp() override;
};

// Expresión new: new Type o new Type[dims]
class NewExp : public Exp {
public:
    int elemSize = 0;          // tamano de una instancia de allocType
    Type allocType;
    vector<Exp*> arrayDims;
    NewExp(const Type& t, const vector<Exp*>& dims, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::NEW_EXP; }
    Type accept(Visitor* visitor) override;
    ~NewExp() override;
};

// Expresión delete: delete expr o delete[] expr
class DeleteExp : public Exp {
public:
    bool isArray;
    Exp* exp;
    DeleteExp(bool isArray, Exp* exp, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::DELETE_EXP; }
    Type accept(Visitor* visitor) override;
    ~DeleteExp() override;
};

// Lista de inicialización entre llaves: { e1, e2, ... }
class BracedInitList : public Exp {
public:
    vector<int> elementOffsets;   // offset de cada elemento dentro del agregado
    vector<Type> elementTypes;
    vector<Exp*> elements;
    BracedInitList(const vector<Exp*>& elems, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::BRACED_INIT; }
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
    NodeKind nodeKind() const override { return NodeKind::BLOCK_STMT; }
    void accept(Visitor* visitor) override;
    ~BlockStmt() override;
};

// Expresión evaluada como sentencia: expr;
class ExprStmt : public Stmt {
public:
    Exp* expr;
    ExprStmt(Exp* e, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::EXPR_STMT; }
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
    NodeKind nodeKind() const override { return NodeKind::IF_STMT; }
    void accept(Visitor* visitor) override;
    ~IfStmt() override;
};

// Sentencia while: while (cond) body
class WhileStmt : public Stmt {
public:
    Exp* condition;
    Stmt* body;
    WhileStmt(Exp* cond, Stmt* body, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::WHILE_STMT; }
    void accept(Visitor* visitor) override;
    ~WhileStmt() override;
};

// Declarador de inicialización de variable
class VarInit : public ASTNode {
public:
    VarInfo info;              // lo completa el TypeChecker
    string name;
    int ptrCount;
    bool isRef;
    vector<Exp*> arrayDims;
    Exp* initializer; // Puede ser nullptr
    VarInit(const string& n, int ptrs, bool ref, const vector<Exp*>& dims, Exp* init, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::VAR_INIT; }
    void accept(Visitor* visitor) override;
    ~VarInit() override;
};

// Declaración de variables: BaseType var1, *var2 = 5, arr[10];
class VarDecl : public ASTNode {
public:
    Type baseType;
    vector<VarInit*> inits;
    VarDecl(const Type& bt, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::VAR_DECL; }
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
    NodeKind nodeKind() const override { return NodeKind::FOR_STMT; }
    void accept(Visitor* visitor) override;
    ~ForStmt() override;
};

// Sentencia do-while: do body while (cond);
class DoWhileStmt : public Stmt {
public:
    Stmt* body;
    Exp* condition;
    DoWhileStmt(Stmt* body, Exp* cond, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::DO_WHILE_STMT; }
    void accept(Visitor* visitor) override;
    ~DoWhileStmt() override;
};

// Sentencia break;
class BreakStmt : public Stmt {
public:
    BreakStmt(int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::BREAK_STMT; }
    void accept(Visitor* visitor) override;
    ~BreakStmt() override;
};

// Sentencia continue;
class ContinueStmt : public Stmt {
public:
    ContinueStmt(int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::CONTINUE_STMT; }
    void accept(Visitor* visitor) override;
    ~ContinueStmt() override;
};

// Sentencia return [expr];
class ReturnStmt : public Stmt {
public:
    Exp* expr; // nullptr para funciones void
    ReturnStmt(Exp* e = nullptr, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::RETURN_STMT; }
    void accept(Visitor* visitor) override;
    ~ReturnStmt() override;
};

// =========================================================================
// Declaraciones de Funciones, Clases y Directivas
// =========================================================================

// Parámetro formal de función: BaseType declarator
class Param : public ASTNode {
public:
    VarInfo info;
    Type type;
    string name;
    int ptrCount;
    bool isRef;
    vector<Exp*> arrayDims;
    Param(const Type& t, const string& n, int ptrs, bool ref, const vector<Exp*>& dims, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::PARAM; }
    void accept(Visitor* visitor) override;
    ~Param() override;
};

// Declaración de función: ReturnType id ( params ) Block
class FunDecl : public ASTNode {
public:
    FunctionLayout layout;     // incluye owner y firma resuelta
    Type returnType;
    string name;
    vector<Param*> params;
    BlockStmt* body;
    FunDecl(const Type& retT, const string& n, const vector<Param*>& ps, BlockStmt* b, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::FUN_DECL; }
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
    NodeKind nodeKind() const override { return NodeKind::RECORD_MEMBER; }
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
    RecordLayout layout;
    RecordKind kind;
    string name;
    vector<RecordMember*> members;
    RecordDecl(RecordKind k, const string& n, const vector<RecordMember*>& mems, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::RECORD_DECL; }
    void accept(Visitor* visitor) override;
    ~RecordDecl() override;
};

// Directiva #include <Library>
class IncludeDirective : public ASTNode {
public:
    string library;
    IncludeDirective(const string& lib, int line = 1, int col = 1);
    NodeKind nodeKind() const override { return NodeKind::INCLUDE_DIRECTIVE; }
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
    NodeKind nodeKind() const override { return NodeKind::USING_DIRECTIVE; }
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
    NodeKind nodeKind() const override { return NodeKind::PROGRAM; }
    void accept(Visitor* visitor) override;
    ~Program() override;
};

#endif // AST_H
