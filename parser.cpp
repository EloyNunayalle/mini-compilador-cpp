#include <iostream>
#include <stdexcept>
#include "parser.h"

using namespace std;

Parser::Parser(Scanner* sc)
    : scanner(sc), current(nullptr), previous(nullptr), loopDepth(0) {
    current = scanner->nextToken();
    if (current) allocatedTokens.push_back(current);
    if (current && current->type == Token::ERR) {
        error("Error léxico inicial: " + current->text);
    }
}

Parser::~Parser() {
    for (Token* t : allocatedTokens) {
        delete t;
    }
}

void Parser::error(const string& msg) {
    int l = current ? current->line : (previous ? previous->line : 1);
    int c = current ? current->col : (previous ? previous->col : 1);
    throw runtime_error("Error sintáctico [Línea " + to_string(l) + ", Columna " + to_string(c) + "]: " + msg);
}

bool Parser::isAtEnd() const {
    return !current || current->type == Token::END;
}

bool Parser::check(Token::Type ttype) const {
    if (isAtEnd()) return false;
    return current->type == ttype;
}

bool Parser::advance() {
    if (!isAtEnd()) {
        previous = current;
        current = scanner->nextToken();
        if (current) allocatedTokens.push_back(current);
        if (check(Token::ERR)) {
            error("Error léxico: " + current->text);
        }
        return true;
    }
    return false;
}

bool Parser::match(Token::Type ttype) {
    if (check(ttype)) {
        advance();
        return true;
    }
    return false;
}

Token* Parser::consume(Token::Type ttype, const string& errorMessage) {
    if (check(ttype)) {
        advance();
        return previous;
    }
    error(errorMessage + " (se encontró '" + (current ? current->text : "EOF") + "')");
    return nullptr;
}

bool Parser::isBaseTypeStart() const {
    if (!current) return false;
    switch (current->type) {
        case Token::KW_INT:
        case Token::KW_FLOAT:
        case Token::KW_DOUBLE:
        case Token::KW_BOOL:
        case Token::KW_CHAR:
        case Token::KW_VOID:
            return true;
        case Token::ID:
            return knownTypes.find(current->text) != knownTypes.end();
        default:
            return false;
    }
}

// =========================================================================
// Raíz y Directivas
// =========================================================================

Program* Parser::parseProgram() {
    int startLine = current ? current->line : 1;
    int startCol = current ? current->col : 1;
    Program* prog = new Program(startLine, startCol);

    // [ IncludeList ]
    if (check(Token::HASH) || check(Token::KW_INCLUDE)) {
        prog->includes = parseIncludeList();
    }

    // [ UsingList ]
    if (check(Token::KW_USING)) {
        prog->usings = parseUsingList();
    }

    // Decl { Decl }
    while (!isAtEnd()) {
        prog->declarations.push_back(parseDecl());
    }

    return prog;
}

vector<IncludeDirective*> Parser::parseIncludeList() {
    vector<IncludeDirective*> list;
    while (check(Token::HASH) || check(Token::KW_INCLUDE)) {
        list.push_back(parseInclude());
    }
    return list;
}

IncludeDirective* Parser::parseInclude() {
    int l = current->line;
    int c = current->col;
    if (match(Token::HASH)) {
        consume(Token::KW_INCLUDE, "Se esperaba 'include' después de '#'");
    } else {
        consume(Token::KW_INCLUDE, "Se esperaba '#include'");
    }

    consume(Token::LT, "Se esperaba '<' en directiva #include");

    string libName = "";
    if (match(Token::LIB_IOSTREAM))      libName = "iostream";
    else if (match(Token::LIB_STRING))   libName = "string";
    else if (match(Token::LIB_CMATH))    libName = "cmath";
    else if (match(Token::ID))           libName = previous->text;
    else error("Nombre de librería desconocido en #include");

    consume(Token::GT, "Se esperaba '>' al cerrar la librería");
    return new IncludeDirective(libName, l, c);
}

vector<UsingDirective*> Parser::parseUsingList() {
    vector<UsingDirective*> list;
    while (check(Token::KW_USING)) {
        list.push_back(parseUsing());
    }
    return list;
}

