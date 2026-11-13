#include <climits>
#include "optimizer.h"

using namespace std;

namespace opt {

void childSlots(Exp* e, vector<Exp**>& out) {
    switch (e->nodeKind()) {
        case NodeKind::BINARY_EXP: {
            BinaryExp* b = static_cast<BinaryExp*>(e);
            out.push_back(&b->left);
            out.push_back(&b->right);
            break;
        }
        case NodeKind::UNARY_EXP:
            out.push_back(&static_cast<UnaryExp*>(e)->exp);
            break;
        case NodeKind::ASSIGN_EXP: {
            AssignExp* a = static_cast<AssignExp*>(e);
            out.push_back(&a->target);
            out.push_back(&a->value);
            break;
        }
        case NodeKind::POSTFIX_EXP: {
            PostfixExp* p = static_cast<PostfixExp*>(e);
            if (p->base) out.push_back(&p->base);
            if (p->indexExp) out.push_back(&p->indexExp);
            for (Exp*& a : p->args) out.push_back(&a);
            break;
        }
        case NodeKind::NEW_EXP: {
            NewExp* n = static_cast<NewExp*>(e);
            for (Exp*& d : n->arrayDims) out.push_back(&d);
            for (Exp*& a : n->ctorArgs) out.push_back(&a);
            break;
        }
        case NodeKind::DELETE_EXP:
            out.push_back(&static_cast<DeleteExp*>(e)->exp);
            break;
        case NodeKind::BRACED_INIT: {
            BracedInitList* bl = static_cast<BracedInitList*>(e);
            for (Exp*& x : bl->elements) out.push_back(&x);
            break;
        }
        default:
            break;
    }
}

bool isPlainInt(const Type& t) {
    return t.kind == Type::INT && t.ptrLevel == 0 && t.arrayDims.empty() && !t.isRef;
}

bool isLiteralInt(const Exp* e, long long& v) {
    switch (e->nodeKind()) {
        case NodeKind::NUMBER_EXP:
            v = static_cast<const NumberExp*>(e)->value;
            return v >= INT_MIN && v <= INT_MAX;
        case NodeKind::CHAR_EXP:
            v = static_cast<const CharExp*>(e)->value;
            return true;
        case NodeKind::BOOL_EXP:
            v = static_cast<const BoolExp*>(e)->value ? 1 : 0;
            return true;
        default:
            return false;
    }
}

// Pura: no escribe memoria, no llama funciones, no desreferencia punteros ni divide
// (la division puede fallar). Leer una variable es puro.
bool isPure(const Exp* e) {
    switch (e->nodeKind()) {
        case NodeKind::NUMBER_EXP:
        case NodeKind::REAL_EXP:
        case NodeKind::BOOL_EXP:
        case NodeKind::CHAR_EXP:
        case NodeKind::STRING_EXP:
        case NodeKind::NULLPTR_EXP:
            return true;
        case NodeKind::ID_EXP: {
            const IdExp* id = static_cast<const IdExp*>(e);
            return id->idKind == ID_VAR || id->idKind == ID_FIELD;
        }
        case NodeKind::UNARY_EXP: {
            const UnaryExp* u = static_cast<const UnaryExp*>(e);
            switch (u->op) {
                case PLUS_UNARY:
                case MINUS_UNARY:
                case NOT_UNARY:
                case BIT_NOT_UNARY:
                    return isPure(u->exp);
                default:
                    return false;
            }
        }
        case NodeKind::BINARY_EXP: {
            const BinaryExp* b = static_cast<const BinaryExp*>(e);
            if (b->mode != BM_NORMAL) return false;
            if (b->op == DIV_OP || b->op == MOD_OP) return false;
            return isPure(b->left) && isPure(b->right);
        }
        case NodeKind::POSTFIX_EXP: {
            const PostfixExp* p = static_cast<const PostfixExp*>(e);
            // un campo de un objeto nombrado no puede fallar
            return p->kind == DOT_ACCESS && isPure(p->base);
        }
        default:
            return false;
    }
}

}  // namespace opt

// Recorrido con reemplazo

Exp* RewriteVisitor::rw(Exp* e) {
    if (!e) return nullptr;
    e->accept(this);
    return post(e);
}

