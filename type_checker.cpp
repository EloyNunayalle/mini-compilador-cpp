#include <iostream>
#include "type_checker.h"

using namespace std;

TypeChecker::TypeChecker()
    : currentFunction(nullptr), loopDepth(0) {}

void TypeChecker::addError(const string& msg, int line, int col) {
    string loc = (line > 0) ? "[Línea " + to_string(line) + ", Columna " + to_string(col) + "] " : "";
    errors.push_back("Error Semántico " + loc + msg);
}

void TypeChecker::addWarning(const string& msg, int line, int col) {
    string loc = (line > 0) ? "[Línea " + to_string(line) + ", Columna " + to_string(col) + "] " : "";
    warnings.push_back("Advertencia " + loc + msg);
}

bool TypeChecker::hasErrors() const {
    return !errors.empty();
}

void TypeChecker::printReport(ostream& out) const {
    if (!warnings.empty()) {
        out << "\n--- ADVERTENCIAS (" << warnings.size() << ") ---\n";
        for (const string& w : warnings) {
            out << "  " << w << "\n";
        }
    }
    if (!errors.empty()) {
        out << "\n--- ERRORES SEMÁNTICOS (" << errors.size() << ") ---\n";
        for (const string& e : errors) {
            out << "  " << e << "\n";
        }
    } else {
        out << "\nAnálisis semántico y comprobación de tipos completados exitosamente sin errores.\n";
    }
}

void TypeChecker::check(Program* prog) {
    if (!prog) return;

    env.clear();
    env.add_level(); // Ámbito global
    functions.clear();
    records.clear();
    errors.clear();
    warnings.clear();

    // Pase 1: Pre-registrar funciones y structs en el ámbito global
    for (ASTNode* decl : prog->declarations) {
        FunDecl* fd = dynamic_cast<FunDecl*>(decl);
        if (fd) {
            if (functions.count(fd->name)) {
                addError("Redefinición de la función '" + fd->name + "'", fd->line, fd->col);
            } else {
                functions[fd->name] = fd;
            }
        }
        RecordDecl* rd = dynamic_cast<RecordDecl*>(decl);
        if (rd) {
            if (records.count(rd->name)) {
                addError("Redefinición de clase/struct '" + rd->name + "'", rd->line, rd->col);
            } else {
                records[rd->name] = rd;
            }
        }
    }

    // Pase 2: Visitar el AST completo
    prog->accept(this);

    // Verificación de la función main
    if (!functions.count("main")) {
        addWarning("El programa no contiene la función 'main' requerida para la ejecución en C++.");
    }
}

// =========================================================================
// Expresiones
// =========================================================================

Type TypeChecker::visit(BinaryExp* exp) {
    Type lt = exp->left->accept(this);
    Type rt = exp->right->accept(this);

    if (lt.isError() || rt.isError()) return Type::makeError();

    switch (exp->op) {
        case PLUS_OP:
        case MINUS_OP:
        case MUL_OP:
        case DIV_OP: {
            if (!lt.isNumeric() || !rt.isNumeric()) {
                addError("Los operandos de '" + Exp::binopToString(exp->op) +
                         "' deben ser numéricos (se recibió " + lt.toString() + " y " + rt.toString() + ")",
                         exp->line, exp->col);
                return Type::makeError();
            }
            if (lt.isDouble() || rt.isDouble()) return Type(Type::DOUBLE);
            if (lt.isFloat() || rt.isFloat())   return Type(Type::FLOAT);
            return Type(Type::INT);
        }

        case MOD_OP: {
            if (!lt.isInt() && lt.kind != Type::CHAR && lt.kind != Type::BOOL) {
                addError("El operador '%' requiere operandos enteros", exp->line, exp->col);
                return Type::makeError();
            }
            return Type(Type::INT);
        }

        case EQ_OP:
        case NE_OP: {
            if (!lt.isCompatibleWith(rt) && !rt.isCompatibleWith(lt)) {
                addWarning("Comparación de igualdad entre tipos potencialmente incompatibles (" +
                           lt.toString() + " y " + rt.toString() + ")", exp->line, exp->col);
            }
            return Type(Type::BOOL);
        }

        case LT_OP:
        case LE_OP:
        case GT_OP:
        case GE_OP: {
            if (!lt.isNumeric() || !rt.isNumeric()) {
                addError("Los operadores relacionales requieren operandos numéricos", exp->line, exp->col);
                return Type::makeError();
            }
            return Type(Type::BOOL);
        }

        case AND_OP:
        case OR_OP: {
            return Type(Type::BOOL);
        }

        case BIT_AND_OP:
        case BIT_OR_OP:
        case BIT_XOR_OP:
        case LSHIFT_OP:
        case RSHIFT_OP: {
            return Type(Type::INT);
        }
    }

    return Type::makeError();
}