UsingDirective* Parser::parseUsing() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_USING, "Se esperaba 'using'");

    bool isNs = false;
    if (match(Token::KW_NAMESPACE)) {
        isNs = true;
    }

    vector<string> qid = parseQualifiedId();
    consume(Token::SEMICOL, "Se esperaba ';' al final de la directiva using");

    return new UsingDirective(isNs, qid, l, c);
}

vector<string> Parser::parseQualifiedId() {
    vector<string> parts;
    Token* first = consume(Token::ID, "Se esperaba identificador");
    parts.push_back(first->text);

    while (match(Token::COLON_COLON)) {
        Token* next = consume(Token::ID, "Se esperaba identificador después de '::'");
        parts.push_back(next->text);
    }
    return parts;
}

// =========================================================================
// Tipos
// =========================================================================

Type Parser::parseBaseType() {
    if (match(Token::KW_INT))    return Type(Type::INT);
    if (match(Token::KW_FLOAT))  return Type(Type::FLOAT);
    if (match(Token::KW_DOUBLE)) return Type(Type::DOUBLE);
    if (match(Token::KW_BOOL))   return Type(Type::BOOL);
    if (match(Token::KW_CHAR))   return Type(Type::CHAR);
    if (match(Token::KW_VOID))   return Type(Type::VOID);

    if (check(Token::ID)) {
        vector<string> qid = parseQualifiedId();
        string fullName = "";
        for (size_t i = 0; i < qid.size(); i++) {
            if (i > 0) fullName += "::";
            fullName += qid[i];
        }
        return Type::makeRecord(fullName);
    }

    error("Se esperaba un tipo base (int, float, double, bool, char, void o tipo de usuario)");
    return Type::makeError();
}

Type Parser::parseReturnType() {
    Type t = parseBaseType();
    while (match(Token::MUL)) {
        t.ptrLevel++;
    }
    if (match(Token::AMP)) {
        t.isRef = true;
    }
    return t;
}

// =========================================================================
// Declaraciones
// =========================================================================

ASTNode* Parser::parseDecl() {
    if (check(Token::KW_CLASS) || check(Token::KW_STRUCT)) {
        return parseRecordDecl();
    }

    // Distinción entre Función y Variable
    // Ambos inician con un ReturnType / BaseType.
    // Una función tiene la forma: ReturnType id ( ... ) Block
    Type type = parseReturnType();
    Token* idTok = consume(Token::ID, "Se esperaba nombre del identificador en la declaración");
    string name = idTok->text;

    if (check(Token::LPAREN)) {
        // Es una función!
        advance(); // consume '('
        vector<Param*> params = parseParamDeclList();
        consume(Token::RPAREN, "Se esperaba ')' tras la lista de parámetros");
        BlockStmt* body = parseBlock();
        return new FunDecl(type, name, params, body, idTok->line, idTok->col);
    } else {
        // Es una declaración de variable
        VarDecl* vdecl = new VarDecl(type, idTok->line, idTok->col);

        // Primer declarador ya consumió idTok->text (con los posibles * aplicados en parseReturnType)
        vector<Exp*> dims;
        if (check(Token::LBRACKET)) {
            dims = parseArrayDims();
        }
        Exp* init = nullptr;
        if (match(Token::ASSIGN)) {
            init = parseInitializer();
        }
        vdecl->inits.push_back(new VarInit(name, type.ptrLevel, type.isRef, dims, init, idTok->line, idTok->col));

        // Declaraciones adicionales en la misma línea: , var2, *var3
        while (match(Token::COMMA)) {
            vdecl->inits.push_back(parseVarInit(type));
        }

        consume(Token::SEMICOL, "Se esperaba ';' al final de la declaración de variables");
        return vdecl;
    }
}

VarInit* Parser::parseVarInit(const Type& /*baseType*/) {
    int ptrs = 0;
    while (match(Token::MUL)) ptrs++;

    bool isRef = false;
    if (match(Token::AMP)) isRef = true;

    Token* idTok = consume(Token::ID, "Se esperaba identificador en la declaración");
    vector<Exp*> dims;
    if (check(Token::LBRACKET)) {
        dims = parseArrayDims();
    }

    Exp* init = nullptr;
    if (match(Token::ASSIGN)) {
        init = parseInitializer();
    } else if (isRef) {
        error("Las referencias en C++ deben ser inicializadas obligatoriamente");
    }

    return new VarInit(idTok->text, ptrs, isRef, dims, init, idTok->line, idTok->col);
}