void RewriteVisitor::rwSlot(Exp*& slot) {
    if (slot) slot = rw(slot);
}

void RewriteVisitor::run(Program* prog) {
    prog->accept(this);
}

Type RewriteVisitor::visit(BinaryExp* exp) { rwSlot(exp->left); rwSlot(exp->right); return Type(); }
Type RewriteVisitor::visit(UnaryExp* exp) { rwSlot(exp->exp); return Type(); }
Type RewriteVisitor::visit(AssignExp* exp) { rwSlot(exp->target); rwSlot(exp->value); return Type(); }
Type RewriteVisitor::visit(NumberExp*) { return Type(); }
Type RewriteVisitor::visit(RealExp*) { return Type(); }
Type RewriteVisitor::visit(BoolExp*) { return Type(); }
Type RewriteVisitor::visit(CharExp*) { return Type(); }
Type RewriteVisitor::visit(StringExp*) { return Type(); }
Type RewriteVisitor::visit(NullptrExp*) { return Type(); }
Type RewriteVisitor::visit(IdExp*) { return Type(); }

Type RewriteVisitor::visit(PostfixExp* exp) {
    rwSlot(exp->base);
    rwSlot(exp->indexExp);
    for (Exp*& a : exp->args) rwSlot(a);
    return Type();
}

Type RewriteVisitor::visit(NewExp* exp) {
    for (Exp*& d : exp->arrayDims) rwSlot(d);
    for (Exp*& a : exp->ctorArgs) rwSlot(a);
    return Type();
}

Type RewriteVisitor::visit(DeleteExp* exp) { rwSlot(exp->exp); return Type(); }

Type RewriteVisitor::visit(BracedInitList* exp) {
    for (Exp*& x : exp->elements) rwSlot(x);
    return Type();
}

void RewriteVisitor::visit(BlockStmt* stm) {
    for (ASTNode* item : stm->items) item->accept(this);
}

void RewriteVisitor::visit(ExprStmt* stm) { rwSlot(stm->expr); }

void RewriteVisitor::visit(IfStmt* stm) {
    rwSlot(stm->condition);
    stm->thenStmt->accept(this);
    if (stm->elseStmt) stm->elseStmt->accept(this);
}

void RewriteVisitor::visit(WhileStmt* stm) {
    rwSlot(stm->condition);
    stm->body->accept(this);
}

void RewriteVisitor::visit(ForStmt* stm) {
    if (stm->initDecl) stm->initDecl->accept(this);
    for (Exp*& e : stm->initExprs) rwSlot(e);
    rwSlot(stm->condition);
    for (Exp*& e : stm->updateExprs) rwSlot(e);
    stm->body->accept(this);
}

void RewriteVisitor::visit(DoWhileStmt* stm) {
    stm->body->accept(this);
    rwSlot(stm->condition);
}

void RewriteVisitor::visit(BreakStmt*) {}
void RewriteVisitor::visit(ContinueStmt*) {}
void RewriteVisitor::visit(ReturnStmt* stm) { rwSlot(stm->expr); }

void RewriteVisitor::visit(VarInit* init) {
    rwSlot(init->initializer);
    for (Exp*& a : init->ctorArgs) rwSlot(a);
}

void RewriteVisitor::visit(VarDecl* decl) {
    for (VarInit* vi : decl->inits) vi->accept(this);
}

void RewriteVisitor::visit(Param*) {}

void RewriteVisitor::visit(FunDecl* fun) {
    if (wantsFunction(fun)) fun->body->accept(this);
}

void RewriteVisitor::visit(RecordMember* member) {
    if (member->funDecl) member->funDecl->accept(this);
}

void RewriteVisitor::visit(RecordDecl* record) {
    for (RecordMember* m : record->members) m->accept(this);
}

void RewriteVisitor::visit(IncludeDirective*) {}
void RewriteVisitor::visit(UsingDirective*) {}

void RewriteVisitor::visit(Program* prog) {
    program = prog;
    for (ASTNode* d : prog->declarations) {
        // los inicializadores globales no se tocan
        if (d->nodeKind() == NodeKind::VAR_DECL) continue;
        d->accept(this);
    }
}

