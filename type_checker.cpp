#include <iostream>
#include "type_checker.h"

using namespace std;

static int alignUp(int v, int a) {
    return (v + a - 1) / a * a;
}

static Type makeType(Type::Kind k, int ptr = 0) {
    return Type(k, ptr, false);
}

TypeChecker::TypeChecker()
    : currentFunction(nullptr), currentRecord(nullptr), loopDepth(0),
      hasIostream(false), hasString(false), hasCmath(false), usingNamespaceStd(false),
      frameUsed(0), paramIndex(0) {}

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
    env.add_level(); // ambito global
    functions.clear();
    records.clear();
    errors.clear();
    warnings.clear();
    usingNames.clear();
    hasIostream = hasString = hasCmath = usingNamespaceStd = false;
    currentFunction = nullptr;
    currentRecord = nullptr;
    loopDepth = 0;
    frameUsed = 0;

    prog->accept(this);

    if (!functions.count("main")) {
        addWarning("El programa no contiene la función 'main' requerida para la ejecución en C++.");
    }
}

// Anotacion y utilidades de tipos

Type TypeChecker::annotate(Exp* e, const Type& t, bool lvalue) {
    e->inferredType = t;
    e->isLValue = lvalue;
    return t;
}

Type TypeChecker::decay(const Type& t) const {
    if (t.arrayDims.size() == 1) {
        Type p = t;
        p.arrayDims.clear();
        p.ptrLevel++;
        p.isRef = false;
        return p;
    }
    return t;
}

bool TypeChecker::isNullLiteral(const Exp* e) const {
    if (e->nodeKind() == NodeKind::NULLPTR_EXP) return true;
    if (e->nodeKind() == NodeKind::NUMBER_EXP) return static_cast<const NumberExp*>(e)->value == 0;
    return false;
}

bool TypeChecker::boolConvertible(const Type& t) const {
    Type d = decay(t.withoutRef());
    return d.isNumeric() || d.isPointer();
}

bool TypeChecker::printable(const Type& t) const {
    Type d = decay(t.withoutRef());
    if (d.isNumeric() || d.isString()) return true;
    if (d.kind == Type::ENDL) return true;
    return d.kind == Type::CHAR && d.ptrLevel == 1 && d.arrayDims.empty();
}

bool TypeChecker::stdNameVisible(const IdExp* id, const string& name) const {
    if (id->parts.size() == 2) return id->parts[0] == "std" && id->parts[1] == name;
    if (id->parts.size() == 1) return usingNamespaceStd || usingNames.count(name) > 0;
    return false;
}

bool TypeChecker::assignable(const Type& toRaw, Exp* from) {
    Type to = toRaw.withoutRef();
    Type fr = decay(from->inferredType.withoutRef());
    if (to.isError() || fr.isError()) return false;
    if (to.isArray() || fr.isArray()) return false;

    if (to.isNumeric()) {
        if (fr.isNumeric()) return true;
        return to.isBool() && fr.isPointer();
    }
    if (to.isPointer()) {
        if (isNullLiteral(from)) return true;
        if (!fr.isPointer()) return false;
        if (to.sameBase(fr)) return true;
        return to.kind == Type::VOID && to.ptrLevel == 1;
    }
    if (to.isString()) {
        return fr.isString() || (fr.kind == Type::CHAR && fr.ptrLevel == 1);
    }
    if (to.isRecord()) {
        return fr.isRecord() && to.recordName == fr.recordName && !hasStrings(to);
    }
    return false;
}

// Evaluacion de expresiones constantes enteras, solo para dimensiones de arreglos
bool TypeChecker::constEval(Exp* e, long long& out) {
    switch (e->nodeKind()) {
        case NodeKind::NUMBER_EXP:
            out = static_cast<NumberExp*>(e)->value;
            return true;
        case NodeKind::CHAR_EXP:
            out = static_cast<long long>(static_cast<CharExp*>(e)->value);
            return true;
        case NodeKind::BOOL_EXP:
            out = static_cast<BoolExp*>(e)->value ? 1 : 0;
            return true;
        case NodeKind::UNARY_EXP: {
            UnaryExp* u = static_cast<UnaryExp*>(e);
            long long v;
            if (!constEval(u->exp, v)) return false;
            switch (u->op) {
                case PLUS_UNARY:    out = v; return true;
                case MINUS_UNARY:   out = -v; return true;
                case NOT_UNARY:     out = !v; return true;
                case BIT_NOT_UNARY: out = ~v; return true;
                default: return false;
            }
        }
        case NodeKind::BINARY_EXP: {
            BinaryExp* b = static_cast<BinaryExp*>(e);
            long long l, r;
            if (!constEval(b->left, l) || !constEval(b->right, r)) return false;
            switch (b->op) {
                case PLUS_OP:  out = l + r; return true;
                case MINUS_OP: out = l - r; return true;
                case MUL_OP:   out = l * r; return true;
                case DIV_OP:   if (r == 0) return false; out = l / r; return true;
                case MOD_OP:   if (r == 0) return false; out = l % r; return true;
                case EQ_OP:    out = l == r; return true;
                case NE_OP:    out = l != r; return true;
                case LT_OP:    out = l < r; return true;
                case LE_OP:    out = l <= r; return true;
                case GT_OP:    out = l > r; return true;
                case GE_OP:    out = l >= r; return true;
                case AND_OP:   out = l && r; return true;
                case OR_OP:    out = l || r; return true;
                case BIT_AND_OP: out = l & r; return true;
                case BIT_OR_OP:  out = l | r; return true;
                case BIT_XOR_OP: out = l ^ r; return true;
                case LSHIFT_OP:  if (r < 0 || r > 62) return false; out = l << r; return true;
                case RSHIFT_OP:  if (r < 0 || r > 62) return false; out = l >> r; return true;
            }
            return false;
        }
        default:
            return false;
    }
}

// Tamanos, alineaciones y strings contenidos (modelo LP64)