VarDecl* Parser::parseVarDecl() {
    int l = current->line;
    int c = current->col;
    Type bt = parseBaseType();
    VarDecl* decl = new VarDecl(bt, l, c);

    do {
        decl->inits.push_back(parseVarInit(bt));
    } while (match(Token::COMMA));

    consume(Token::SEMICOL, "Se esperaba ';' al final de la declaración de variable");
    return decl;
}

VarDecl* Parser::parseVarDeclNoSemicolon() {
    int l = current->line;
    int c = current->col;
    Type bt = parseBaseType();
    VarDecl* decl = new VarDecl(bt, l, c);

    do {
        decl->inits.push_back(parseVarInit(bt));
    } while (match(Token::COMMA));

    return decl;
}

vector<Param*> Parser::parseParamDeclList() {
    vector<Param*> params;
    if (check(Token::RPAREN)) return params;

    params.push_back(parseParam());
    while (match(Token::COMMA)) {
        params.push_back(parseParam());
    }
    return params;
}

Param* Parser::parseParam() {
    int l = current->line;
    int c = current->col;
    Type bt = parseBaseType();

    int ptrs = 0;
    while (match(Token::MUL)) ptrs++;

    bool isRef = false;
    if (match(Token::AMP)) isRef = true;

    Token* idTok = consume(Token::ID, "Se esperaba identificador de parámetro");
    vector<Exp*> dims;
    if (check(Token::LBRACKET)) {
        dims = parseArrayDims();
    }

    return new Param(bt, idTok->text, ptrs, isRef, dims, l, c);
}

RecordDecl* Parser::parseRecordDecl() {
    int l = current->line;
    int c = current->col;
    RecordKind kind = match(Token::KW_CLASS) ? RECORD_CLASS : RECORD_STRUCT;
    if (kind == RECORD_STRUCT) {
        consume(Token::KW_STRUCT, "Se esperaba 'class' o 'struct'");
    }

    Token* nameTok = consume(Token::ID, "Se esperaba nombre de clase/struct");
    string recordName = nameTok->text;
    knownTypes.insert(recordName);

    consume(Token::LBRACE, "Se esperaba '{' al iniciar definición de clase/struct");

    vector<RecordMember*> members;
    AccessSpecifier currentAccess = (kind == RECORD_CLASS) ? ACCESS_PRIVATE : ACCESS_PUBLIC;

    while (!check(Token::RBRACE) && !isAtEnd()) {
        if (match(Token::KW_PUBLIC)) {
            consume(Token::COLON, "Se esperaba ':' después de 'public'");
            currentAccess = ACCESS_PUBLIC;
            continue;
        } else if (match(Token::KW_PRIVATE)) {
            consume(Token::COLON, "Se esperaba ':' después de 'private'");
            currentAccess = ACCESS_PRIVATE;
            continue;
        }

        // Miembro: VarDecl o FunDecl
        Type type = parseReturnType();
        Token* id = consume(Token::ID, "Se esperaba nombre del miembro");
        if (check(Token::LPAREN)) {
            advance(); // '('
            vector<Param*> params = parseParamDeclList();
            consume(Token::RPAREN, "Se esperaba ')' tras parámetros del método");
            BlockStmt* body = parseBlock();
            FunDecl* fd = new FunDecl(type, id->text, params, body, id->line, id->col);
            members.push_back(new RecordMember(currentAccess, nullptr, fd, id->line, id->col));
        } else {
            VarDecl* vd = new VarDecl(type, id->line, id->col);
            vector<Exp*> dims;
            if (check(Token::LBRACKET)) dims = parseArrayDims();
            Exp* init = nullptr;
            if (match(Token::ASSIGN)) init = parseInitializer();
            vd->inits.push_back(new VarInit(id->text, type.ptrLevel, type.isRef, dims, init, id->line, id->col));
            while (match(Token::COMMA)) {
                vd->inits.push_back(parseVarInit(type));
            }
            consume(Token::SEMICOL, "Se esperaba ';' tras declaración de campo");
            members.push_back(new RecordMember(currentAccess, vd, nullptr, id->line, id->col));
        }
    }

    consume(Token::RBRACE, "Se esperaba '}' al cerrar clase/struct");
    consume(Token::SEMICOL, "Se esperaba ';' al final de clase/struct");

    return new RecordDecl(kind, recordName, members, l, c);
}