Type TypeChecker::visit(UnaryExp* exp) {
    Type t = exp->exp->accept(this);
    if (t.isError()) return Type::makeError();

    switch (exp->op) {
        case PLUS_UNARY:
        case MINUS_UNARY:
            if (!t.isNumeric()) {
                addError("Operando unario '+/-' requiere tipo numérico", exp->line, exp->col);
                return Type::makeError();
            }
            return t;

        case NOT_UNARY:
            return Type(Type::BOOL);

        case BIT_NOT_UNARY:
            return Type(Type::INT);

        case DEREF_UNARY: {
            if (t.ptrLevel <= 0) {
                addError("No se puede desreferenciar un tipo no puntero: " + t.toString(), exp->line, exp->col);
                return Type::makeError();
            }
            Type res = t;
            res.ptrLevel--;
            return res;
        }

        case ADDR_UNARY: {
            Type res = t;
            res.ptrLevel++;
            return res;
        }

        case PRE_INC_UNARY:
        case PRE_DEC_UNARY:
            if (!t.isNumeric() && t.ptrLevel <= 0) {
                addError("Operador '++ / --' requiere tipo numérico o puntero", exp->line, exp->col);
                return Type::makeError();
            }
            return t;
    }
    return Type::makeError();
}

Type TypeChecker::visit(AssignExp* exp) {
    Type lt = exp->target->accept(this);
    Type rt = exp->value->accept(this);

    if (lt.isError() || rt.isError()) return Type::makeError();

    if (!lt.isCompatibleWith(rt)) {
        addError("No se puede asignar un valor de tipo '" + rt.toString() +
                 "' a una variable de tipo '" + lt.toString() + "'", exp->line, exp->col);
        return Type::makeError();
    }

    return lt;
}

Type TypeChecker::visit(NumberExp* exp) {
    return Type(static_cast<int>(exp->value));
}

Type TypeChecker::visit(RealExp* exp) {
    return Type(exp->value);
}

Type TypeChecker::visit(BoolExp* exp) {
    return Type(exp->value);
}

Type TypeChecker::visit(CharExp* exp) {
    return Type(exp->value);
}

Type TypeChecker::visit(StringExp* exp) {
    return Type(exp->value);
}

Type TypeChecker::visit(NullptrExp* /*exp*/) {
    Type t(Type::INT);
    t.intValue = 0;
    t.isConstExpr = true;
    return t;
}

Type TypeChecker::visit(IdExp* exp) {
    string name = exp->getFullName();
    if (!env.check(name)) {
        // Podría ser una función referenciada
        if (functions.count(name)) {
            return functions[name]->returnType;
        }
        addError("Identificador no declarado: '" + name + "'", exp->line, exp->col);
        return Type::makeError();
    }
    return env.lookup(name);
}