// Fold

void FoldVisitor::note(const Exp* e, const string& what) {
    result.count++;
    result.details.push_back("L" + to_string(e->line) + ":C" + to_string(e->col) + " " + what);
}

Exp* FoldVisitor::post(Exp* e) {
    if (e->nodeKind() == NodeKind::BINARY_EXP) return foldBinary(static_cast<BinaryExp*>(e));
    if (e->nodeKind() == NodeKind::UNARY_EXP) return foldUnary(static_cast<UnaryExp*>(e));
    return e;
}

static Exp* takeLeft(BinaryExp* b) {
    Exp* l = b->left;
    b->left = nullptr;
    delete b;
    return l;
}

static Exp* takeRight(BinaryExp* b) {
    Exp* r = b->right;
    b->right = nullptr;
    delete b;
    return r;
}

static Exp* makeInt(long long v, const Exp* at) {
    NumberExp* n = new NumberExp(v, at->line, at->col);
    n->inferredType = Type(Type::INT);
    return n;
}

static Exp* makeBool(bool v, const Exp* at) {
    BoolExp* n = new BoolExp(v, at->line, at->col);
    n->inferredType = Type(Type::BOOL, 0, false);
    return n;
}

Exp* FoldVisitor::foldBinary(BinaryExp* b) {
    if (b->mode != BM_NORMAL) return b;
    const Type& ot = b->operandType;
    bool intOps = ot.ptrLevel == 0 && (ot.kind == Type::INT || ot.kind == Type::BOOL) && ot.arrayDims.empty();
    if (!intOps) return b;

    long long lv = 0, rv = 0;
    bool lc = opt::isLiteralInt(b->left, lv);
    bool rc = opt::isLiteralInt(b->right, rv);

    if (lc && rc) {
        int a = static_cast<int>(lv), c = static_cast<int>(rv), r = 0;
        switch (b->op) {
            case PLUS_OP:  if (__builtin_add_overflow(a, c, &r)) return b; note(b, "plegado " + to_string(a) + " + " + to_string(c)); return makeInt(r, b);
            case MINUS_OP: if (__builtin_sub_overflow(a, c, &r)) return b; note(b, "plegado " + to_string(a) + " - " + to_string(c)); return makeInt(r, b);
            case MUL_OP:   if (__builtin_mul_overflow(a, c, &r)) return b; note(b, "plegado " + to_string(a) + " * " + to_string(c)); return makeInt(r, b);
            case DIV_OP:
                if (c == 0 || (a == INT_MIN && c == -1)) return b;
                note(b, "plegado " + to_string(a) + " / " + to_string(c));
                return makeInt(a / c, b);
            case MOD_OP:
                if (c == 0 || (a == INT_MIN && c == -1)) return b;
                note(b, "plegado " + to_string(a) + " % " + to_string(c));
                return makeInt(a % c, b);
            case BIT_AND_OP: note(b, "plegado &"); return makeInt(a & c, b);
            case BIT_OR_OP:  note(b, "plegado |"); return makeInt(a | c, b);
            case BIT_XOR_OP: note(b, "plegado ^"); return makeInt(a ^ c, b);
            case LSHIFT_OP: {
                if (c < 0 || c > 30 || a < 0) return b;
                long long v = static_cast<long long>(a) << c;
                if (v > INT_MAX) return b;
                note(b, "plegado <<");
                return makeInt(v, b);
            }
            case RSHIFT_OP:
                if (c < 0 || c > 31) return b;
                note(b, "plegado >>");
                return makeInt(a >> c, b);
            case EQ_OP: note(b, "plegado =="); return makeBool(a == c, b);
            case NE_OP: note(b, "plegado !="); return makeBool(a != c, b);
            case LT_OP: note(b, "plegado <"); return makeBool(a < c, b);
            case LE_OP: note(b, "plegado <="); return makeBool(a <= c, b);
            case GT_OP: note(b, "plegado >"); return makeBool(a > c, b);
            case GE_OP: note(b, "plegado >="); return makeBool(a >= c, b);
            case AND_OP: note(b, "plegado &&"); return makeBool(a != 0 && c != 0, b);
            case OR_OP:  note(b, "plegado ||"); return makeBool(a != 0 || c != 0, b);
        }
        return b;
    }

    // cortocircuito: solo se elimina el lado derecho si es puro
    if (b->op == AND_OP || b->op == OR_OP) {
        bool isAnd = (b->op == AND_OP);
        if (lc) {
            bool decided = isAnd ? (lv == 0) : (lv != 0);
            if (decided && opt::isPure(b->right)) {
                note(b, isAnd ? "false && puro => false" : "true || puro => true");
                return makeBool(!isAnd, b);
            }
            if (!decided && b->right->inferredType.kind == Type::BOOL && b->right->inferredType.ptrLevel == 0) {
                note(b, isAnd ? "true && x => x" : "false || x => x");
                return takeRight(b);
            }
        }
        return b;
    }

    // identidades sobre enteros: el resultado conserva el tipo int del operando
    if (!opt::isPlainInt(b->inferredType)) return b;
    bool leftInt = opt::isPlainInt(b->left->inferredType);
    bool rightInt = opt::isPlainInt(b->right->inferredType);
    switch (b->op) {
        case PLUS_OP:
            if (rc && rv == 0 && leftInt) { note(b, "x + 0 => x"); return takeLeft(b); }
            if (lc && lv == 0 && rightInt) { note(b, "0 + x => x"); return takeRight(b); }
            break;
        case MINUS_OP:
            if (rc && rv == 0 && leftInt) { note(b, "x - 0 => x"); return takeLeft(b); }
            break;
        case MUL_OP:
            if (rc && rv == 1 && leftInt) { note(b, "x * 1 => x"); return takeLeft(b); }
            if (lc && lv == 1 && rightInt) { note(b, "1 * x => x"); return takeRight(b); }
            if (rc && rv == 0 && leftInt && opt::isPure(b->left)) { note(b, "x * 0 => 0 (x puro)"); return makeInt(0, b); }
            if (lc && lv == 0 && rightInt && opt::isPure(b->right)) { note(b, "0 * x => 0 (x puro)"); return makeInt(0, b); }
            if (rc && rv == 2 && leftInt && opt::isPure(b->left)) {
                note(b, "x * 2 => x + x");
                BinaryExp* s = new BinaryExp(b->left, cloneExp(b->left), PLUS_OP, b->line, b->col);
                s->operandType = Type(Type::INT);
                s->inferredType = Type(Type::INT);
                b->left = nullptr;
                delete b;
                return s;
            }
            if (lc && lv == 2 && rightInt && opt::isPure(b->right)) {
                note(b, "2 * x => x + x");
                BinaryExp* s = new BinaryExp(b->right, cloneExp(b->right), PLUS_OP, b->line, b->col);
                s->operandType = Type(Type::INT);
                s->inferredType = Type(Type::INT);
                b->right = nullptr;
                delete b;
                return s;
            }
            break;
        case DIV_OP:
            if (rc && rv == 1 && leftInt) { note(b, "x / 1 => x"); return takeLeft(b); }
            break;
        default:
            break;
    }
    return b;
}