// =========================================================================
// Bloques y Sentencias
// =========================================================================

BlockStmt* Parser::parseBlock() {
    int l = current->line;
    int c = current->col;
    consume(Token::LBRACE, "Se esperaba '{' al inicio del bloque");

    BlockStmt* block = new BlockStmt(l, c);

    while (!check(Token::RBRACE) && !isAtEnd()) {
        if (isBaseTypeStart()) {
            block->items.push_back(parseVarDecl());
        } else {
            block->items.push_back(parseStmt());
        }
    }

    consume(Token::RBRACE, "Se esperaba '}' al cerrar el bloque");
    return block;
}

Stmt* Parser::parseStmt() {
    if (check(Token::KW_IF))       return parseIfStmt();
    if (check(Token::KW_WHILE))    return parseWhileStmt();
    if (check(Token::KW_FOR))      return parseForStmt();
    if (check(Token::KW_DO))       return parseDoWhileStmt();
    if (check(Token::KW_BREAK))    return parseBreakStmt();
    if (check(Token::KW_CONTINUE)) return parseContinueStmt();
    if (check(Token::KW_RETURN))   return parseReturnStmt();
    if (check(Token::LBRACE))      return parseBlock();

    return parseExprStmt();
}

IfStmt* Parser::parseIfStmt() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_IF, "Se esperaba 'if'");
    consume(Token::LPAREN, "Se esperaba '(' después de 'if'");
    Exp* cond = parseCExp();
    consume(Token::RPAREN, "Se esperaba ')' tras condición de 'if'");

    Stmt* thenStmt = parseStmt();
    Stmt* elseStmt = nullptr;

    // Resuelve ambigüedad dangling-else asociándolo al if más cercano
    if (match(Token::KW_ELSE)) {
        elseStmt = parseStmt();
    }

    return new IfStmt(cond, thenStmt, elseStmt, l, c);
}

WhileStmt* Parser::parseWhileStmt() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_WHILE, "Se esperaba 'while'");
    consume(Token::LPAREN, "Se esperaba '(' después de 'while'");
    Exp* cond = parseCExp();
    consume(Token::RPAREN, "Se esperaba ')' tras condición de 'while'");

    loopDepth++;
    Stmt* body = parseStmt();
    loopDepth--;

    return new WhileStmt(cond, body, l, c);
}

ForStmt* Parser::parseForStmt() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_FOR, "Se esperaba 'for'");
    consume(Token::LPAREN, "Se esperaba '(' después de 'for'");

    // ForInit ::= VarDeclNoSemicolon | CExp { "," CExp } | vacio
    VarDecl* initDecl = nullptr;
    vector<Exp*> initExprs;
    if (isBaseTypeStart()) {
        initDecl = parseVarDeclNoSemicolon();
    } else if (!check(Token::SEMICOL)) {
        initExprs.push_back(parseCExp());
        while (match(Token::COMMA)) {
            initExprs.push_back(parseCExp());
        }
    }
    consume(Token::SEMICOL, "Se esperaba ';' en cabecera de for (init)");

    // Condición [CExp]
    Exp* cond = nullptr;
    if (!check(Token::SEMICOL)) {
        cond = parseCExp();
    }
    consume(Token::SEMICOL, "Se esperaba ';' en cabecera de for (cond)");

    // Actualización [ForUpdate]
    vector<Exp*> updateExprs;
    if (!check(Token::RPAREN)) {
        updateExprs.push_back(parseCExp());
        while (match(Token::COMMA)) {
            updateExprs.push_back(parseCExp());
        }
    }
    consume(Token::RPAREN, "Se esperaba ')' al cerrar cabecera de for");

    loopDepth++;
    Stmt* body = parseStmt();
    loopDepth--;

    return new ForStmt(initDecl, initExprs, cond, updateExprs, body, l, c);
}

