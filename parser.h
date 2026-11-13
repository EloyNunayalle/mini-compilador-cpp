#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <unordered_set>
#include "scanner.h"
#include "token.h"
#include "ast.h"

using namespace std;

// Parser descendente recursivo según la especificación EBNF de C++
class Parser {
private:
    Scanner* scanner;
    Token* current;
    Token* previous;
    int loopDepth;
    unordered_set<string> knownTypes; // Tipos definidos por usuario (structs / classes)
    vector<Token*> allocatedTokens;   // Gestión de memoria segura para tokens durante el parseo

    bool match(Token::Type ttype);
    bool check(Token::Type ttype) const;
    bool advance();
    bool isAtEnd() const;
    Token* consume(Token::Type ttype, const string& errorMessage);
    void error(const string& msg);

    bool isBaseTypeStart() const;
    bool peekIsStdString() const;
    Token::Type peekTokenType() const;     // tipo del token que sigue al actual
    bool startsCtorCall() const;           // T( ... ) al inicio de una sentencia
    Type parseBaseTypeCore();
    vector<MemberInit> parseMemberInitList();
    RecordMember* parseCtorMember(const string& recordName, AccessSpecifier access);
    RecordMember* parseDtorMember(const string& recordName, AccessSpecifier access);
    unsigned parsePtrDeclarator(int& ptrs);  // { "*" [const] }, devuelve los bits const

public:
    Parser(Scanner* scanner);
    ~Parser();

    // Raíz y directivas
    Program* parseProgram();
    vector<IncludeDirective*> parseIncludeList();
    IncludeDirective* parseInclude();
    vector<UsingDirective*> parseUsingList();
    UsingDirective* parseUsing();
    vector<string> parseQualifiedId();

    // Tipos
    Type parseBaseType();
    Type parseReturnType();

    // Declaraciones
    ASTNode* parseDecl();
    VarDecl* parseVarDecl();
    VarDecl* parseVarDeclNoSemicolon();
    vector<VarInit*> parseVarList(const Type& baseType);
    VarInit* parseVarInit(const Type& baseType);
    FunDecl* parseFunDecl();
    vector<Param*> parseParamDeclList();
    Param* parseParam();
    RecordDecl* parseRecordDecl();
    RecordMember* parseClassMember();

    // Bloques y sentencias
    BlockStmt* parseBlock();
    Stmt* parseStmt();
    IfStmt* parseIfStmt();
    WhileStmt* parseWhileStmt();
    ForStmt* parseForStmt();
    DoWhileStmt* parseDoWhileStmt();
    BreakStmt* parseBreakStmt();
    ContinueStmt* parseContinueStmt();
    ReturnStmt* parseReturnStmt();
    ExprStmt* parseExprStmt();

    // Expresiones con jerarquía de precedencia
    Exp* parseCExp();
    Exp* parseAssignExp();
    Exp* parseLogicOr();
    Exp* parseLogicAnd();
    Exp* parseBitOr();
    Exp* parseBitXor();
    Exp* parseBitAnd();
    Exp* parseEqExp();
    Exp* parseRelExp();
    Exp* parseShiftExp();
    Exp* parseExp();
    Exp* parseTerm();
    Exp* parseUnary();
    Exp* parseDeleteExpr();
    Exp* parseNewExpr();
    Exp* parsePostfix();
    Exp* parsePrimary();
    Exp* parseInitializer();
    BracedInitList* parseBracedInitList();
    vector<Exp*> parseArrayDims();
    vector<Exp*> parseArgList();
};

#endif // PARSER_H