int TypeChecker::sizeOf(const Type& t) {
    if (t.isRef) return 8;
    long long count = 1;
    for (int d : t.arrayDims) count *= d;
    int es = 0;
    if (t.ptrLevel > 0) {
        es = 8;
    } else {
        switch (t.kind) {
            case Type::BOOL:
            case Type::CHAR:   es = 1; break;
            case Type::INT:
            case Type::FLOAT:  es = 4; break;
            case Type::DOUBLE: es = 8; break;
            case Type::STRING: es = 16; break;
            case Type::RECORD: {
                auto it = records.find(t.recordName);
                es = (it == records.end()) ? 0 : it->second->layout.size;
                break;
            }
            default: es = 0;
        }
    }
    return static_cast<int>(es * count);
}

int TypeChecker::alignOf(const Type& t) {
    if (t.isRef || t.ptrLevel > 0) return 8;
    switch (t.kind) {
        case Type::BOOL:
        case Type::CHAR:   return 1;
        case Type::INT:
        case Type::FLOAT:  return 4;
        case Type::DOUBLE:
        case Type::STRING: return 8;
        case Type::RECORD: {
            auto it = records.find(t.recordName);
            return (it == records.end()) ? 1 : it->second->layout.align;
        }
        default: return 1;
    }
}

bool TypeChecker::hasStrings(const Type& t) {
    if (t.isRef || t.ptrLevel > 0) return false;
    if (t.kind == Type::STRING) return true;
    if (t.kind == Type::RECORD) {
        auto it = records.find(t.recordName);
        return it != records.end() && !it->second->layout.stringOffsets.empty();
    }
    return false;
}

void TypeChecker::collectStringOffsets(const Type& t, vector<int>& out) {
    if (t.isRef || t.ptrLevel > 0 || t.isArray()) return;
    if (t.kind == Type::STRING) {
        out.push_back(0);
    } else if (t.kind == Type::RECORD) {
        auto it = records.find(t.recordName);
        if (it != records.end()) {
            for (int o : it->second->layout.stringOffsets) out.push_back(o);
        }
    }
}

int TypeChecker::pointeeSize(const Type& ptr, int line, int col) {
    Type p = ptr;
    p.ptrLevel--;
    p.arrayDims.clear();
    if (p.kind == Type::VOID && p.ptrLevel == 0) {
        addError("Aritmética o desreferencia sobre 'void*' no permitida", line, col);
        return 1;
    }
    return sizeOf(p);
}

Type TypeChecker::resolveType(Type t, int line, int col) {
    if (t.kind == Type::RECORD) {
        if (!records.count(t.recordName)) {
            addError("Tipo no declarado: '" + t.recordName + "'", line, col);
            return Type::makeError();
        }
    } else if (t.kind == Type::STRING) {
        if (!hasString) {
            addError("El tipo 'string' requiere #include <string>", line, col);
            return Type::makeError();
        }
        bool qualified = (t.recordName == "std::string");
        if (!qualified && !usingNamespaceStd && !usingNames.count("string")) {
            addError("'string' requiere 'using namespace std;' o el nombre 'std::string'", line, col);
            return Type::makeError();
        }
        t.recordName = "";
    }
    return t;
}

Type TypeChecker::declaredType(const Type& base, int ptrs, bool ref, const vector<Exp*>& dims, int line, int col) {
    Type t = base;
    t.ptrLevel = ptrs;
    t.isRef = ref;
    t.arrayDims.clear();
    t = resolveType(t, line, col);
    if (t.isError()) return t;

    for (Exp* d : dims) {
        long long v;
        if (!constEval(d, v)) {
            addError("La dimensión del arreglo debe ser una expresión constante entera", line, col);
            return Type::makeError();
        }
        if (v <= 0 || v > 100000000) {
            addError("Dimensión de arreglo inválida: " + to_string(v), line, col);
            return Type::makeError();
        }
        t.arrayDims.push_back(static_cast<int>(v));
    }
    return t;
}

VarInfo TypeChecker::allocLocal(const Type& t) {
    VarInfo info;
    info.type = t;
    info.size = sizeOf(t);
    info.align = alignOf(t);
    info.storage = Storage::LOCAL;
    int sz = info.size > 0 ? info.size : 1;
    frameUsed = alignUp(frameUsed + sz, info.align);
    info.offset = -frameUsed;
    collectStringOffsets(t, info.stringOffsets);
    return info;
}

VarInfo TypeChecker::makeVarInfo(const Type& t, bool global, const string& name) {
    if (!global) return allocLocal(t);
    VarInfo info;
    info.type = t;
    info.size = sizeOf(t);
    info.align = alignOf(t);
    info.storage = Storage::GLOBAL;
    info.label = "g_" + name;
    collectStringOffsets(t, info.stringOffsets);
    return info;
}

// Declaraciones de records y funciones (primer pase, en orden fuente)