Exp* FoldVisitor::foldUnary(UnaryExp* u) {
    long long v;
    if (!opt::isLiteralInt(u->exp, v)) return u;
    int a = static_cast<int>(v);
    switch (u->op) {
        case MINUS_UNARY:
            if (a == INT_MIN) return u;
            note(u, "plegado -" + to_string(a));
            return makeInt(-a, u);
        case PLUS_UNARY:
            note(u, "plegado +" + to_string(a));
            return makeInt(a, u);
        case BIT_NOT_UNARY:
            note(u, "plegado ~");
            return makeInt(~a, u);
        case NOT_UNARY:
            note(u, "plegado !");
            return makeBool(a == 0, u);
        default:
            return u;
    }
}

// Inline

static bool sameType(const Type& a, const Type& b) {
    if (a.kind != b.kind || a.ptrLevel != b.ptrLevel) return false;
    if (a.kind == Type::RECORD && a.recordName != b.recordName) return false;
    return a.arrayDims.empty() && b.arrayDims.empty();
}

static bool scalarOrPtr(const Type& t) {
    if (t.isRef || !t.arrayDims.empty()) return false;
    if (t.ptrLevel > 0) return true;
    return t.kind == Type::INT || t.kind == Type::BOOL || t.kind == Type::CHAR ||
           t.kind == Type::FLOAT || t.kind == Type::DOUBLE;
}