Type TypeChecker::visit(PostfixExp* exp) {
    Type bt = exp->base->accept(this);

    switch (exp->kind) {
        case POST_INC:
        case POST_DEC:
            return bt;

        case ARRAY_ACCESS: {
            if (bt.ptrLevel <= 0 && bt.arrayDims.empty()) {
                addError("Subíndice '[]' aplicado a un tipo que no es arreglo ni puntero: " + bt.toString(),
                         exp->line, exp->col);
                return Type::makeError();
            }
            if (exp->indexExp) {
                Type it = exp->indexExp->accept(this);
                if (!it.isNumeric()) {
                    addError("El índice del arreglo debe ser un número entero", exp->line, exp->col);
                }
            }
            Type res = bt;
            if (res.ptrLevel > 0) res.ptrLevel--;
            return res;
        }

        case DOT_ACCESS: {
            if (bt.kind != Type::RECORD) {
                addError("Acceso a miembro '.' sobre tipo no estructurado: " + bt.toString(), exp->line, exp->col);
                return Type::makeError();
            }
            if (records.count(bt.recordName)) {
                RecordDecl* rd = records[bt.recordName];
                for (RecordMember* m : rd->members) {
                    if (m->varDecl) {
                        for (VarInit* vi : m->varDecl->inits) {
                            if (vi->name == exp->memberName) {
                                Type t = m->varDecl->baseType;
                                t.ptrLevel = vi->ptrCount;
                                t.isRef = vi->isRef;
                                return t;
                            }
                        }
                    }
                    if (m->funDecl && m->funDecl->name == exp->memberName) {
                        return m->funDecl->returnType;
                    }
                }
                addError("El miembro '" + exp->memberName + "' no existe en '" + bt.recordName + "'",
                         exp->line, exp->col);
            }
            return Type::makeError();
        }

        case ARROW_ACCESS: {
            if (bt.ptrLevel != 1 || bt.kind != Type::RECORD) {
                addError("Acceso '->' requiere un puntero a struct/class (se recibió " + bt.toString() + ")",
                         exp->line, exp->col);
                return Type::makeError();
            }
            if (records.count(bt.recordName)) {
                RecordDecl* rd = records[bt.recordName];
                for (RecordMember* m : rd->members) {
                    if (m->varDecl) {
                        for (VarInit* vi : m->varDecl->inits) {
                            if (vi->name == exp->memberName) {
                                Type t = m->varDecl->baseType;
                                t.ptrLevel = vi->ptrCount;
                                t.isRef = vi->isRef;
                                return t;
                            }
                        }
                    }
                }
                addError("El miembro '" + exp->memberName + "' no existe en '" + bt.recordName + "'",
                         exp->line, exp->col);
            }
            return Type::makeError();
        }

        case CALL_ACCESS: {
            IdExp* id = dynamic_cast<IdExp*>(exp->base);
            if (!id) {
                addError("Llamada a función sobre una expresión que no es un identificador", exp->line, exp->col);
                return Type::makeError();
            }
            string fname = id->getFullName();
            if (!functions.count(fname)) {
                // Funciones intrínsecas o de librerías comunes
                if (fname == "print" || fname == "printf" || fname == "sqrt") {
                    return Type(Type::INT);
                }
                addError("Llamada a función no declarada: '" + fname + "'", exp->line, exp->col);
                return Type::makeError();
            }

            FunDecl* fd = functions[fname];
            if (exp->args.size() != fd->params.size()) {
                addError("La función '" + fname + "' espera " + to_string(fd->params.size()) +
                         " argumentos, pero recibió " + to_string(exp->args.size()), exp->line, exp->col);
            } else {
                for (size_t i = 0; i < exp->args.size(); i++) {
                    Type at = exp->args[i]->accept(this);
                    Type pt = fd->params[i]->type;
                    pt.ptrLevel = fd->params[i]->ptrCount;
                    pt.isRef = fd->params[i]->isRef;
                    if (!pt.isCompatibleWith(at)) {
                        addError("Argumento " + to_string(i + 1) + " de '" + fname + "' tiene tipo '" +
                                 at.toString() + "', pero se esperaba '" + pt.toString() + "'",
                                 exp->line, exp->col);
                    }
                }
            }
            return fd->returnType;
        }
    }

    return Type::makeError();
}

Type TypeChecker::visit(NewExp* exp) {
    Type t = exp->allocType;
    t.ptrLevel++;
    for (Exp* d : exp->arrayDims) {
        Type dt = d->accept(this);
        if (!dt.isNumeric()) {
            addError("La dimensión en 'new' debe ser entera", exp->line, exp->col);
        }
    }
    return t;
}

Type TypeChecker::visit(DeleteExp* exp) {
    Type t = exp->exp->accept(this);
    if (t.ptrLevel <= 0) {
        addError("Operador 'delete' requiere un puntero", exp->line, exp->col);
    }
    return Type(Type::VOID);
}

Type TypeChecker::visit(BracedInitList* exp) {
    for (Exp* e : exp->elements) {
        e->accept(this);
    }
    return Type();
}

// =========================================================================
// Sentencias
// =========================================================================

void TypeChecker::visit(BlockStmt* stm) {
    env.add_level();
    for (ASTNode* item : stm->items) {
        item->accept(this);
    }
    env.remove_level();
}

void TypeChecker::visit(ExprStmt* stm) {
    if (stm->expr) {
        stm->expr->accept(this);
    }
}

void TypeChecker::visit(IfStmt* stm) {
    stm->condition->accept(this);
    stm->thenStmt->accept(this);
    if (stm->elseStmt) {
        stm->elseStmt->accept(this);
    }
}

void TypeChecker::visit(WhileStmt* stm) {
    stm->condition->accept(this);
    loopDepth++;
    stm->body->accept(this);
    loopDepth--;
}

void TypeChecker::visit(ForStmt* stm) {
    env.add_level();
    if (stm->initDecl) stm->initDecl->accept(this);
    for (Exp* e : stm->initExprs) e->accept(this);
    if (stm->condition) stm->condition->accept(this);
    for (Exp* e : stm->updateExprs) e->accept(this);

    loopDepth++;
    stm->body->accept(this);
    loopDepth--;
    env.remove_level();
}