DoWhileStmt* Parser::parseDoWhileStmt() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_DO, "Se esperaba 'do'");

    loopDepth++;
    Stmt* body = parseStmt();
    loopDepth--;

    consume(Token::KW_WHILE, "Se esperaba 'while' después del cuerpo do");
    consume(Token::LPAREN, "Se esperaba '(' después de 'while'");
    Exp* cond = parseCExp();
    consume(Token::RPAREN, "Se esperaba ')' tras condición de do-while");
    consume(Token::SEMICOL, "Se esperaba ';' al final de do-while");

    return new DoWhileStmt(body, cond, l, c);
}

BreakStmt* Parser::parseBreakStmt() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_BREAK, "Se esperaba 'break'");
    consume(Token::SEMICOL, "Se esperaba ';' después de 'break'");
    if (loopDepth <= 0) {
        error("'break' solo es válido dentro de un bucle (while, for, do-while)");
    }
    return new BreakStmt(l, c);
}

ContinueStmt* Parser::parseContinueStmt() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_CONTINUE, "Se esperaba 'continue'");
    consume(Token::SEMICOL, "Se esperaba ';' después de 'continue'");
    if (loopDepth <= 0) {
        error("'continue' solo es válido dentro de un bucle (while, for, do-while)");
    }
    return new ContinueStmt(l, c);
}

ReturnStmt* Parser::parseReturnStmt() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_RETURN, "Se esperaba 'return'");

    Exp* expr = nullptr;
    if (!check(Token::SEMICOL)) {
        expr = parseCExp();
    }
    consume(Token::SEMICOL, "Se esperaba ';' después de 'return'");
    return new ReturnStmt(expr, l, c);
}

ExprStmt* Parser::parseExprStmt() {
    int l = current->line;
    int c = current->col;
    Exp* expr = parseCExp();
    consume(Token::SEMICOL, "Se esperaba ';' al final de la expresión");
    return new ExprStmt(expr, l, c);
}

// =========================================================================
// Expresiones con Jerarquía de Precedencia según EBNF
// =========================================================================

Exp* Parser::parseCExp() {
    return parseAssignExp();
}

// AssignExp ::= LogicOr [ "=" AssignExp ]
Exp* Parser::parseAssignExp() {
    Exp* left = parseLogicOr();
    if (match(Token::ASSIGN)) {
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseAssignExp(); // Asociatividad por derecha
        return new AssignExp(left, right, l, c);
    }
    return left;
}

// LogicOr ::= LogicAnd { "||" LogicAnd }
Exp* Parser::parseLogicOr() {
    Exp* expr = parseLogicAnd();
    while (match(Token::OR)) {
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseLogicAnd();
        expr = new BinaryExp(expr, right, OR_OP, l, c);
    }
    return expr;
}

// LogicAnd ::= BitOr { "&&" BitOr }
Exp* Parser::parseLogicAnd() {
    Exp* expr = parseBitOr();
    while (match(Token::AND)) {
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseBitOr();
        expr = new BinaryExp(expr, right, AND_OP, l, c);
    }
    return expr;
}

// BitOr ::= BitXor { "|" BitXor }
Exp* Parser::parseBitOr() {
    Exp* expr = parseBitXor();
    while (match(Token::BIT_OR)) {
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseBitXor();
        expr = new BinaryExp(expr, right, BIT_OR_OP, l, c);
    }
    return expr;
}

// BitXor ::= BitAnd { "^" BitAnd }
Exp* Parser::parseBitXor() {
    Exp* expr = parseBitAnd();
    while (match(Token::BIT_XOR)) {
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseBitAnd();
        expr = new BinaryExp(expr, right, BIT_XOR_OP, l, c);
    }
    return expr;
}

// BitAnd ::= EqExp { "&" EqExp }
Exp* Parser::parseBitAnd() {
    Exp* expr = parseEqExp();
    while (match(Token::AMP)) {
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseEqExp();
        expr = new BinaryExp(expr, right, BIT_AND_OP, l, c);
    }
    return expr;
}

// EqExp ::= RelExp { ("==" | "!=") RelExp }
Exp* Parser::parseEqExp() {
    Exp* expr = parseRelExp();
    while (check(Token::EQ) || check(Token::NE)) {
        advance();
        BinaryOp op = (previous->type == Token::EQ) ? EQ_OP : NE_OP;
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseRelExp();
        expr = new BinaryExp(expr, right, op, l, c);
    }
    return expr;
}