// Devuelve el motivo del rechazo, o vacio si la funcion se puede inlinar
string InlineVisitor::rejectReason(FunDecl* f, Exp*& body) {
    if (f->name == "main") return "es main";
    if (!f->body || f->body->items.size() != 1 || f->body->items[0]->nodeKind() != NodeKind::RETURN_STMT) {
        return "el cuerpo no es un unico return";
    }
    ReturnStmt* r = static_cast<ReturnStmt*>(f->body->items[0]);
    if (!r->expr) return "return sin valor";
    const Type& rt = f->layout.returnType;
    if (!scalarOrPtr(rt)) return "el retorno no es escalar";
    for (const VarInfo& p : f->layout.params) {
        if (!scalarOrPtr(p.type)) return "un parametro no es escalar ni puntero";
    }
    if (!sameType(r->expr->inferredType.withoutRef(), rt)) return "el return requiere una conversion implicita";

    vector<Exp*> work;
    work.push_back(r->expr);
    while (!work.empty()) {
        Exp* e = work.back();
        work.pop_back();
        switch (e->nodeKind()) {
            case NodeKind::NUMBER_EXP:
            case NodeKind::REAL_EXP:
            case NodeKind::BOOL_EXP:
            case NodeKind::CHAR_EXP:
            case NodeKind::NULLPTR_EXP:
                break;
            case NodeKind::ID_EXP: {
                IdExp* id = static_cast<IdExp*>(e);
                bool isParam = false;
                for (Param* p : f->params) {
                    if (id->parts.size() == 1 && id->parts[0] == p->name) isParam = true;
                }
                if (!isParam || id->idKind != ID_VAR || id->var.storage != Storage::LOCAL) {
                    return "usa variables que no son parametros";
                }
                break;
            }
            case NodeKind::UNARY_EXP: {
                UnaryExp* u = static_cast<UnaryExp*>(e);
                if (u->op == DEREF_UNARY || u->op == ADDR_UNARY || u->op == PRE_INC_UNARY || u->op == PRE_DEC_UNARY) {
                    return "usa desreferencia, direccion o incremento";
                }
                break;
            }
            case NodeKind::BINARY_EXP: {
                BinaryExp* b = static_cast<BinaryExp*>(e);
                if (b->mode != BM_NORMAL) return "usa aritmetica de punteros o streams";
                if (b->op == AND_OP || b->op == OR_OP) return "usa && o || (cortocircuito)";
                break;
            }
            default:
                return "contiene llamadas, accesos, asignaciones o cadenas";
        }
        vector<Exp**> kids;
        opt::childSlots(e, kids);
        for (Exp** k : kids) work.push_back(*k);
    }
    body = r->expr;
    return "";
}

void InlineVisitor::analyze(Program* prog) {
    candidates.clear();
    for (ASTNode* d : prog->declarations) {
        if (d->nodeKind() != NodeKind::FUN_DECL) continue;
        FunDecl* f = static_cast<FunDecl*>(d);
        Exp* body = nullptr;
        string why = rejectReason(f, body);
        if (why.empty()) {
            Candidate c;
            c.fun = f;
            c.body = body;
            candidates[f->name] = c;
        } else if (f->name != "main") {
            result.details.push_back("L" + to_string(f->line) + " no inlinable: " + f->name + " (" + why + ")");
        }
    }
}

static void substitute(Exp*& slot, FunDecl* f, const vector<Exp*>& args) {
    if (slot->nodeKind() == NodeKind::ID_EXP) {
        IdExp* id = static_cast<IdExp*>(slot);
        for (size_t i = 0; i < f->params.size(); i++) {
            if (id->parts.size() == 1 && id->parts[0] == f->params[i]->name) {
                Exp* rep = cloneExp(args[i]);
                delete slot;
                slot = rep;
                return;
            }
        }
        return;
    }
    vector<Exp**> kids;
    opt::childSlots(slot, kids);
    for (Exp** k : kids) substitute(*k, f, args);
}