void TypeChecker::declareRecord(RecordDecl* rd) {
    if (records.count(rd->name)) {
        addError("Redefinición de clase/struct '" + rd->name + "'", rd->line, rd->col);
        return;
    }
    records[rd->name] = rd;
    RecordLayout& L = rd->layout;
    L.name = rd->name;
    L.complete = false;

    int offset = 0;
    int maxAlign = 1;
    for (RecordMember* m : rd->members) {
        if (!m->varDecl) continue;
        for (VarInit* vi : m->varDecl->inits) {
            Type t = declaredType(m->varDecl->baseType, vi->ptrCount, vi->isRef, vi->arrayDims, vi->line, vi->col);
            if (t.isError()) continue;
            if (L.findField(vi->name)) {
                addError("Campo duplicado: '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            if (t.kind == Type::VOID && t.ptrLevel == 0) {
                addError("Un campo no puede ser de tipo void: '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            if (t.isRef) {
                addError("Campos de tipo referencia no soportados (no hay constructores): '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            if (vi->initializer) {
                addError("Inicializadores de campo no soportados (no hay constructores): '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            if (t.kind == Type::RECORD && t.ptrLevel == 0) {
                if (!records[t.recordName]->layout.complete) {
                    addError("El tipo '" + t.recordName + "' es incompleto como campo por valor", vi->line, vi->col);
                    continue;
                }
            }
            if (t.isArray() && hasStrings(t)) {
                addError("Arreglos de string o de records con string no soportados: '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            FieldInfo f;
            f.name = vi->name;
            f.type = t;
            f.size = sizeOf(t);
            f.align = alignOf(t);
            f.isPrivate = (m->access == ACCESS_PRIVATE);
            offset = alignUp(offset, f.align);
            f.offset = offset;
            offset += f.size;
            if (f.align > maxAlign) maxAlign = f.align;

            vector<int> so;
            collectStringOffsets(t, so);
            for (int o : so) L.stringOffsets.push_back(f.offset + o);
            L.fields.push_back(f);
        }
    }
    L.align = maxAlign;
    L.size = alignUp(offset > 0 ? offset : 1, maxAlign);
    L.complete = true;

    for (RecordMember* m : rd->members) {
        if (!m->funDecl) continue;
        declareFunction(m->funDecl, rd);
    }
}

void TypeChecker::declareFunction(FunDecl* fd, RecordDecl* owner) {
    FunctionLayout& L = fd->layout;
    L.isMethod = (owner != nullptr);
    L.ownerRecord = owner ? owner->name : "";
    if (owner) {
        L.label = "m_" + owner->name + "_" + fd->name;
    } else {
        L.label = (fd->name == "main") ? "main" : "f_" + fd->name;
    }

    Type rt = resolveType(fd->returnType, fd->line, fd->col);
    if (!rt.isError()) {
        if (!rt.isRef && rt.ptrLevel == 0 && (rt.kind == Type::RECORD || rt.kind == Type::STRING)) {
            addError("Retorno de '" + rt.toString() + "' por valor no soportado en '" + fd->name +
                     "' (use puntero o referencia)", fd->line, fd->col);
        }
    }
    L.returnType = rt;

    if (owner) {
        VarInfo thisInfo;
        thisInfo.type = Type::makeRecord(owner->name, 1);
        L.params.push_back(thisInfo);
    }

    for (Param* p : fd->params) {
        Type t = p->type;
        t.ptrLevel = p->ptrCount;
        t.isRef = p->isRef;
        t.arrayDims.clear();
        t = resolveType(t, p->line, p->col);
        if (!t.isError()) {
            if (p->arrayDims.size() > 1) {
                addError("Parámetros de arreglo multidimensional no soportados: '" + p->name + "'", p->line, p->col);
                t = Type::makeError();
            } else if (p->arrayDims.size() == 1) {
                t.ptrLevel++; // un parametro de arreglo decae a puntero
            }
        }
        if (!t.isError()) {
            if (t.kind == Type::VOID && t.ptrLevel == 0 && !t.isRef) {
                addError("Parámetro de tipo void: '" + p->name + "'", p->line, p->col);
                t = Type::makeError();
            } else if (!t.isRef && t.ptrLevel == 0 && (t.kind == Type::RECORD || t.kind == Type::STRING)) {
                addError("Paso de '" + t.toString() + "' por valor no soportado en el parámetro '" + p->name +
                         "' (use puntero o referencia)", p->line, p->col);
                t = Type::makeError();
            }
        }
        VarInfo vi;
        vi.type = t;
        L.params.push_back(vi);
    }

    if (owner) {
        RecordLayout& RL = owner->layout;
        if (RL.methods.count(fd->name) || RL.findField(fd->name)) {
            addError("Redefinición del miembro '" + fd->name + "' en '" + owner->name + "'", fd->line, fd->col);
            return;
        }
        MethodInfo mi;
        mi.fun = fd;
        mi.isPrivate = false;
        for (RecordMember* m : owner->members) {
            if (m->funDecl == fd) mi.isPrivate = (m->access == ACCESS_PRIVATE);
        }
        RL.methods[fd->name] = mi;
    } else {
        if (functions.count(fd->name)) {
            addError("Redefinición de la función '" + fd->name + "'", fd->line, fd->col);
        } else {
            functions[fd->name] = fd;
        }
    }
}

// Expresiones

Type TypeChecker::visit(BinaryExp* exp) {
    Type lt = exp->left->accept(this);
    Type rt = exp->right->accept(this);
    if (lt.isError() || rt.isError()) return annotate(exp, Type::makeError());

    string opName = Exp::binopToString(exp->op);

    if (lt.isStream() || rt.isStream() || rt.kind == Type::ENDL || lt.kind == Type::ENDL) {
        if (exp->op == LSHIFT_OP && lt.kind == Type::OSTREAM) {
            if (!printable(rt)) {
                addError("No se puede imprimir un valor de tipo '" + rt.toString() + "' con cout", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->mode = BM_COUT;
            exp->operandType = decay(rt.withoutRef());
            return annotate(exp, makeType(Type::OSTREAM));
        }
        if (exp->op == RSHIFT_OP && lt.kind == Type::ISTREAM) {
            Type d = rt.withoutRef();
            bool readable = d.isInt() || d.isChar() || d.isFloat() || d.isDouble() || d.isString();
            if (!exp->right->isLValue || !readable) {
                addError("El destino de 'cin >>' debe ser una variable int, char, float, double o string", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->mode = BM_CIN;
            exp->operandType = d;
            return annotate(exp, makeType(Type::ISTREAM));
        }
        addError("Uso inválido de cout/cin/endl con el operador '" + opName + "'", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }

    Type l = decay(lt.withoutRef());
    Type r = decay(rt.withoutRef());

    auto arith = [&]() {
        if (l.isDouble() || r.isDouble()) return makeType(Type::DOUBLE);
        if (l.isFloat() || r.isFloat())   return makeType(Type::FLOAT);
        return makeType(Type::INT);
    };

    switch (exp->op) {
        case PLUS_OP:
        case MINUS_OP:
        case MUL_OP:
        case DIV_OP: {
            if (exp->op == PLUS_OP || exp->op == MINUS_OP) {
                if (l.isPointer() && r.isIntegral()) {
                    exp->mode = (exp->op == PLUS_OP) ? BM_PTR_PLUS_INT : BM_PTR_MINUS_INT;
                    exp->ptrScale = pointeeSize(l, exp->line, exp->col);
                    return annotate(exp, l);
                }
                if (exp->op == PLUS_OP && l.isIntegral() && r.isPointer()) {
                    exp->mode = BM_INT_PLUS_PTR;
                    exp->ptrScale = pointeeSize(r, exp->line, exp->col);
                    return annotate(exp, r);
                }
                if (exp->op == MINUS_OP && l.isPointer() && r.isPointer() && l.sameBase(r)) {
                    exp->mode = BM_PTR_DIFF;
                    exp->ptrScale = pointeeSize(l, exp->line, exp->col);
                    return annotate(exp, makeType(Type::INT));
                }
            }
            if (!l.isNumeric() || !r.isNumeric()) {
                addError("Los operandos de '" + opName + "' deben ser numéricos (se recibió " +
                         lt.toString() + " y " + rt.toString() + ")", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->operandType = arith();
            return annotate(exp, exp->operandType);
        }

        case MOD_OP: {
            if (!l.isIntegral() || !r.isIntegral()) {
                addError("El operador '%' requiere operandos enteros", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->operandType = makeType(Type::INT);
            return annotate(exp, exp->operandType);
        }

        case EQ_OP:
        case NE_OP:
        case LT_OP:
        case LE_OP:
        case GT_OP:
        case GE_OP: {
            bool eq = (exp->op == EQ_OP || exp->op == NE_OP);
            if (l.isNumeric() && r.isNumeric()) {
                exp->operandType = arith();
                return annotate(exp, makeType(Type::BOOL));
            }
            if (eq && l.isString() && r.isString()) {
                exp->mode = BM_STR_STR;
                return annotate(exp, makeType(Type::BOOL));
            }
            bool lc = l.kind == Type::CHAR && l.ptrLevel == 1;
            bool rc = r.kind == Type::CHAR && r.ptrLevel == 1;
            if (eq && l.isString() && rc) {
                exp->mode = BM_STR_CSTR;
                return annotate(exp, makeType(Type::BOOL));
            }
            if (eq && lc && r.isString()) {
                exp->mode = BM_CSTR_STR;
                return annotate(exp, makeType(Type::BOOL));
            }
            bool lnull = isNullLiteral(exp->left), rnull = isNullLiteral(exp->right);
            if (l.isPointer() && (r.isPointer() || rnull)) {
                bool ok = rnull || l.sameBase(r) || (l.kind == Type::VOID && l.ptrLevel == 1) ||
                          (r.kind == Type::VOID && r.ptrLevel == 1);
                if (ok) {
                    exp->operandType = l;
                    return annotate(exp, makeType(Type::BOOL));
                }
            }
            if (r.isPointer() && lnull) {
                exp->operandType = r;
                return annotate(exp, makeType(Type::BOOL));
            }
            addError("Comparación '" + opName + "' entre tipos incompatibles (" + lt.toString() +
                     " y " + rt.toString() + ")", exp->line, exp->col);
            return annotate(exp, Type::makeError());
        }

        case AND_OP:
        case OR_OP: {
            if (!boolConvertible(l) || !boolConvertible(r)) {
                addError("Los operandos de '" + opName + "' deben poder convertirse a bool", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->operandType = makeType(Type::BOOL);
            return annotate(exp, makeType(Type::BOOL));
        }

        case BIT_AND_OP:
        case BIT_OR_OP:
        case BIT_XOR_OP:
        case LSHIFT_OP:
        case RSHIFT_OP: {
            if (!l.isIntegral() || !r.isIntegral()) {
                addError("El operador '" + opName + "' requiere operandos enteros", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->operandType = makeType(Type::INT);
            return annotate(exp, exp->operandType);
        }
    }
    return annotate(exp, Type::makeError());
}

Type TypeChecker::visit(UnaryExp* exp) {
    Type t0 = exp->exp->accept(this);
    if (t0.isError()) return annotate(exp, Type::makeError());
    Type t = decay(t0.withoutRef());

    switch (exp->op) {
        case PLUS_UNARY:
        case MINUS_UNARY:
            if (!t.isNumeric()) {
                addError("Operando unario '+/-' requiere tipo numérico", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            if (t.isBool() || t.isChar()) t = makeType(Type::INT);
            return annotate(exp, t);

        case NOT_UNARY:
            if (!boolConvertible(t)) {
                addError("El operador '!' requiere un operando convertible a bool", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            return annotate(exp, makeType(Type::BOOL));

        case BIT_NOT_UNARY:
            if (!t.isIntegral()) {
                addError("El operador '~' requiere un operando entero", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            return annotate(exp, makeType(Type::INT));

        case DEREF_UNARY: {
            if (!t.isPointer()) {
                addError("No se puede desreferenciar un tipo no puntero: " + t0.toString(), exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            pointeeSize(t, exp->line, exp->col);
            Type res = t;
            res.ptrLevel--;
            return annotate(exp, res, true);
        }

        case ADDR_UNARY: {
            if (!exp->exp->isLValue || t0.isArray()) {
                addError("El operador '&' requiere una variable o elemento (lvalue)", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            Type res = t0.withoutRef();
            res.ptrLevel++;
            return annotate(exp, res);
        }

        case PRE_INC_UNARY:
        case PRE_DEC_UNARY:
            if (!exp->exp->isLValue || t0.isArray() || (!t.isNumeric() && !t.isPointer()) || t.isBool()) {
                addError("Operador '++ / --' requiere un lvalue numérico (no bool) o puntero", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            if (t.isPointer()) exp->ptrScale = pointeeSize(t, exp->line, exp->col);
            return annotate(exp, t0.withoutRef(), true);
    }
    return annotate(exp, Type::makeError());
}

Type TypeChecker::visit(AssignExp* exp) {
    Type lt = exp->target->accept(this);
    Type rt = exp->value->accept(this);
    if (lt.isError() || rt.isError()) return annotate(exp, Type::makeError());

    if (!exp->target->isLValue) {
        addError("El destino de la asignación debe ser una variable o elemento (lvalue)", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    if (lt.isArray()) {
        addError("No se puede asignar a un arreglo completo", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    if (!assignable(lt, exp->value)) {
        addError("No se puede asignar un valor de tipo '" + rt.toString() +
                 "' a una variable de tipo '" + lt.toString() + "'", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    return annotate(exp, lt.withoutRef());
}

Type TypeChecker::visit(NumberExp* exp) {
    return annotate(exp, Type(static_cast<int>(exp->value)));
}

Type TypeChecker::visit(RealExp* exp) {
    return annotate(exp, Type(exp->value));
}

Type TypeChecker::visit(BoolExp* exp) {
    return annotate(exp, Type(exp->value));
}

Type TypeChecker::visit(CharExp* exp) {
    return annotate(exp, Type(exp->value));
}

Type TypeChecker::visit(StringExp* exp) {
    return annotate(exp, makeType(Type::CHAR, 1));
}

Type TypeChecker::visit(NullptrExp* exp) {
    Type t = makeType(Type::VOID, 1);
    t.isConstExpr = true;
    return annotate(exp, t);
}

Type TypeChecker::visit(IdExp* exp) {
    VarInfo vi;
    if (exp->parts.size() == 1 && env.lookup(exp->parts[0], vi)) {
        exp->var = vi;
        exp->idKind = (vi.storage == Storage::FIELD) ? ID_FIELD : ID_VAR;
        exp->isRefVar = vi.type.isRef;
        return annotate(exp, vi.type.withoutRef(), true);
    }

    const string& last = exp->parts.back();
    bool stream = (last == "cout" || last == "cin" || last == "endl");
    if (stream && (exp->parts.size() == 1 || (exp->parts.size() == 2 && exp->parts[0] == "std"))) {
        if (!hasIostream) {
            addError("'" + exp->getFullName() + "' requiere #include <iostream>", exp->line, exp->col);
            return annotate(exp, Type::makeError());
        }
        if (!stdNameVisible(exp, last)) {
            addError("'" + last + "' no está visible: use 'std::" + last + "' o 'using namespace std;'",
                     exp->line, exp->col);
            return annotate(exp, Type::makeError());
        }
        if (last == "cout") { exp->idKind = ID_COUT; return annotate(exp, makeType(Type::OSTREAM)); }
        if (last == "cin")  { exp->idKind = ID_CIN;  return annotate(exp, makeType(Type::ISTREAM)); }
        exp->idKind = ID_ENDL;
        return annotate(exp, makeType(Type::ENDL));
    }

    if (functions.count(exp->getFullName())) {
        addError("La función '" + exp->getFullName() + "' no se puede usar como valor", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    addError("Identificador no declarado: '" + exp->getFullName() + "'", exp->line, exp->col);
    return annotate(exp, Type::makeError());
}

Type TypeChecker::checkMemberAccess(PostfixExp* exp) {
    Type bt = exp->base->accept(this);
    if (bt.isError()) return annotate(exp, Type::makeError());
    bool arrow = (exp->kind == ARROW_ACCESS);

    Type rec = bt;
    if (arrow) {
        if (bt.ptrLevel != 1 || bt.isArray() || bt.kind != Type::RECORD) {
            addError("Acceso '->' requiere un puntero a struct/class (se recibió " + bt.toString() + ")",
                     exp->line, exp->col);
            return annotate(exp, Type::makeError());
        }
        rec.ptrLevel = 0;
    } else if (!bt.isRecord()) {
        addError("Acceso a miembro '.' sobre tipo no estructurado: " + bt.toString(), exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }

    RecordDecl* rd = records[rec.recordName];
    const FieldInfo* f = rd->layout.findField(exp->memberName);
    if (!f) {
        if (rd->layout.methods.count(exp->memberName)) {
            addError("El método '" + exp->memberName + "' debe invocarse con paréntesis", exp->line, exp->col);
        } else {
            addError("El miembro '" + exp->memberName + "' no existe en '" + rec.recordName + "'",
                     exp->line, exp->col);
        }
        return annotate(exp, Type::makeError());
    }
    if (f->isPrivate && (!currentRecord || currentRecord->name != rd->name)) {
        addError("El miembro '" + exp->memberName + "' es privado en '" + rd->name + "'", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    exp->fieldOffset = f->offset;
    return annotate(exp, f->type, true);
}

Type TypeChecker::callResult(PostfixExp* call, FunDecl* fd) {
    Type rt = fd->layout.returnType;
    if (rt.isError()) return annotate(call, rt);
    return annotate(call, rt.withoutRef(), rt.isRef);
}

void TypeChecker::checkArgs(PostfixExp* call, FunDecl* fd, const string& name) {
    size_t off = fd->layout.isMethod ? 1 : 0;
    size_t np = fd->layout.params.size() - off;

    for (Exp* a : call->args) a->accept(this);

    if (call->args.size() != np) {
        addError("La función '" + name + "' espera " + to_string(np) +
                 " argumentos, pero recibió " + to_string(call->args.size()), call->line, call->col);
        return;
    }
    for (size_t i = 0; i < np; i++) {
        Exp* a = call->args[i];
        Type pt = fd->layout.params[i + off].type;
        Type at = a->inferredType;
        if (pt.isError() || at.isError()) continue;
        if (pt.isRef) {
            if (!a->isLValue || !at.withoutRef().sameBase(pt) || at.isArray()) {
                addError("Argumento " + to_string(i + 1) + " de '" + name + "' debe ser una variable de tipo '" +
                         pt.withoutRef().toString() + "' (parámetro por referencia)", call->line, call->col);
            }
        } else if (!assignable(pt, a)) {
            addError("Argumento " + to_string(i + 1) + " de '" + name + "' tiene tipo '" +
                     at.toString() + "', pero se esperaba '" + pt.toString() + "'", call->line, call->col);
        }
    }
}

Type TypeChecker::checkCall(PostfixExp* call) {
    Exp* base = call->base;

    if (base->nodeKind() == NodeKind::ID_EXP) {
        IdExp* id = static_cast<IdExp*>(base);
        string name = id->getFullName();

        if (currentRecord && id->parts.size() == 1) {
            auto it = currentRecord->layout.methods.find(name);
            if (it != currentRecord->layout.methods.end()) {
                call->callKind = CALL_METHOD_IMPLICIT;
                call->callee = it->second.fun;
                checkArgs(call, call->callee, name);
                return callResult(call, call->callee);
            }
        }

        auto fit = functions.find(name);
        if (fit != functions.end()) {
            call->callKind = CALL_FUNCTION;
            call->callee = fit->second;
            checkArgs(call, call->callee, name);
            return callResult(call, call->callee);
        }

        if (name == "sqrt" || name == "std::sqrt") {
            for (Exp* a : call->args) a->accept(this);
            if (!hasCmath) {
                addError("'sqrt' requiere #include <cmath>", call->line, call->col);
                return annotate(call, Type::makeError());
            }
            if (call->args.size() != 1 || !decay(call->args[0]->inferredType.withoutRef()).isNumeric()) {
                addError("'sqrt' espera un argumento numérico", call->line, call->col);
                return annotate(call, Type::makeError());
            }
            call->callKind = CALL_BUILTIN;
            call->builtin = BI_SQRT;
            return annotate(call, makeType(Type::DOUBLE));
        }

        for (Exp* a : call->args) a->accept(this);
        addError("Llamada a función no declarada: '" + name + "'", call->line, call->col);
        return annotate(call, Type::makeError());
    }

    if (base->nodeKind() == NodeKind::POSTFIX_EXP) {
        PostfixExp* pf = static_cast<PostfixExp*>(base);
        if (pf->kind == DOT_ACCESS || pf->kind == ARROW_ACCESS) {
            bool arrow = (pf->kind == ARROW_ACCESS);
            Type ot = pf->base->accept(this);
            if (ot.isError()) {
                for (Exp* a : call->args) a->accept(this);
                return annotate(call, Type::makeError());
            }
            Type rec = ot;
            if (arrow) {
                if (ot.ptrLevel != 1 || ot.isArray()) {
                    addError("Acceso '->' requiere un puntero (se recibió " + ot.toString() + ")", call->line, call->col);
                    for (Exp* a : call->args) a->accept(this);
                    return annotate(call, Type::makeError());
                }
                rec.ptrLevel = 0;
            } else if (ot.ptrLevel != 0 || ot.isArray()) {
                addError("Acceso '.' sobre un puntero o arreglo; use '->' (se recibió " + ot.toString() + ")",
                         call->line, call->col);
                for (Exp* a : call->args) a->accept(this);
                return annotate(call, Type::makeError());
            }

            const string& m = pf->memberName;

            if (rec.kind == Type::STRING) {
                for (Exp* a : call->args) a->accept(this);
                call->callKind = CALL_BUILTIN;
                size_t n = call->args.size();
                if ((m == "size" || m == "length") && n == 0) {
                    call->builtin = BI_STR_SIZE;
                    return annotate(call, makeType(Type::INT));
                }
                if (m == "empty" && n == 0) {
                    call->builtin = BI_STR_EMPTY;
                    return annotate(call, makeType(Type::BOOL));
                }
                if (m == "clear" && n == 0) {
                    call->builtin = BI_STR_CLEAR;
                    return annotate(call, makeType(Type::VOID));
                }
                if (m == "c_str" && n == 0) {
                    call->builtin = BI_STR_C_STR;
                    return annotate(call, makeType(Type::CHAR, 1));
                }
                if (m == "append" && n == 1) {
                    Type at = decay(call->args[0]->inferredType.withoutRef());
                    if (at.isString()) call->builtin = BI_STR_APPEND_STR;
                    else if (at.kind == Type::CHAR && at.ptrLevel == 1) call->builtin = BI_STR_APPEND_CSTR;
                    else {
                        addError("append espera un string o un char*", call->line, call->col);
                        return annotate(call, Type::makeError());
                    }
                    return annotate(call, makeType(Type::VOID));
                }
                if (m == "push_back" && n == 1) {
                    if (!decay(call->args[0]->inferredType.withoutRef()).isChar()) {
                        addError("push_back espera un char", call->line, call->col);
                        return annotate(call, Type::makeError());
                    }
                    call->builtin = BI_STR_APPEND_CHAR;
                    return annotate(call, makeType(Type::VOID));
                }
                addError("Método de string no soportado: '" + m + "' con " + to_string(n) + " argumentos",
                         call->line, call->col);
                return annotate(call, Type::makeError());
            }

            if (rec.kind == Type::RECORD && rec.ptrLevel == 0) {
                RecordDecl* rd = records[rec.recordName];
                auto it = rd->layout.methods.find(m);
                if (it == rd->layout.methods.end()) {
                    for (Exp* a : call->args) a->accept(this);
                    addError("El método '" + m + "' no existe en '" + rec.recordName + "'", call->line, call->col);
                    return annotate(call, Type::makeError());
                }
                if (it->second.isPrivate && (!currentRecord || currentRecord->name != rd->name)) {
                    for (Exp* a : call->args) a->accept(this);
                    addError("El método '" + m + "' es privado en '" + rd->name + "'", call->line, call->col);
                    return annotate(call, Type::makeError());
                }
                call->callKind = CALL_METHOD;
                call->callee = it->second.fun;
                checkArgs(call, call->callee, m);
                return callResult(call, call->callee);
            }

            for (Exp* a : call->args) a->accept(this);
            addError("Llamada a método sobre un tipo sin métodos: " + ot.toString(), call->line, call->col);
            return annotate(call, Type::makeError());
        }
    }

    for (Exp* a : call->args) a->accept(this);
    addError("Llamada a función sobre una expresión que no es un identificador ni un método", call->line, call->col);
    return annotate(call, Type::makeError());
}

Type TypeChecker::visit(PostfixExp* exp) {
    switch (exp->kind) {
        case POST_INC:
        case POST_DEC: {
            Type bt0 = exp->base->accept(this);
            if (bt0.isError()) return annotate(exp, Type::makeError());
            Type bt = decay(bt0.withoutRef());
            if (!exp->base->isLValue || bt0.isArray() || (!bt.isNumeric() && !bt.isPointer()) || bt.isBool()) {
                addError("Operador '++ / --' requiere un lvalue numérico (no bool) o puntero", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            if (bt.isPointer()) exp->ptrScale = pointeeSize(bt, exp->line, exp->col);
            return annotate(exp, bt0.withoutRef());
        }

        case ARRAY_ACCESS: {
            Type bt = exp->base->accept(this);
            Type it = exp->indexExp ? exp->indexExp->accept(this) : Type::makeError();
            if (bt.isError() || it.isError()) return annotate(exp, Type::makeError());
            if (!it.withoutRef().isIntegral()) {
                addError("El índice del arreglo debe ser un entero", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            Type res;
            if (bt.isArray()) {
                res = bt.elementType();
            } else if (bt.isPointer()) {
                pointeeSize(bt, exp->line, exp->col);
                res = bt.withoutRef();
                res.ptrLevel--;
            } else if (bt.isString()) {
                res = makeType(Type::CHAR);
            } else {
                addError("Subíndice '[]' aplicado a un tipo que no es arreglo, puntero ni string: " + bt.toString(),
                         exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->elemSize = sizeOf(res);
            return annotate(exp, res, true);
        }

        case DOT_ACCESS:
        case ARROW_ACCESS:
            return checkMemberAccess(exp);

        case CALL_ACCESS:
            return checkCall(exp);
    }
    return annotate(exp, Type::makeError());
}

Type TypeChecker::visit(NewExp* exp) {
    Type t = resolveType(exp->allocType, exp->line, exp->col);
    if (t.isError()) {
        for (Exp* d : exp->arrayDims) d->accept(this);
        return annotate(exp, t);
    }
    exp->allocType = t;
    if (exp->arrayDims.size() > 1) {
        addError("'new' con más de una dimensión no soportado", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    for (Exp* d : exp->arrayDims) {
        Type dt = d->accept(this);
        if (!dt.isError() && !dt.withoutRef().isIntegral()) {
            addError("La dimensión en 'new' debe ser entera", exp->line, exp->col);
        }
    }
    if (t.ptrLevel == 0 && t.kind == Type::VOID) {
        addError("No se puede reservar memoria de tipo void", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    if (hasStrings(t)) {
        addError("'new' de string o de records con string no soportado (no hay constructores)", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    exp->elemSize = sizeOf(t);
    Type res = t;
    res.ptrLevel++;
    return annotate(exp, res);
}

Type TypeChecker::visit(DeleteExp* exp) {
    Type t = exp->exp->accept(this);
    if (!t.isError() && !t.isPointer()) {
        addError("Operador 'delete' requiere un puntero", exp->line, exp->col);
    }
    return annotate(exp, makeType(Type::VOID));
}

Type TypeChecker::visit(BracedInitList* exp) {
    addError("Una lista de inicialización con llaves solo es válida al declarar una variable", exp->line, exp->col);
    for (Exp* e : exp->elements) e->accept(this);
    return annotate(exp, Type::makeError());
}

// Sentencias

void TypeChecker::checkCondition(Exp* cond, int line, int col) {
    Type t = cond->accept(this);
    if (t.isError()) return;
    if (!boolConvertible(t)) {
        addError("La condición debe ser booleana (bool, numérica o puntero), se recibió '" + t.toString() + "'",
                 line, col);
    }
}

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
    checkCondition(stm->condition, stm->line, stm->col);
    stm->thenStmt->accept(this);
    if (stm->elseStmt) {
        stm->elseStmt->accept(this);
    }
}

void TypeChecker::visit(WhileStmt* stm) {
    checkCondition(stm->condition, stm->line, stm->col);
    loopDepth++;
    stm->body->accept(this);
    loopDepth--;
}

void TypeChecker::visit(ForStmt* stm) {
    env.add_level();
    if (stm->initDecl) stm->initDecl->accept(this);
    for (Exp* e : stm->initExprs) e->accept(this);
    if (stm->condition) checkCondition(stm->condition, stm->line, stm->col);
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
    checkCondition(stm->condition, stm->line, stm->col);
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

    Type retType = currentFunction->layout.returnType;
    if (retType.isError()) {
        if (stm->expr) stm->expr->accept(this);
        return;
    }
    if (retType.isVoid()) {
        if (stm->expr) {
            stm->expr->accept(this);
            addError("Función void '" + currentFunction->name + "' no debe devolver un valor", stm->line, stm->col);
        }
        return;
    }
    if (!stm->expr) {
        addError("Función no void '" + currentFunction->name + "' debe devolver una expresión de tipo '" +
                 retType.toString() + "'", stm->line, stm->col);
        return;
    }
    Type et = stm->expr->accept(this);
    if (et.isError()) return;
    if (retType.isRef) {
        if (!stm->expr->isLValue || !et.withoutRef().sameBase(retType)) {
            addError("Una función que retorna '" + retType.toString() + "' debe devolver una variable de ese tipo",
                     stm->line, stm->col);
        }
    } else if (!assignable(retType, stm->expr)) {
        addError("Tipo de retorno incompatible en '" + currentFunction->name +
                 "': se esperaba '" + retType.toString() + "', pero se devolvió '" + et.toString() + "'",
                 stm->line, stm->col);
    }
}

// Declaraciones y directivas

void TypeChecker::visit(VarInit* /*init*/) {
    // Manejado en VarDecl
}

void TypeChecker::checkInitializer(const Type& target, Exp* init, int line, int col) {
    if (init->nodeKind() == NodeKind::BRACED_INIT) {
        BracedInitList* bl = static_cast<BracedInitList*>(init);
        annotate(bl, target);
        bl->elementOffsets.clear();
        bl->elementTypes.clear();

        if (target.isArray()) {
            Type et = target.elementType();
            int esz = sizeOf(et);
            if (static_cast<int>(bl->elements.size()) > target.arrayDims[0]) {
                addError("Demasiados inicializadores para el arreglo (máximo " +
                         to_string(target.arrayDims[0]) + ")", line, col);
            }
            for (size_t i = 0; i < bl->elements.size(); i++) {
                bl->elementOffsets.push_back(static_cast<int>(i) * esz);
                bl->elementTypes.push_back(et);
                checkInitializer(et, bl->elements[i], line, col);
            }
        } else if (target.isRecord()) {
            RecordLayout& L = records[target.recordName]->layout;
            if (hasStrings(target)) {
                addError("Inicialización con llaves no soportada para records con string", line, col);
                return;
            }
            if (bl->elements.size() > L.fields.size()) {
                addError("Demasiados inicializadores para '" + target.recordName + "'", line, col);
            }
            for (size_t i = 0; i < bl->elements.size() && i < L.fields.size(); i++) {
                bl->elementOffsets.push_back(L.fields[i].offset);
                bl->elementTypes.push_back(L.fields[i].type);
                checkInitializer(L.fields[i].type, bl->elements[i], line, col);
            }
        } else if (!target.isRef && (target.isNumeric() || target.isPointer())) {
            if (bl->elements.size() > 1) {
                addError("Demasiados inicializadores para una variable escalar", line, col);
            } else if (bl->elements.size() == 1) {
                bl->elementOffsets.push_back(0);
                bl->elementTypes.push_back(target);
                checkInitializer(target, bl->elements[0], line, col);
            }
        } else {
            addError("No se puede inicializar un valor de tipo '" + target.toString() + "' con llaves", line, col);
        }
        return;
    }

    Type it = init->accept(this);
    if (it.isError()) return;
    if (target.isRef) {
        if (!init->isLValue || !it.withoutRef().sameBase(target) || it.isArray()) {
            addError("Una referencia '" + target.toString() + "' debe inicializarse con una variable del mismo tipo",
                     line, col);
        }
    } else if (target.isArray()) {
        addError("Un arreglo solo puede inicializarse con una lista entre llaves", line, col);
    } else if (!assignable(target, init)) {
        addError("Inicialización incompatible: tipo '" + target.toString() + "' inicializado con '" +
                 it.toString() + "'", line, col);
    }
}

void TypeChecker::visit(VarDecl* decl) {
    bool global = (currentFunction == nullptr);

    for (VarInit* vi : decl->inits) {
        bool redecl = env.check_current_level(vi->name);
        if (redecl) {
            addError("Redeclaración de variable en el mismo ámbito: '" + vi->name + "'", vi->line, vi->col);
        }

        Type t = declaredType(decl->baseType, vi->ptrCount, vi->isRef, vi->arrayDims, vi->line, vi->col);

        if (!t.isError()) {
            if (t.kind == Type::VOID && t.ptrLevel == 0) {
                addError("Variable de tipo void: '" + vi->name + "'", vi->line, vi->col);
                t = Type::makeError();
            } else if (t.isStream() || t.kind == Type::ENDL) {
                t = Type::makeError();
            } else if (t.isRef && t.isArray()) {
                addError("Arreglo de referencias no permitido: '" + vi->name + "'", vi->line, vi->col);
                t = Type::makeError();
            } else if (t.isArray() && hasStrings(t)) {
                addError("Arreglos de string o de records con string no soportados: '" + vi->name + "'",
                         vi->line, vi->col);
                t = Type::makeError();
            }
        }

        if (vi->initializer) {
            if (t.isError()) {
                if (vi->initializer->nodeKind() != NodeKind::BRACED_INIT) vi->initializer->accept(this);
            } else {
                checkInitializer(t, vi->initializer, vi->line, vi->col);
            }
        }

        VarInfo info;
        if (t.isError()) {
            info.type = t;
        } else {
            info = makeVarInfo(t, global, vi->name);
            if (!global) {
                for (int so : info.stringOffsets) curStringSlots.push_back(info.offset + so);
            }
        }
        vi->info = info;
        if (!redecl) env.add_var(vi->name, info);
    }
}

void TypeChecker::visit(Param* param) {
    VarInfo& slot = currentFunction->layout.params[paramIndex];
    Type t = slot.type;
    paramIndex++;

    if (env.check_current_level(param->name)) {
        addError("Nombre de parámetro duplicado: '" + param->name + "'", param->line, param->col);
    }
    VarInfo info;
    if (t.isError()) {
        info.type = t;
    } else {
        info = allocLocal(t);
    }
    slot = info;
    param->info = info;
    env.add_var(param->name, info);
}

void TypeChecker::visit(FunDecl* fun) {
    currentFunction = fun;
    frameUsed = 0;
    curStringSlots.clear();
    loopDepth = 0;
    env.add_level(); // ambito de la funcion

    paramIndex = 0;
    if (fun->layout.isMethod) {
        VarInfo ti = allocLocal(fun->layout.params[0].type);
        fun->layout.params[0] = ti;
        env.add_var("this", ti);
        paramIndex = 1;
    }
    for (Param* p : fun->params) {
        p->accept(this);
    }

    if (fun->body) {
        // los items se visitan en el mismo nivel de los parametros
        for (ASTNode* item : fun->body->items) {
            item->accept(this);
        }
    }

    env.remove_level();

    fun->layout.stringSlots = curStringSlots;
    if (!curStringSlots.empty()) {
        frameUsed = alignUp(frameUsed, 8) + 16;
        fun->layout.retSaveOffset = -frameUsed;
    }
    fun->layout.frameSize = alignUp(frameUsed, 16);
    currentFunction = nullptr;
}

void TypeChecker::visit(RecordMember* member) {
    if (member->funDecl) member->funDecl->accept(this);
}

void TypeChecker::visit(RecordDecl* record) {
    auto it = records.find(record->name);
    if (it == records.end() || it->second != record) return; // redefinicion ya reportada

    currentRecord = record;
    env.add_level();
    for (const FieldInfo& f : record->layout.fields) {
        VarInfo vi;
        vi.type = f.type;
        vi.offset = f.offset;
        vi.size = f.size;
        vi.align = f.align;
        vi.storage = Storage::FIELD;
        env.add_var(f.name, vi);
    }
    for (RecordMember* m : record->members) {
        m->accept(this);
    }
    env.remove_level();
    currentRecord = nullptr;
}

void TypeChecker::visit(IncludeDirective* inc) {
    if (inc->library == "iostream") hasIostream = true;
    else if (inc->library == "string") hasString = true;
    else if (inc->library == "cmath") hasCmath = true;
    else addError("Librería no soportada: <" + inc->library + ">", inc->line, inc->col);
}

void TypeChecker::visit(UsingDirective* usg) {
    if (usg->isNamespace) {
        if (usg->getFullName() == "std") usingNamespaceStd = true;
        else addWarning("Namespace no soportado en using: '" + usg->getFullName() + "'", usg->line, usg->col);
    } else if (usg->qualifiedId.size() == 2 && usg->qualifiedId[0] == "std") {
        usingNames.insert(usg->qualifiedId[1]);
    } else {
        addWarning("Directiva using no soportada: '" + usg->getFullName() + "'", usg->line, usg->col);
    }
}

void TypeChecker::visit(Program* prog) {
    for (IncludeDirective* inc : prog->includes) {
        inc->accept(this);
    }
    for (UsingDirective* usg : prog->usings) {
        usg->accept(this);
    }

    // Primer pase en orden fuente: layout de records y firmas de funciones
    for (ASTNode* decl : prog->declarations) {
        if (decl->nodeKind() == NodeKind::RECORD_DECL) {
            declareRecord(static_cast<RecordDecl*>(decl));
        } else if (decl->nodeKind() == NodeKind::FUN_DECL) {
            declareFunction(static_cast<FunDecl*>(decl), nullptr);
        }
    }

    // Segundo pase en orden fuente: globales, cuerpos de funciones y metodos
    for (ASTNode* decl : prog->declarations) {
        decl->accept(this);
    }
}