// RelExp ::= ShiftExp { RelOp ShiftExp }
Exp* Parser::parseRelExp() {
    Exp* expr = parseShiftExp();
    while (check(Token::LT) || check(Token::LE) || check(Token::GT) || check(Token::GE)) {
        advance();
        BinaryOp op;
        switch (previous->type) {
            case Token::LT: op = LT_OP; break;
            case Token::LE: op = LE_OP; break;
            case Token::GT: op = GT_OP; break;
            case Token::GE: op = GE_OP; break;
            default: op = LT_OP; break;
        }
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseShiftExp();
        expr = new BinaryExp(expr, right, op, l, c);
    }
    return expr;
}

// ShiftExp ::= Exp { ("<<" | ">>") Exp }
Exp* Parser::parseShiftExp() {
    Exp* expr = parseExp();
    while (check(Token::LSHIFT) || check(Token::RSHIFT)) {
        advance();
        BinaryOp op = (previous->type == Token::LSHIFT) ? LSHIFT_OP : RSHIFT_OP;
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseExp();
        expr = new BinaryExp(expr, right, op, l, c);
    }
    return expr;
}

// Exp ::= Term { ("+" | "-") Term }
Exp* Parser::parseExp() {
    Exp* expr = parseTerm();
    while (check(Token::PLUS) || check(Token::MINUS)) {
        advance();
        BinaryOp op = (previous->type == Token::PLUS) ? PLUS_OP : MINUS_OP;
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseTerm();
        expr = new BinaryExp(expr, right, op, l, c);
    }
    return expr;
}

// Term ::= Unary { ("*" | "/" | "%") Unary }
Exp* Parser::parseTerm() {
    Exp* expr = parseUnary();
    while (check(Token::MUL) || check(Token::DIV) || check(Token::MOD)) {
        advance();
        BinaryOp op;
        switch (previous->type) {
            case Token::MUL: op = MUL_OP; break;
            case Token::DIV: op = DIV_OP; break;
            case Token::MOD: op = MOD_OP; break;
            default: op = MUL_OP; break;
        }
        int l = previous->line;
        int c = previous->col;
        Exp* right = parseUnary();
        expr = new BinaryExp(expr, right, op, l, c);
    }
    return expr;
}

// Unary
Exp* Parser::parseUnary() {
    int l = current->line;
    int c = current->col;

    if (match(Token::PLUS))     return new UnaryExp(parseUnary(), PLUS_UNARY, l, c);
    if (match(Token::MINUS))    return new UnaryExp(parseUnary(), MINUS_UNARY, l, c);
    if (match(Token::NOT))      return new UnaryExp(parseUnary(), NOT_UNARY, l, c);
    if (match(Token::BIT_NOT))  return new UnaryExp(parseUnary(), BIT_NOT_UNARY, l, c);
    if (match(Token::MUL))      return new UnaryExp(parseUnary(), DEREF_UNARY, l, c);
    if (match(Token::AMP))      return new UnaryExp(parseUnary(), ADDR_UNARY, l, c);
    if (match(Token::INC))      return new UnaryExp(parseUnary(), PRE_INC_UNARY, l, c);
    if (match(Token::DEC))      return new UnaryExp(parseUnary(), PRE_DEC_UNARY, l, c);

    if (check(Token::KW_DELETE)) return parseDeleteExpr();
    if (check(Token::KW_NEW))    return parseNewExpr();

    return parsePostfix();
}

Exp* Parser::parseDeleteExpr() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_DELETE, "Se esperaba 'delete'");

    bool isArray = false;
    if (match(Token::LBRACKET)) {
        consume(Token::RBRACKET, "Se esperaba ']' después de 'delete['");
        isArray = true;
    }

    Exp* exp = parseUnary();
    return new DeleteExp(isArray, exp, l, c);
}

Exp* Parser::parseNewExpr() {
    int l = current->line;
    int c = current->col;
    consume(Token::KW_NEW, "Se esperaba 'new'");

    Type targetType = parseBaseType();
    while (match(Token::MUL)) {
        targetType.ptrLevel++;
    }

    vector<Exp*> dims;
    if (check(Token::LBRACKET)) {
        dims = parseArrayDims();
    }

    return new NewExp(targetType, dims, l, c);
}