void TypeChecker::visit(DoWhileStmt* stm) {
    loopDepth++;
    stm->body->accept(this);
    loopDepth--;
    stm->condition->accept(this);
}

void TypeChecker::visit(BreakStmt* stm) {
    if (loopDepth <= 0) {
        addError("Sentencia 'break' utilizada fuera de un bucle", stm->line, stm->col);
    }
}

void TypeChecker::visit(ContinueStmt* stm) {
    if (loopDepth <= 0) {
        addError("Sentencia 'continue' utilizada fuera de un bucle", stm->line, stm->col);
    }
}

void TypeChecker::visit(ReturnStmt* stm) {
    if (!currentFunction) {
        addError("Sentencia 'return' fuera de una función", stm->line, stm->col);
        return;
    }

    Type retType = currentFunction->returnType;
    if (retType.isVoid()) {
        if (stm->expr) {
            addError("Función void '" + currentFunction->name + "' no debe devolver un valor", stm->line, stm->col);
        }
    } else {
        if (!stm->expr) {
            addError("Función no void '" + currentFunction->name + "' debe devolver una expresión de tipo '" +
                     retType.toString() + "'", stm->line, stm->col);
        } else {
            Type et = stm->expr->accept(this);
            if (!retType.isCompatibleWith(et)) {
                addError("Tipo de retorno incompatible en '" + currentFunction->name +
                         "': se esperaba '" + retType.toString() + "', pero se devolvió '" + et.toString() + "'",
                         stm->line, stm->col);
            }
        }
    }
}

// =========================================================================
// Declaraciones y Directivas
// =========================================================================

void TypeChecker::visit(VarInit* /*init*/) {
    // Manejado en VarDecl
}

void TypeChecker::visit(VarDecl* decl) {
    for (VarInit* vi : decl->inits) {
        if (env.check_current_level(vi->name)) {
            addError("Redeclaración de variable en el mismo ámbito: '" + vi->name + "'", vi->line, vi->col);
        }

        Type t = decl->baseType;
        t.ptrLevel = vi->ptrCount;
        t.isRef = vi->isRef;
        if (!vi->arrayDims.empty()) {
            t.ptrLevel += vi->arrayDims.size();
        }

        if (vi->initializer) {
            Type it = vi->initializer->accept(this);
            if (!t.isCompatibleWith(it)) {
                addError("Inicialización incompatible para variable '" + vi->name +
                         "': tipo '" + t.toString() + "' inicializado con '" + it.toString() + "'",
                         vi->line, vi->col);
            }
        }

        env.add_var(vi->name, t);
    }
}

void TypeChecker::visit(Param* param) {
    Type t = param->type;
    t.ptrLevel = param->ptrCount;
    t.isRef = param->isRef;
    if (!param->arrayDims.empty()) {
        t.ptrLevel += param->arrayDims.size();
    }

    if (env.check_current_level(param->name)) {
        addError("Nombre de parámetro duplicado: '" + param->name + "'", param->line, param->col);
    }
    env.add_var(param->name, t);
}

void TypeChecker::visit(FunDecl* fun) {
    currentFunction = fun;
    env.add_level(); // Ámbito de la función

    for (Param* p : fun->params) {
        p->accept(this);
    }

    if (fun->body) {
        // En lugar de llamar a fun->body->accept que crearía otro nivel,
        // visitamos sus items en el nivel de parámetros
        for (ASTNode* item : fun->body->items) {
            item->accept(this);
        }
    }

    env.remove_level();
    currentFunction = nullptr;
}

void TypeChecker::visit(RecordMember* member) {
    if (member->varDecl) member->varDecl->accept(this);
    if (member->funDecl) member->funDecl->accept(this);
}

void TypeChecker::visit(RecordDecl* record) {
    env.add_level();
    for (RecordMember* m : record->members) {
        m->accept(this);
    }
    env.remove_level();
}

void TypeChecker::visit(IncludeDirective* /*inc*/) {
    // Validar nombre de librería
}

void TypeChecker::visit(UsingDirective* /*usg*/) {
    // Registro de namespaces
}

void TypeChecker::visit(Program* prog) {
    for (IncludeDirective* inc : prog->includes) {
        inc->accept(this);
    }
    for (UsingDirective* usg : prog->usings) {
        usg->accept(this);
    }
    for (ASTNode* decl : prog->declarations) {
        decl->accept(this);
    }
}