Exp* InlineVisitor::post(Exp* e) {
    if (e->nodeKind() != NodeKind::POSTFIX_EXP) return e;
    PostfixExp* call = static_cast<PostfixExp*>(e);
    if (call->kind != CALL_ACCESS || call->callKind != CALL_FUNCTION || !call->callee) return e;
    auto it = candidates.find(call->callee->name);
    if (it == candidates.end() || it->second.fun != call->callee) return e;
    FunDecl* f = it->second.fun;
    string where = "L" + to_string(call->line) + ":C" + to_string(call->col) + " " + f->name;

    if (call->args.size() != f->params.size()) return e;
    for (size_t i = 0; i < call->args.size(); i++) {
        Exp* a = call->args[i];
        if (!opt::isPure(a)) {
            result.details.push_back(where + " no se inlina: argumento " + to_string(i + 1) + " con efectos");
            return e;
        }
        if (!sameType(a->inferredType.withoutRef(), f->layout.params[i].type.withoutRef())) {
            result.details.push_back(where + " no se inlina: argumento " + to_string(i + 1) + " requiere conversion");
            return e;
        }
    }

    Exp* clone = cloneExp(it->second.body);
    substitute(clone, f, call->args);
    result.count++;
    result.details.push_back(where + " inlinada");
    delete call;
    return clone;
}

// Sethi-Ullman

int SethiVisitor::label(const Exp* e) const {
    if (e->nodeKind() == NodeKind::UNARY_EXP) return label(static_cast<const UnaryExp*>(e)->exp);
    if (e->nodeKind() == NodeKind::BINARY_EXP) {
        const BinaryExp* b = static_cast<const BinaryExp*>(e);
        int l = label(b->left), r = label(b->right);
        return l == r ? l + 1 : (l > r ? l : r);
    }
    return 1;
}

Exp* SethiVisitor::post(Exp* e) {
    if (e->nodeKind() != NodeKind::BINARY_EXP) return e;
    BinaryExp* b = static_cast<BinaryExp*>(e);
    if (b->mode != BM_NORMAL) return e;
    switch (b->op) {
        case PLUS_OP: case MUL_OP: case BIT_AND_OP: case BIT_OR_OP: case BIT_XOR_OP: case EQ_OP: case NE_OP:
            break;
        default:
            return e;
    }
    if (!opt::isPlainInt(b->left->inferredType) || !opt::isPlainInt(b->right->inferredType)) return e;
    if (!opt::isPure(b->left) || !opt::isPure(b->right)) return e;
    int l = label(b->left), r = label(b->right);
    if (r > l) {
        Exp* t = b->left;
        b->left = b->right;
        b->right = t;
        result.count++;
        result.details.push_back("L" + to_string(b->line) + ":C" + to_string(b->col) + " operandos intercambiados (etiquetas " +
                                 to_string(l) + " y " + to_string(r) + ")");
    }
    return e;
}

bool runOptimizations(Program* ast, TypeChecker& checker, const vector<string>& passes, OptReport& rep) {
    rep.enabled = true;
    auto wanted = [&](const string& n) {
        for (const string& p : passes) {
            if (p == n) return true;
        }
        return false;
    };

    auto recheck = [&](const string& after) {
        checker.check(ast);
        if (checker.hasErrors()) {
            rep.log.push_back("la verificacion posterior a '" + after + "' fallo: " +
                              (checker.errorList.empty() ? string("error desconocido") : checker.errorList[0].message));
            return false;
        }
        return true;
    };

    if (wanted("inline")) {
        InlineVisitor v;
        v.analyze(ast);
        v.run(ast);
        rep.passes.push_back("inline");
        rep.results.push_back(v.result);
        if (!recheck("inline")) return false;
    }
    if (wanted("fold")) {
        FoldVisitor v;
        v.run(ast);
        rep.passes.push_back("fold");
        rep.results.push_back(v.result);
        if (!recheck("fold")) return false;
    }
    if (wanted("sethi")) {
        SethiVisitor v;
        v.run(ast);
        rep.passes.push_back("sethi");
        rep.results.push_back(v.result);
        if (!recheck("sethi")) return false;
    }
    return true;
}