// Postfix ::= Primary { PostfixSuffix }
Exp* Parser::parsePostfix() {
    Exp* expr = parsePrimary();

    while (true) {
        int l = current ? current->line : 1;
        int c = current ? current->col : 1;

        if (match(Token::LBRACKET)) {
            Exp* idx = parseCExp();
            consume(Token::RBRACKET, "Se esperaba ']' al cerrar indexación de arreglo");
            PostfixExp* post = new PostfixExp(expr, ARRAY_ACCESS, l, c);
            post->indexExp = idx;
            expr = post;
        } else if (match(Token::DOT)) {
            Token* member = consume(Token::ID, "Se esperaba nombre de campo después de '.'");
            PostfixExp* post = new PostfixExp(expr, DOT_ACCESS, l, c);
            post->memberName = member->text;
            expr = post;
        } else if (match(Token::ARROW)) {
            Token* member = consume(Token::ID, "Se esperaba nombre de campo después de '->'");
            PostfixExp* post = new PostfixExp(expr, ARROW_ACCESS, l, c);
            post->memberName = member->text;
            expr = post;
        } else if (match(Token::LPAREN)) {
            vector<Exp*> args = parseArgList();
            consume(Token::RPAREN, "Se esperaba ')' tras los argumentos de la función");
            PostfixExp* post = new PostfixExp(expr, CALL_ACCESS, l, c);
            post->args = args;
            expr = post;
        } else if (match(Token::INC)) {
            expr = new PostfixExp(expr, POST_INC, l, c);
        } else if (match(Token::DEC)) {
            expr = new PostfixExp(expr, POST_DEC, l, c);
        } else {
            break;
        }
    }

    return expr;
}

// Primary
Exp* Parser::parsePrimary() {
    int l = current ? current->line : 1;
    int c = current ? current->col : 1;

    if (match(Token::NUM)) {
        long long val = stoll(previous->text);
        return new NumberExp(val, l, c);
    }
    if (match(Token::REAL)) {
        double val = stod(previous->text);
        return new RealExp(val, l, c);
    }
    if (match(Token::KW_TRUE)) {
        return new BoolExp(true, l, c);
    }
    if (match(Token::KW_FALSE)) {
        return new BoolExp(false, l, c);
    }
    if (match(Token::CHAR_LITERAL)) {
        char ch = previous->text.empty() ? '\0' : previous->text[0];
        return new CharExp(ch, l, c);
    }
    if (match(Token::STRING_LITERAL)) {
        return new StringExp(previous->text, l, c);
    }
    if (match(Token::KW_NULLPTR)) {
        return new NullptrExp(l, c);
    }
    if (match(Token::LPAREN)) {
        Exp* exp = parseCExp();
        consume(Token::RPAREN, "Se esperaba ')' tras expresión entre paréntesis");
        return exp;
    }
    if (check(Token::ID)) {
        vector<string> qid = parseQualifiedId();
        return new IdExp(qid, l, c);
    }

    error("Expresión no reconocida o inesperada: '" + (current ? current->text : "EOF") + "'");
    return nullptr;
}

Exp* Parser::parseInitializer() {
    if (check(Token::LBRACE)) {
        return parseBracedInitList();
    }
    return parseCExp();
}

BracedInitList* Parser::parseBracedInitList() {
    int l = current->line;
    int c = current->col;
    consume(Token::LBRACE, "Se esperaba '{' al inicio de la lista de inicialización");

    vector<Exp*> elems;
    if (!check(Token::RBRACE)) {
        elems.push_back(parseInitializer());
        while (match(Token::COMMA)) {
            if (check(Token::RBRACE)) break; // Coma al final permitida en C++
            elems.push_back(parseInitializer());
        }
    }

    consume(Token::RBRACE, "Se esperaba '}' al cerrar la lista de inicialización");
    return new BracedInitList(elems, l, c);
}

vector<Exp*> Parser::parseArrayDims() {
    vector<Exp*> dims;
    while (match(Token::LBRACKET)) {
        dims.push_back(parseCExp());
        consume(Token::RBRACKET, "Se esperaba ']' tras dimensión del arreglo");
    }
    return dims;
}

vector<Exp*> Parser::parseArgList() {
    vector<Exp*> args;
    if (check(Token::RPAREN)) return args;

    args.push_back(parseCExp());
    while (match(Token::COMMA)) {
        args.push_back(parseCExp());
    }
    return args;
}
