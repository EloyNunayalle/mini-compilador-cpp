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
      frameUsed(0), initFrameUsed(0), paramIndex(0), program(nullptr) {}

void TypeChecker::addError(const string& msg, int line, int col) {
    string loc = (line > 0) ? "[Línea " + to_string(line) + ", Columna " + to_string(col) + "] " : "";
    errors.push_back("Error Semántico " + loc + msg);
    errorList.push_back({"typecheck", msg, line, col});
}

void TypeChecker::addWarning(const string& msg, int line, int col) {
    string loc = (line > 0) ? "[Línea " + to_string(line) + ", Columna " + to_string(col) + "] " : "";
    warnings.push_back("Advertencia " + loc + msg);
    warningList.push_back({"typecheck", msg, line, col});
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
    errorList.clear();
    warningList.clear();
    usingNames.clear();
    hasIostream = hasString = hasCmath = usingNamespaceStd = false;
    currentFunction = nullptr;
    currentRecord = nullptr;
    loopDepth = 0;
    frameUsed = 0;
    initFrameUsed = 0;
    program = prog;

    prog->accept(this);
    prog->initFrameSize = alignUp(initFrameUsed, 16);

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

// Conversion de calificadores entre punteros del mismo tipo base y nivel: no se puede
// quitar const y agregarlo en un nivel interno exige const en todos los niveles intermedios
bool TypeChecker::qualConvertible(const Type& to, const Type& from) const {
    int n = to.ptrLevel;
    for (int k = 0; k < n; k++) {
        bool tc = to.constAt(k);
        bool fc = from.constAt(k);
        if (fc && !tc) return false;
        if (tc && !fc) {
            for (int j = k + 1; j < n; j++) {
                if (!to.constAt(j)) return false;
            }
        }
    }
    return true;
}

// Enlace directo de una referencia a un lvalue: los niveles internos deben coincidir
bool TypeChecker::refBindable(const Type& target, const Type& from) const {
    if (target.ptrLevel != from.ptrLevel) return false;
    for (int k = 0; k < target.ptrLevel; k++) {
        if (target.constAt(k) != from.constAt(k)) return false;
    }
    if (from.isConstObj() && !target.isConstObj()) return false;
    return true;
}

bool TypeChecker::isRecordRvalueOf(Exp* e, const Type& rec) const {
    Type t = e->inferredType.withoutRef();
    return !e->isLValue && t.isRecord() && t.recordName == rec.recordName;
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
        // un literal de cadena se acepta como char* (GCC lo admite con advertencia)
        if (from->nodeKind() == NodeKind::STRING_EXP && to.kind == Type::CHAR && to.ptrLevel == 1) return true;
        if (to.sameBase(fr)) return qualConvertible(to, fr);
        if (to.kind == Type::VOID && to.ptrLevel == 1) {
            // cualquier puntero a void*, sin perder const del objeto apuntado
            return !(fr.constAt(fr.ptrLevel - 1) && !to.constAt(0));
        }
        return false;
    }
    if (to.isString()) {
        return fr.isString() || (fr.kind == Type::CHAR && fr.ptrLevel == 1);
    }
    if (to.isRecord()) {
        if (!fr.isRecord() || to.recordName != fr.recordName) return false;
        RecordLayout* L = layoutOf(to);
        return L && L->copyCtorAvail != Avail::DELETED;
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
        case NodeKind::ID_EXP: {
            IdExp* id = static_cast<IdExp*>(e);
            if (id->parts.size() != 1) return false;
            VarInfo vi;
            if (env.lookup(id->parts[0], vi)) {
                if (!vi.hasConstVal) return false;
                out = vi.constVal;
                return true;
            }
            auto it = globalConsts.find(id->parts[0]);
            if (it == globalConsts.end()) return false;
            out = it->second;
            return true;
        }
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

// Los inicializadores de campo en la clase se limitan a expresiones constantes: asi se
// pueden evaluar en cualquier constructor sin depender de un marco de funcion
bool TypeChecker::isConstantInit(Exp* e) const {
    switch (e->nodeKind()) {
        case NodeKind::NUMBER_EXP:
        case NodeKind::REAL_EXP:
        case NodeKind::BOOL_EXP:
        case NodeKind::CHAR_EXP:
        case NodeKind::STRING_EXP:
        case NodeKind::NULLPTR_EXP:
            return true;
        case NodeKind::UNARY_EXP: {
            UnaryExp* u = static_cast<UnaryExp*>(e);
            if (u->op == DEREF_UNARY || u->op == ADDR_UNARY || u->op == PRE_INC_UNARY || u->op == PRE_DEC_UNARY)
                return false;
            return isConstantInit(u->exp);
        }
        case NodeKind::BINARY_EXP: {
            BinaryExp* b = static_cast<BinaryExp*>(e);
            if (b->op == LSHIFT_OP || b->op == RSHIFT_OP) {
                // los desplazamientos se admiten solo entre constantes enteras
            }
            return isConstantInit(b->left) && isConstantInit(b->right);
        }
        case NodeKind::BRACED_INIT: {
            BracedInitList* bl = static_cast<BracedInitList*>(e);
            for (Exp* x : bl->elements) {
                if (!isConstantInit(x)) return false;
            }
            return true;
        }
        default:
            return false;
    }
}

// Tamanos, alineaciones y propiedades de ciclo de vida (modelo LP64)

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

bool TypeChecker::isClassObj(const Type& t) const {
    return (t.kind == Type::RECORD || t.kind == Type::STRING) && t.ptrLevel == 0 && !t.isRef;
}

bool TypeChecker::isByValueClass(const Type& t) const {
    return isClassObj(t) && t.arrayDims.empty();
}

RecordLayout* TypeChecker::layoutOf(const Type& t) {
    if (t.kind != Type::RECORD) return nullptr;
    auto it = records.find(t.recordName);
    return it == records.end() ? nullptr : &it->second->layout;
}

bool TypeChecker::hasStringIn(const Type& t) {
    if (!isClassObj(t)) return false;
    if (t.kind == Type::STRING) return true;
    RecordLayout* L = layoutOf(t);
    return L && L->hasString;
}

bool TypeChecker::dtorTrivial(const Type& t) {
    if (!isClassObj(t)) return true;
    if (t.kind == Type::STRING) return false;
    RecordLayout* L = layoutOf(t);
    return !L || L->dtorTrivial;
}

bool TypeChecker::copyTrivial(const Type& t) {
    if (!isClassObj(t)) return true;
    if (t.kind == Type::STRING) return false;
    RecordLayout* L = layoutOf(t);
    return !L || L->copyTrivial;
}

bool TypeChecker::assignTrivial(const Type& t) {
    if (!isClassObj(t)) return true;
    if (t.kind == Type::STRING) return false;
    RecordLayout* L = layoutOf(t);
    return !L || L->assignTrivial;
}

bool TypeChecker::defaultTrivial(const Type& t) {
    if (!isClassObj(t)) return true;
    if (t.kind == Type::STRING) return false;
    RecordLayout* L = layoutOf(t);
    return !L || L->defaultTrivial;
}

const RecordAbi& TypeChecker::abiOf(const Type& t) {
    static RecordAbi stringAbi;
    stringAbi.trivialForCalls = false;
    RecordLayout* L = layoutOf(t);
    if (L) return L->abi;
    return stringAbi;
}

int TypeChecker::pointeeSize(const Type& ptr, int line, int col) {
    Type p = ptr.pointee();
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

Type TypeChecker::declaredType(const Type& base, int ptrs, bool ref, unsigned ptrConst,
                               const vector<Exp*>& dims, int line, int col) {
    Type t = base;
    t.ptrLevel = ptrs;
    t.isRef = ref;
    t.arrayDims.clear();
    t.constBits = (base.constBits & 1u) | ptrConst;
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
    return info;
}

int TypeChecker::allocTemp(const Type& t) {
    return allocLocal(t).offset;
}

VarInfo TypeChecker::makeVarInfo(const Type& t, bool global, const string& name) {
    if (!global) return allocLocal(t);
    VarInfo info;
    info.type = t;
    info.size = sizeOf(t);
    info.align = alignOf(t);
    info.storage = Storage::GLOBAL;
    info.label = "g_" + name;
    return info;
}

// Clasificacion ABI de records (System V AMD64, seccion 3.2.3)

void TypeChecker::flattenAbi(const Type& t, int off, AbiClass cls[2], bool& bad) {
    if (t.isArray()) {
        Type et = t.elementType();
        int esz = sizeOf(et);
        for (int i = 0; i < t.arrayDims[0]; i++) {
            flattenAbi(et, off + i * esz, cls, bad);
            if (bad) return;
        }
        return;
    }
    if (t.kind == Type::RECORD && t.ptrLevel == 0 && !t.isRef) {
        RecordLayout* L = layoutOf(t);
        if (!L) return;
        for (const FieldInfo& f : L->fields) {
            flattenAbi(f.type, off + f.offset, cls, bad);
            if (bad) return;
        }
        return;
    }
    int eb = off / 8;
    if (eb > 1) {
        bad = true;
        return;
    }
    AbiClass c = AbiClass::INTEGER;
    if (!t.isRef && t.ptrLevel == 0 && (t.kind == Type::FLOAT || t.kind == Type::DOUBLE)) c = AbiClass::SSE;
    if (cls[eb] == AbiClass::NO_CLASS) cls[eb] = c;
    else if (cls[eb] == AbiClass::INTEGER || c == AbiClass::INTEGER) cls[eb] = AbiClass::INTEGER;
}

void TypeChecker::classifyRecord(RecordDecl* rd) {
    RecordLayout& L = rd->layout;
    RecordAbi& A = L.abi;
    A = RecordAbi();
    A.trivialForCalls = L.copyTrivial && L.dtorTrivial;
    A.empty = L.fields.empty();
    if (!A.trivialForCalls || A.empty) return;
    if (L.size > 16) {
        A.memory = true;
        return;
    }
    AbiClass cls[2] = {AbiClass::NO_CLASS, AbiClass::NO_CLASS};
    bool bad = false;
    for (const FieldInfo& f : L.fields) {
        flattenAbi(f.type, f.offset, cls, bad);
        if (bad) break;
    }
    if (bad) {
        A.memory = true;
        return;
    }
    A.neight = (L.size + 7) / 8;
    for (int i = 0; i < A.neight; i++) {
        A.cls[i] = (cls[i] == AbiClass::NO_CLASS) ? AbiClass::INTEGER : cls[i];
    }
}

// Declaraciones de records y funciones (primer pase, en orden fuente)

FunDecl* TypeChecker::makeImplicit(RecordDecl* rd, FunKind kind, bool copyParam) {
    vector<Param*> ps;
    if (copyParam) {
        Type pt = Type::makeRecord(rd->name);
        pt.constBits |= 1u;
        ps.push_back(new Param(pt, "other", 0, true, {}, rd->line, rd->col));
    }
    Type rt = Type(Type::VOID);
    string name = rd->name;
    if (kind == FunKind::DTOR) name = "~" + rd->name;
    if (kind == FunKind::COPY_ASSIGN) {
        name = "operator=";
        rt = Type::makeRecord(rd->name, 0, true);
    }
    FunDecl* fd = new FunDecl(rt, name, ps, new BlockStmt(rd->line, rd->col), rd->line, rd->col);
    fd->funKind = kind;
    fd->isImplicit = true;
    rd->implicitFuns.push_back(fd);
    return fd;
}

bool TypeChecker::isCopyCtorSig(const FunDecl* fd, const RecordDecl* rd) const {
    if (fd->params.size() != 1) return false;
    const Param* p = fd->params[0];
    return p->isRef && p->ptrCount == 0 && p->arrayDims.empty() &&
           p->type.kind == Type::RECORD && p->type.recordName == rd->name;
}

void TypeChecker::analyzeSpecialMembers(RecordDecl* rd) {
    RecordLayout& L = rd->layout;
    FunDecl *uDefault = nullptr, *uParam = nullptr, *uCopy = nullptr, *uDtor = nullptr, *uAssign = nullptr;
    bool anyCtor = false;

    for (RecordMember* m : rd->members) {
        if (!m->funDecl) continue;
        FunDecl* fd = m->funDecl;
        fd->isPrivate = (m->access == ACCESS_PRIVATE);
        switch (fd->funKind) {
            case FunKind::CTOR:
                anyCtor = true;
                if (fd->params.empty()) {
                    if (uDefault) addError("Constructor por defecto duplicado en '" + rd->name + "'", fd->line, fd->col);
                    else uDefault = fd;
                } else if (isCopyCtorSig(fd, rd)) {
                    if (uCopy) addError("Constructor de copia duplicado en '" + rd->name + "'", fd->line, fd->col);
                    else uCopy = fd;
                } else {
                    const Param* p0 = fd->params[0];
                    if (fd->params.size() == 1 && p0->type.kind == Type::RECORD && p0->type.recordName == rd->name &&
                        p0->ptrCount == 0 && !p0->isRef) {
                        addError("El constructor de copia debe recibir una referencia: " + rd->name + "(const " +
                                 rd->name + "&)", fd->line, fd->col);
                    } else if (uParam) {
                        addError("Solo se permite un constructor con parámetros en '" + rd->name +
                                 "' (además del constructor por defecto y el de copia)", fd->line, fd->col);
                    } else {
                        uParam = fd;
                    }
                }
                break;
            case FunKind::DTOR:
                if (uDtor) addError("Solo se permite un destructor en '" + rd->name + "'", fd->line, fd->col);
                else uDtor = fd;
                break;
            case FunKind::COPY_ASSIGN: {
                bool okRet = fd->returnType.kind == Type::RECORD && fd->returnType.recordName == rd->name &&
                             fd->returnType.ptrLevel == 0 && fd->returnType.isRef;
                bool okPar = fd->params.size() == 1 && fd->params[0]->isRef && fd->params[0]->ptrCount == 0 &&
                             fd->params[0]->type.kind == Type::RECORD && fd->params[0]->type.recordName == rd->name &&
                             (fd->params[0]->type.constBits & 1u);
                if (!okRet || !okPar) {
                    addError("La asignación de copia debe declararse como '" + rd->name + "& operator=(const " +
                             rd->name + "& otra)'", fd->line, fd->col);
                } else if (uAssign) {
                    addError("Asignación de copia duplicada en '" + rd->name + "'", fd->line, fd->col);
                } else {
                    uAssign = fd;
                }
                break;
            }
            case FunKind::NORMAL:
                if (fd->name == rd->name) {
                    addError("Un método no puede llamarse como la clase: '" + fd->name + "'", fd->line, fd->col);
                }
                break;
        }
    }

    bool fDtorNT = false, fCopyNT = false, fAssignNT = false, fDefaultNT = false;
    bool fDefaultDel = false, fCopyDel = false, fAssignDel = false;
    bool anyPrivate = false;
    for (const FieldInfo& f : L.fields) {
        Type et = f.type;
        et.arrayDims.clear();
        if (f.isPrivate) anyPrivate = true;
        if (!dtorTrivial(et)) fDtorNT = true;
        if (!copyTrivial(et)) fCopyNT = true;
        if (!assignTrivial(et)) fAssignNT = true;

        if (f.classInit) {
            fDefaultNT = true;
        } else if (f.type.isRef) {
            fDefaultDel = true;
        } else if (f.type.isConstObj() && !isClassObj(et)) {
            fDefaultDel = true;
        } else if (isClassObj(et)) {
            if (!defaultTrivial(et)) fDefaultNT = true;
            RecordLayout* fl = layoutOf(et);
            if (fl && fl->defaultCtorAvail != Avail::USER && fl->defaultCtorAvail != Avail::IMPLICIT) fDefaultDel = true;
        }
        if (f.type.isRef || f.type.isConstObj()) fAssignDel = true;
        if (RecordLayout* fl = layoutOf(et)) {
            if (isClassObj(et)) {
                if (fl->copyCtorAvail == Avail::DELETED) fCopyDel = true;
                if (fl->copyAssignAvail == Avail::DELETED) fAssignDel = true;
            }
        }
    }

    // constructor por defecto
    if (uDefault) {
        L.defaultCtorAvail = Avail::USER;
        L.defaultCtor = uDefault;
        L.defaultTrivial = false;
    } else if (!anyCtor) {
        if (fDefaultDel) {
            L.defaultCtorAvail = Avail::DELETED;
            L.defaultTrivial = false;
        } else {
            L.defaultCtorAvail = Avail::IMPLICIT;
            L.defaultTrivial = !fDefaultNT;
        }
    } else {
        L.defaultCtorAvail = Avail::NONE;
        L.defaultTrivial = false;
    }

    // constructor con parametros
    L.paramCtorAvail = uParam ? Avail::USER : Avail::NONE;
    L.paramCtor = uParam;

    // constructor de copia
    if (uCopy) {
        L.copyCtorAvail = Avail::USER;
        L.copyCtor = uCopy;
        L.copyTrivial = false;
    } else {
        L.copyCtorAvail = fCopyDel ? Avail::DELETED : Avail::IMPLICIT;
        L.copyTrivial = !fCopyNT;
    }

    // asignacion de copia
    if (uAssign) {
        L.copyAssignAvail = Avail::USER;
        L.copyAssign = uAssign;
        L.assignTrivial = false;
    } else {
        L.copyAssignAvail = fAssignDel ? Avail::DELETED : Avail::IMPLICIT;
        L.assignTrivial = !fAssignNT;
    }

    // destructor
    if (uDtor) {
        L.dtorAvail = Avail::USER;
        L.dtor = uDtor;
        L.dtorTrivial = false;
    } else {
        L.dtorAvail = Avail::IMPLICIT;
        L.dtorTrivial = !fDtorNT;
    }

    L.isAggregate = !anyCtor && !anyPrivate;

    if (L.defaultCtorAvail == Avail::IMPLICIT && !L.defaultTrivial) L.defaultCtor = makeImplicit(rd, FunKind::CTOR, false);
    if (L.copyCtorAvail == Avail::IMPLICIT && !L.copyTrivial) L.copyCtor = makeImplicit(rd, FunKind::CTOR, true);
    if (L.copyAssignAvail == Avail::IMPLICIT && !L.assignTrivial) L.copyAssign = makeImplicit(rd, FunKind::COPY_ASSIGN, true);
    if (L.dtorAvail == Avail::IMPLICIT && !L.dtorTrivial) L.dtor = makeImplicit(rd, FunKind::DTOR, false);

    classifyRecord(rd);
}

void TypeChecker::declareRecord(RecordDecl* rd) {
    if (records.count(rd->name)) {
        addError("Redefinición de clase/struct '" + rd->name + "'", rd->line, rd->col);
        return;
    }
    records[rd->name] = rd;
    for (FunDecl* f : rd->implicitFuns) delete f;
    rd->implicitFuns.clear();
    rd->layout = RecordLayout();
    RecordLayout& L = rd->layout;
    L.name = rd->name;
    L.complete = false;

    int offset = 0;
    int maxAlign = 1;
    for (RecordMember* m : rd->members) {
        if (!m->varDecl) continue;
        for (VarInit* vi : m->varDecl->inits) {
            Type t = declaredType(m->varDecl->baseType, vi->ptrCount, vi->isRef, vi->ptrConst, vi->arrayDims,
                                  vi->line, vi->col);
            if (t.isError()) continue;
            if (L.findField(vi->name)) {
                addError("Campo duplicado: '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            if (t.kind == Type::VOID && t.ptrLevel == 0) {
                addError("Un campo no puede ser de tipo void: '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            if (t.isRef && t.isArray()) {
                addError("Arreglo de referencias no permitido: '" + vi->name + "'", vi->line, vi->col);
                continue;
            }
            if (t.kind == Type::RECORD && t.ptrLevel == 0 && !t.isRef) {
                if (!records[t.recordName]->layout.complete) {
                    addError("El tipo '" + t.recordName + "' es incompleto como campo por valor", vi->line, vi->col);
                    continue;
                }
            }
            if (vi->initializer) {
                if (t.isRef) {
                    addError("Inicializador en la clase para una referencia no soportado: '" + vi->name + "'",
                             vi->line, vi->col);
                    continue;
                }
                if (!isConstantInit(vi->initializer)) {
                    addError("El inicializador de campo '" + vi->name +
                             "' debe ser una expresión constante (limitación del subconjunto)", vi->line, vi->col);
                    continue;
                }
            }
            FieldInfo f;
            f.name = vi->name;
            f.type = t;
            f.size = sizeOf(t);
            f.align = alignOf(t);
            f.isPrivate = (m->access == ACCESS_PRIVATE);
            f.classInit = vi->initializer;
            f.line = vi->line;
            f.col = vi->col;
            offset = alignUp(offset, f.align);
            f.offset = offset;
            offset += f.size;
            if (f.align > maxAlign) maxAlign = f.align;
            if (hasStringIn(t)) L.hasString = true;
            L.fields.push_back(f);
        }
    }
    L.align = maxAlign;
    L.size = alignUp(offset > 0 ? offset : 1, maxAlign);
    L.complete = true;

    // los inicializadores constantes se verifican una sola vez, fuera de cualquier funcion
    int savedFrame = frameUsed;
    frameUsed = 0;
    for (FieldInfo& f : L.fields) {
        if (f.classInit) checkInitializer(f.type, f.classInit, f.line, f.col);
    }
    frameUsed = savedFrame;

    analyzeSpecialMembers(rd);

    for (RecordMember* m : rd->members) {
        if (!m->funDecl) continue;
        declareFunction(m->funDecl, rd);
    }
    for (FunDecl* f : rd->implicitFuns) {
        declareFunction(f, rd);
    }
}

void TypeChecker::declareFunction(FunDecl* fd, RecordDecl* owner) {
    FunctionLayout& L = fd->layout;
    L = FunctionLayout();
    L.isMethod = (owner != nullptr);
    L.ownerRecord = owner ? owner->name : "";
    if (owner) {
        switch (fd->funKind) {
            case FunKind::CTOR:
                if (fd->params.empty()) L.label = "ctor_" + owner->name + "_default";
                else if (isCopyCtorSig(fd, owner)) L.label = "ctor_" + owner->name + "_copy";
                else L.label = "ctor_" + owner->name + "_args";
                break;
            case FunKind::DTOR:        L.label = "dtor_" + owner->name; break;
            case FunKind::COPY_ASSIGN: L.label = "assign_" + owner->name; break;
            default:                   L.label = "m_" + owner->name + "_" + fd->name; break;
        }
    } else {
        L.label = (fd->name == "main") ? "main" : "f_" + fd->name;
    }

    Type rt;
    if (fd->funKind == FunKind::CTOR || fd->funKind == FunKind::DTOR) {
        rt = Type(Type::VOID);
    } else {
        rt = resolveType(fd->returnType, fd->line, fd->col);
        if (!rt.isError() && rt.isRef && rt.isArray()) {
            addError("Retorno de arreglo no permitido en '" + fd->name + "'", fd->line, fd->col);
        }
    }
    L.returnType = rt;

    if (owner) {
        VarInfo thisInfo;
        thisInfo.type = Type::makeRecord(owner->name, 1);
        if (fd->isConst && fd->funKind == FunKind::NORMAL) thisInfo.type.constBits |= 1u;
        L.params.push_back(thisInfo);
    }

    for (Param* p : fd->params) {
        Type t = p->type;
        t.ptrLevel = p->ptrCount;
        t.isRef = p->isRef;
        t.arrayDims.clear();
        t.constBits = (p->type.constBits & 1u) | p->ptrConst;
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
            }
        }
        VarInfo vi;
        vi.type = t;
        L.params.push_back(vi);
    }

    computeAbi(fd);

    if (owner) {
        if (fd->funKind == FunKind::NORMAL) {
            RecordLayout& RL = owner->layout;
            if (RL.methods.count(fd->name) || RL.findField(fd->name)) {
                addError("Redefinición del miembro '" + fd->name + "' en '" + owner->name + "'", fd->line, fd->col);
                return;
            }
            MethodInfo mi;
            mi.fun = fd;
            mi.isPrivate = fd->isPrivate;
            RL.methods[fd->name] = mi;
        }
    } else {
        if (functions.count(fd->name)) {
            addError("Redefinición de la función '" + fd->name + "'", fd->line, fd->col);
        } else {
            functions[fd->name] = fd;
        }
    }
}

// Asigna registros y pila a cada parametro y al retorno segun System V AMD64
void TypeChecker::computeAbi(FunDecl* fd) {
    FunctionLayout& L = fd->layout;
    int ni = 0, ns = 0, stack = 0;

    AbiLoc ra;
    const Type& rt = L.returnType;
    if (rt.isError() || rt.isVoid()) {
        ra.pass = Pass::VOID_RET;
    } else if (!rt.isRef && isByValueClass(rt)) {
        const RecordAbi& ab = abiOf(rt);
        ra.size = sizeOf(rt);
        if (ab.empty && rt.kind == Type::RECORD) {
            ra.pass = Pass::EMPTY;
        } else if (!ab.trivialForCalls || ab.memory) {
            ra.pass = Pass::HIDDEN_PTR;
            ni = 1;
        } else {
            ra.pass = Pass::AGG_REGS;
            ra.neight = ab.neight;
            int ri = 0, rs = 0;
            for (int i = 0; i < ab.neight; i++) {
                ra.cls[i] = ab.cls[i];
                ra.reg[i] = (ab.cls[i] == AbiClass::SSE) ? rs++ : ri++;
            }
        }
    } else if (!rt.isRef && rt.ptrLevel == 0 && rt.arrayDims.empty() &&
               (rt.kind == Type::FLOAT || rt.kind == Type::DOUBLE)) {
        ra.pass = Pass::SSE_REG;
    } else {
        ra.pass = Pass::INT_REG;
    }
    L.retAbi = ra;

    L.paramAbi.assign(L.params.size(), AbiLoc());
    for (size_t i = 0; i < L.params.size(); i++) {
        const Type& pt = L.params[i].type;
        AbiLoc a;
        a.size = pt.isError() ? 8 : sizeOf(pt);
        bool isSse = !pt.isError() && !pt.isRef && pt.ptrLevel == 0 && pt.arrayDims.empty() &&
                     (pt.kind == Type::FLOAT || pt.kind == Type::DOUBLE);
        bool byValueClass = !pt.isError() && !pt.isRef && isByValueClass(pt);

        if (byValueClass) {
            const RecordAbi& ab = abiOf(pt);
            if (ab.empty && pt.kind == Type::RECORD) {
                a.pass = Pass::EMPTY;
                L.paramAbi[i] = a;
                continue;
            }
            if (!ab.trivialForCalls) {
                a.pass = Pass::INVISIBLE_REF;
                a.size = 8;
                isSse = false;
            } else if (ab.memory) {
                a.pass = Pass::STACK;
                a.stackBytes = alignUp(a.size, 8);
                a.stackOffset = stack;
                stack += a.stackBytes;
                L.paramAbi[i] = a;
                continue;
            } else {
                int needI = 0, needS = 0;
                for (int k = 0; k < ab.neight; k++) {
                    if (ab.cls[k] == AbiClass::SSE) needS++;
                    else needI++;
                }
                if (ni + needI <= 6 && ns + needS <= 8) {
                    a.pass = Pass::AGG_REGS;
                    a.neight = ab.neight;
                    for (int k = 0; k < ab.neight; k++) {
                        a.cls[k] = ab.cls[k];
                        a.reg[k] = (ab.cls[k] == AbiClass::SSE) ? ns++ : ni++;
                    }
                } else {
                    a.pass = Pass::STACK;
                    a.stackBytes = alignUp(a.size, 8);
                    a.stackOffset = stack;
                    stack += a.stackBytes;
                }
                L.paramAbi[i] = a;
                continue;
            }
        }

        if (isSse) {
            if (ns < 8) {
                a.pass = Pass::SSE_REG;
                a.reg[0] = ns++;
            } else {
                a.pass = Pass::STACK;
                a.stackBytes = 8;
                a.stackOffset = stack;
                stack += 8;
            }
        } else {
            if (a.pass != Pass::INVISIBLE_REF) a.pass = Pass::INT_REG;
            if (ni < 6) {
                a.reg[0] = ni++;
            } else {
                // sin registro: el escalar o el puntero oculto viaja en la pila
                if (a.pass != Pass::INVISIBLE_REF) a.pass = Pass::STACK;
                a.reg[0] = -1;
                a.stackBytes = 8;
                a.stackOffset = stack;
                stack += 8;
            }
        }
        L.paramAbi[i] = a;
    }
    L.stackArgBytes = stack;
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
                    return annotate(exp, l.withoutConst());
                }
                if (exp->op == PLUS_OP && l.isIntegral() && r.isPointer()) {
                    exp->mode = BM_INT_PLUS_PTR;
                    exp->ptrScale = pointeeSize(r, exp->line, exp->col);
                    return annotate(exp, r.withoutConst());
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
            return annotate(exp, t.withoutConst());

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
            return annotate(exp, t.pointee(), true);
        }

        case ADDR_UNARY: {
            if (!exp->exp->isLValue || t0.isArray()) {
                addError("El operador '&' requiere una variable o elemento (lvalue)", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            return annotate(exp, t0.withoutRef().addressOf());
        }

        case PRE_INC_UNARY:
        case PRE_DEC_UNARY:
            if (!exp->exp->isLValue || t0.isArray() || (!t.isNumeric() && !t.isPointer()) || t.isBool()) {
                addError("Operador '++ / --' requiere un lvalue numérico (no bool) o puntero", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            if (t0.isConstObj()) {
                addError("No se puede modificar un objeto const con '++ / --' (tipo '" + t0.toString() + "')",
                         exp->line, exp->col);
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
    if (lt.isConstObj()) {
        addError("No se puede asignar a un objeto const (tipo '" + lt.toString() + "')", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    if (lt.isRecord()) {
        Type vt = rt.withoutRef();
        if (!vt.isRecord() || vt.recordName != lt.recordName) {
            addError("No se puede asignar un valor de tipo '" + rt.toString() +
                     "' a una variable de tipo '" + lt.toString() + "'", exp->line, exp->col);
            return annotate(exp, Type::makeError());
        }
        RecordLayout* L = layoutOf(lt);
        if (L->copyAssignAvail == Avail::DELETED) {
            addError("La asignación de copia de '" + lt.recordName +
                     "' no está disponible (tiene campos const o referencias)", exp->line, exp->col);
            return annotate(exp, Type::makeError());
        }
        return annotate(exp, lt.withoutRef(), true);
    }
    if (!assignable(lt, exp->value)) {
        addError("No se puede asignar un valor de tipo '" + rt.toString() +
                 "' a una variable de tipo '" + lt.toString() + "'", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }
    return annotate(exp, lt.withoutRef().withoutConst());
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
    Type t = makeType(Type::CHAR, 1);
    t.constBits |= 1u;   // un literal es const char*
    return annotate(exp, t);
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
        exp->isRefVar = vi.type.isRef || vi.indirect;
        Type t = vi.type.withoutRef();
        // en un metodo const los campos propios son const (la referencia no propaga const)
        if (vi.storage == Storage::FIELD && !vi.type.isRef && currentFunction && currentFunction->isConst &&
            currentFunction->funKind == FunKind::NORMAL) {
            t = t.withTopConst();
        }
        return annotate(exp, t, true);
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
    exp->fieldIsRef = f->type.isRef;
    Type ft = f->type.withoutRef();
    if (rec.constAt(0) && !f->type.isRef) ft = ft.withTopConst();
    return annotate(exp, ft, true);
}

Type TypeChecker::callResult(PostfixExp* call, FunDecl* fd) {
    Type rt = fd->layout.returnType;
    if (rt.isError()) return annotate(call, rt);
    if (!rt.isRef && isByValueClass(rt)) {
        call->retTemp = allocTemp(rt.withoutConst());
    }
    return annotate(call, rt.withoutRef(), rt.isRef);
}

// Enlace de una referencia a un inicializador. Reglas de C++: un lvalue del mismo tipo sin
// perder const, o un valor temporal solo si la referencia es a const.
bool TypeChecker::bindRefInit(const Type& refType, Exp* init, InitSpec& sp, int line, int col, bool evaluate,
                              const string& what) {
    Type it0 = evaluate ? init->accept(this) : init->inferredType;
    if (it0.isError()) return false;
    Type tb = refType.withoutRef();
    Type ib = it0.withoutRef();
    sp.kind = IK_REF;
    sp.expr = init;
    sp.tempOffset = 0;
    bool sameB = ib.sameBase(tb) && !ib.isArray();

    if (init->isLValue && sameB) {
        if (refBindable(tb, ib)) return true;
        addError("No se puede enlazar " + what + " '" + refType.toString() + "' con un objeto de tipo '" +
                 ib.toString() + "' (se perdería const)", line, col);
        return false;
    }
    if (tb.isConstObj()) {
        if (tb.isRecord()) {
            if (sameB && !init->isLValue) return true;
        } else if (tb.isString()) {
            if (ib.isString() && !init->isLValue) return true;
            Type d = decay(ib);
            if (d.kind == Type::CHAR && d.ptrLevel == 1) {
                sp.tempOffset = allocTemp(Type(Type::STRING));
                return true;
            }
        } else if (assignable(tb, init)) {
            sp.tempOffset = allocTemp(tb.withoutConst());
            return true;
        }
    }
    addError("No se puede enlazar " + what + " '" + refType.toString() + "' con un valor de tipo '" +
             it0.toString() + "'" + (init->isLValue ? "" : " (solo una referencia a const admite temporales)"),
             line, col);
    return false;
}

bool TypeChecker::checkArgList(vector<Exp*>& args, vector<ArgInfo>& info, const vector<VarInfo>& params,
                               size_t offset, const string& name, int line, int col, bool evaluate) {
    info.clear();
    size_t np = params.size() - offset;

    if (evaluate) {
        for (Exp* a : args) a->accept(this);
    }
    if (args.size() != np) {
        addError("La función '" + name + "' espera " + to_string(np) +
                 " argumentos, pero recibió " + to_string(args.size()), line, col);
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < np; i++) {
        Exp* a = args[i];
        Type pt = params[i + offset].type;
        Type at = a->inferredType;
        ArgInfo ai;
        ai.ptype = pt;
        if (pt.isError() || at.isError()) {
            info.push_back(ai);
            continue;
        }
        string what = "Argumento " + to_string(i + 1) + " de '" + name + "'";

        if (pt.isRef) {
            InitSpec sp;
            if (bindRefInit(pt, a, sp, line, col, false, "el parámetro por referencia " + to_string(i + 1) + " de '" + name + "'")) {
                if (sp.tempOffset != 0) {
                    ai.mode = AM_REF_TEMP;
                    ai.tempOffset = sp.tempOffset;
                } else {
                    ai.mode = AM_REF;
                }
            } else {
                ok = false;
            }
        } else if (isByValueClass(pt) && pt.kind == Type::RECORD) {
            Type ab = at.withoutRef();
            if (!ab.isRecord() || ab.recordName != pt.recordName) {
                addError(what + " tiene tipo '" + at.toString() + "', pero se esperaba '" + pt.toString() + "'",
                         line, col);
                ok = false;
            } else if (layoutOf(pt)->copyCtorAvail == Avail::DELETED) {
                addError(what + ": el constructor de copia de '" + pt.recordName + "' no está disponible", line, col);
                ok = false;
            } else if (abiOf(pt).trivialForCalls) {
                ai.mode = AM_AGG;
            } else {
                ai.mode = AM_COPY;
                ai.tempOffset = allocTemp(pt.withoutConst());
            }
        } else if (isByValueClass(pt) && pt.kind == Type::STRING) {
            if (!assignable(pt, a)) {
                addError(what + " tiene tipo '" + at.toString() + "', pero se esperaba '" + pt.toString() + "'",
                         line, col);
                ok = false;
            } else {
                ai.mode = AM_COPY;
                ai.tempOffset = allocTemp(Type(Type::STRING));
            }
        } else if (!assignable(pt, a)) {
            addError(what + " tiene tipo '" + at.toString() + "', pero se esperaba '" + pt.toString() + "'",
                     line, col);
            ok = false;
        }
        info.push_back(ai);
    }
    return ok;
}

// Elige el constructor para T(args): por defecto, de copia o con parametros
bool TypeChecker::resolveCtorCall(const Type& rec, vector<Exp*>& args, vector<ArgInfo>& info, InitSpec& spec,
                                  int line, int col, bool argsEvaluated) {
    if (!argsEvaluated) {
        for (Exp* a : args) a->accept(this);
    }
    spec = InitSpec();
    info.clear();
    for (Exp* a : args) {
        if (a->inferredType.isError()) return false;
    }
    RecordLayout* L = layoutOf(rec);
    if (!L) return false;
    const string& nm = rec.recordName;
    RecordDecl* rd = records[nm];

    auto checkAccess = [&](FunDecl* c) {
        if (c && c->isPrivate && (!currentRecord || currentRecord->name != nm)) {
            addError("El constructor de '" + nm + "' es privado", line, col);
            return false;
        }
        return true;
    };

    if (args.empty()) {
        if (L->defaultCtorAvail == Avail::USER || L->defaultCtorAvail == Avail::IMPLICIT) {
            spec.kind = IK_DEFAULT;
            spec.ctor = L->defaultCtor;
            return checkAccess(L->defaultCtor);
        }
        if (L->defaultCtorAvail == Avail::DELETED) {
            addError("El constructor por defecto de '" + nm +
                     "' no está disponible (hay campos que no se pueden inicializar por defecto)", line, col);
        } else {
            addError("'" + nm + "' no tiene constructor por defecto", line, col);
        }
        return false;
    }

    Type a0 = args[0]->inferredType.withoutRef();
    if (args.size() == 1 && a0.isRecord() && a0.recordName == nm) {
        if (L->copyCtorAvail == Avail::DELETED) {
            addError("El constructor de copia de '" + nm + "' no está disponible", line, col);
            return false;
        }
        spec.kind = IK_COPY;
        spec.expr = args[0];
        spec.ctor = L->copyCtor;
        return checkAccess(L->copyCtor);
    }

    if (L->paramCtorAvail == Avail::USER) {
        FunDecl* c = L->paramCtor;
        if (!checkArgList(args, info, c->layout.params, 1, nm, line, col, false)) return false;
        spec.kind = IK_CTOR;
        spec.args = args;
        spec.argInfo = info;
        spec.ctor = c;
        return checkAccess(c);
    }
    (void)rd;
    addError("'" + nm + "' no tiene un constructor que acepte " + to_string(args.size()) + " argumento(s)", line, col);
    return false;
}

Type TypeChecker::checkCall(PostfixExp* call) {
    Exp* base = call->base;
    call->argInfo.clear();
    call->retTemp = 0;
    call->ctorKind = IK_NONE;

    if (base->nodeKind() == NodeKind::ID_EXP) {
        IdExp* id = static_cast<IdExp*>(base);
        string name = id->getFullName();

        if (currentRecord && id->parts.size() == 1) {
            auto it = currentRecord->layout.methods.find(name);
            if (it != currentRecord->layout.methods.end()) {
                call->callKind = CALL_METHOD_IMPLICIT;
                call->callee = it->second.fun;
                if (currentFunction && currentFunction->isConst && currentFunction->funKind == FunKind::NORMAL &&
                    !call->callee->isConst) {
                    addError("Un método const no puede llamar al método no const '" + name + "'", call->line, call->col);
                }
                checkArgList(call->args, call->argInfo, call->callee->layout.params, 1, name, call->line, call->col);
                return callResult(call, call->callee);
            }
        }

        auto fit = functions.find(name);
        if (fit != functions.end()) {
            call->callKind = CALL_FUNCTION;
            call->callee = fit->second;
            checkArgList(call->args, call->argInfo, call->callee->layout.params, 0, name, call->line, call->col);
            return callResult(call, call->callee);
        }

        auto rit = records.find(name);
        if (rit != records.end() && id->parts.size() == 1) {
            // T(args): construye un temporal
            Type rec = Type::makeRecord(name);
            InitSpec sp;
            vector<ArgInfo> info;
            if (!resolveCtorCall(rec, call->args, info, sp, call->line, call->col, false)) {
                return annotate(call, Type::makeError());
            }
            call->callKind = CALL_CTOR;
            call->callee = sp.ctor;
            call->ctorKind = sp.kind;
            call->argInfo = info;
            call->retTemp = allocTemp(rec);
            return annotate(call, rec);
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
            bool objConst = rec.constAt(0);

            if (rec.kind == Type::STRING) {
                for (Exp* a : call->args) a->accept(this);
                call->callKind = CALL_BUILTIN;
                size_t n = call->args.size();
                auto needMutable = [&]() {
                    if (objConst) {
                        addError("El método '" + m + "' modifica el string, pero el objeto es const", call->line, call->col);
                        return false;
                    }
                    return true;
                };
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
                    if (!needMutable()) return annotate(call, Type::makeError());
                    return annotate(call, makeType(Type::VOID));
                }
                if (m == "c_str" && n == 0) {
                    call->builtin = BI_STR_C_STR;
                    Type ct = makeType(Type::CHAR, 1);
                    ct.constBits |= 1u;
                    return annotate(call, ct);
                }
                if (m == "append" && n == 1) {
                    Type at = decay(call->args[0]->inferredType.withoutRef());
                    if (at.isString()) call->builtin = BI_STR_APPEND_STR;
                    else if (at.kind == Type::CHAR && at.ptrLevel == 1) call->builtin = BI_STR_APPEND_CSTR;
                    else {
                        addError("append espera un string o un char*", call->line, call->col);
                        return annotate(call, Type::makeError());
                    }
                    if (!needMutable()) return annotate(call, Type::makeError());
                    return annotate(call, makeType(Type::VOID));
                }
                if (m == "push_back" && n == 1) {
                    if (!decay(call->args[0]->inferredType.withoutRef()).isChar()) {
                        addError("push_back espera un char", call->line, call->col);
                        return annotate(call, Type::makeError());
                    }
                    call->builtin = BI_STR_APPEND_CHAR;
                    if (!needMutable()) return annotate(call, Type::makeError());
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
                if (objConst && !call->callee->isConst) {
                    addError("No se puede llamar al método no const '" + m + "' sobre un objeto const",
                             call->line, call->col);
                }
                checkArgList(call->args, call->argInfo, call->callee->layout.params, 1, m, call->line, call->col);
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
            if (bt0.isConstObj()) {
                addError("No se puede modificar un objeto const con '++ / --' (tipo '" + bt0.toString() + "')",
                         exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            if (bt.isPointer()) exp->ptrScale = pointeeSize(bt, exp->line, exp->col);
            return annotate(exp, bt0.withoutRef().withoutConst());
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
                res = bt.withoutRef().pointee();
            } else if (bt.isString()) {
                res = makeType(Type::CHAR);
                if (bt.isConstObj()) res = res.withTopConst();
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
    exp->spec = InitSpec();
    exp->hasCookie = false;
    exp->valueInit = false;
    exp->ptrTemp = 0;

    Type t = resolveType(exp->allocType, exp->line, exp->col);
    if (t.isError()) {
        for (Exp* d : exp->arrayDims) d->accept(this);
        for (Exp* a : exp->ctorArgs) a->accept(this);
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

    bool isArr = !exp->arrayDims.empty();
    if (isArr && !exp->ctorArgs.empty()) {
        addError("'new T[n]' no admite argumentos de constructor", exp->line, exp->col);
        return annotate(exp, Type::makeError());
    }

    if (isClassObj(t)) {
        exp->ptrTemp = allocTemp(Type(Type::VOID, 1, false));
        if (!isArr && exp->hasParens) {
            vector<ArgInfo> info;
            if (!resolveCtorCall(t, exp->ctorArgs, info, exp->spec, exp->line, exp->col, false)) {
                return annotate(exp, Type::makeError());
            }
            if (t.kind == Type::RECORD && exp->ctorArgs.empty()) {
                exp->valueInit = layoutOf(t)->defaultCtorAvail != Avail::USER;
            }
        } else if (t.kind == Type::RECORD) {
            if (!needsDefaultInit(t, "'new " + t.recordName + "'", exp->line, exp->col)) {
                return annotate(exp, Type::makeError());
            }
            exp->spec.kind = IK_DEFAULT;
            exp->spec.ctor = layoutOf(t)->defaultCtor;
        } else {
            exp->spec.kind = IK_DEFAULT;
        }
        if (isArr) exp->hasCookie = !dtorTrivial(t);
    } else if (exp->hasParens && !isArr) {
        for (Exp* a : exp->ctorArgs) a->accept(this);
        if (exp->ctorArgs.size() > 1) {
            addError("'new' de un escalar admite a lo sumo un valor inicial", exp->line, exp->col);
            return annotate(exp, Type::makeError());
        }
        if (exp->ctorArgs.empty()) {
            exp->valueInit = true;
            exp->ptrTemp = allocTemp(Type(Type::VOID, 1, false));
        } else if (!exp->ctorArgs[0]->inferredType.isError()) {
            if (!assignable(t, exp->ctorArgs[0])) {
                addError("Valor inicial de tipo '" + exp->ctorArgs[0]->inferredType.toString() +
                         "' incompatible con '" + t.toString() + "'", exp->line, exp->col);
                return annotate(exp, Type::makeError());
            }
            exp->spec.kind = IK_VALUE;
            exp->spec.expr = exp->ctorArgs[0];
            exp->ptrTemp = allocTemp(Type(Type::VOID, 1, false));
        }
    }
    exp->elemSize = sizeOf(t);
    return annotate(exp, t.addressOf());
}

Type TypeChecker::visit(DeleteExp* exp) {
    exp->hasCookie = false;
    Type t = exp->exp->accept(this);
    if (!t.isError() && !t.isPointer()) {
        addError("Operador 'delete' requiere un puntero", exp->line, exp->col);
    } else if (!t.isError()) {
        exp->pointeeType = t.withoutRef().pointee();
        exp->hasCookie = exp->isArray && isClassObj(exp->pointeeType) && !dtorTrivial(exp->pointeeType);
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
        Type tb = retType.withoutRef();
        Type eb = et.withoutRef();
        if (!stm->expr->isLValue || !eb.sameBase(tb) || !refBindable(tb, eb)) {
            addError("Una función que retorna '" + retType.toString() + "' debe devolver una variable de ese tipo" +
                     (stm->expr->isLValue ? " sin perder const" : ""), stm->line, stm->col);
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

bool TypeChecker::needsDefaultInit(const Type& t, const string& what, int line, int col) {
    Type et = t;
    et.arrayDims.clear();
    if (et.kind == Type::RECORD && isClassObj(et)) {
        RecordLayout* L = layoutOf(et);
        if (L && L->defaultCtorAvail != Avail::USER && L->defaultCtorAvail != Avail::IMPLICIT) {
            addError(what + " requiere el constructor por defecto de '" + et.recordName +
                     "', que no existe o no está disponible", line, col);
            return false;
        }
        if (L && L->defaultCtor && L->defaultCtor->isPrivate &&
            (!currentRecord || currentRecord->name != et.recordName)) {
            addError("El constructor por defecto de '" + et.recordName + "' es privado", line, col);
            return false;
        }
    }
    return true;
}

void TypeChecker::checkInitializer(const Type& target, Exp* init, int line, int col) {
    if (init->nodeKind() == NodeKind::BRACED_INIT) {
        BracedInitList* bl = static_cast<BracedInitList*>(init);
        annotate(bl, target);
        bl->aggType = target;
        bl->elementOffsets.clear();
        bl->elementTypes.clear();
        bl->omitted.clear();

        if (target.isArray()) {
            Type et = target.elementType();
            int esz = sizeOf(et);
            int dim0 = target.arrayDims[0];
            if (static_cast<int>(bl->elements.size()) > dim0) {
                addError("Demasiados inicializadores para el arreglo (máximo " + to_string(dim0) + ")", line, col);
            }
            for (size_t i = 0; i < bl->elements.size(); i++) {
                bl->elementOffsets.push_back(static_cast<int>(i) * esz);
                bl->elementTypes.push_back(et);
                checkInitializer(et, bl->elements[i], line, col);
            }
            int given = static_cast<int>(bl->elements.size());
            int rest = dim0 - (given < dim0 ? given : dim0);
            if (rest > 0 && isClassObj(et) && !defaultTrivial(et)) {
                if (needsDefaultInit(et, "Los elementos omitidos del arreglo", line, col)) {
                    BracedInitList::Omitted om;
                    om.offset = given * esz;
                    om.type = et;
                    om.count = rest;
                    om.spec.kind = IK_DEFAULT;
                    RecordLayout* L = layoutOf(et);
                    om.spec.ctor = L ? L->defaultCtor : nullptr;
                    bl->omitted.push_back(om);
                }
            }
        } else if (target.isRecord()) {
            RecordLayout& L = *layoutOf(target);
            if (!L.isAggregate) {
                addError("'" + target.recordName + "' no es un agregado (tiene constructores o miembros privados); "
                         "use '" + target.recordName + " x(args);'", line, col);
                for (Exp* e : bl->elements) {
                    if (e->nodeKind() != NodeKind::BRACED_INIT) e->accept(this);
                }
                return;
            }
            if (bl->elements.size() > L.fields.size()) {
                addError("Demasiados inicializadores para '" + target.recordName + "'", line, col);
            }
            for (size_t i = 0; i < bl->elements.size() && i < L.fields.size(); i++) {
                if (L.fields[i].type.isRef) {
                    addError("Inicializar un campo de referencia con llaves no está soportado: '" +
                             L.fields[i].name + "'", line, col);
                    continue;
                }
                bl->elementOffsets.push_back(L.fields[i].offset);
                bl->elementTypes.push_back(L.fields[i].type);
                checkInitializer(L.fields[i].type, bl->elements[i], line, col);
            }
            for (size_t i = bl->elements.size(); i < L.fields.size(); i++) {
                const FieldInfo& f = L.fields[i];
                if (f.type.isRef) {
                    addError("El campo de referencia '" + f.name + "' debe inicializarse", line, col);
                    continue;
                }
                BracedInitList::Omitted om;
                om.offset = f.offset;
                om.type = f.type;
                if (f.classInit) {
                    om.spec.kind = (f.classInit->nodeKind() == NodeKind::BRACED_INIT) ? IK_BRACED : IK_VALUE;
                    om.spec.expr = f.classInit;
                } else if (isClassObj(f.type) && !defaultTrivial(f.type)) {
                    if (!needsDefaultInit(f.type, "El campo omitido '" + f.name + "'", line, col)) continue;
                    Type et = f.type;
                    om.count = 1;
                    for (int d : et.arrayDims) om.count *= d;
                    et.arrayDims.clear();
                    om.type = et;
                    om.spec.kind = IK_DEFAULT;
                    RecordLayout* fl = layoutOf(et);
                    om.spec.ctor = fl ? fl->defaultCtor : nullptr;
                } else {
                    continue;   // el campo queda en cero
                }
                bl->omitted.push_back(om);
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
    if (target.isArray()) {
        addError("Un arreglo solo puede inicializarse con una lista entre llaves", line, col);
    } else if (target.isRef) {
        addError("No se puede inicializar una referencia dentro de una lista con llaves", line, col);
    } else if (!assignable(target, init)) {
        addError("Inicialización incompatible: tipo '" + target.toString() + "' inicializado con '" +
                 it.toString() + "'", line, col);
    }
}

// Decide como se inicializa la variable y lo deja en vi->spec
void TypeChecker::checkVarInit(VarInit* vi, const Type& t) {
    InitSpec& sp = vi->spec;
    sp = InitSpec();
    int line = vi->line, col = vi->col;
    bool cls = isClassObj(t);

    if (vi->directInit) {
        if (t.isArray()) {
            addError("Un arreglo no puede inicializarse con paréntesis: '" + vi->name + "'", line, col);
            return;
        }
        if (t.isRef) {
            addError("Una referencia debe inicializarse con '='", line, col);
            return;
        }
        if (t.kind == Type::RECORD && cls) {
            if (vi->ctorArgs.empty()) {
                addError("'" + t.recordName + " " + vi->name + "()' declara una función; escriba '" +
                         t.recordName + " " + vi->name + ";'", line, col);
                return;
            }
            vector<ArgInfo> info;
            resolveCtorCall(t, vi->ctorArgs, info, sp, line, col, false);
            return;
        }
        for (Exp* a : vi->ctorArgs) a->accept(this);
        if (vi->ctorArgs.size() != 1) {
            addError("La inicialización con paréntesis de '" + vi->name + "' espera un solo valor", line, col);
            return;
        }
        if (vi->ctorArgs[0]->inferredType.isError()) return;
        if (!assignable(t, vi->ctorArgs[0])) {
            addError("Inicialización incompatible: tipo '" + t.toString() + "' inicializado con '" +
                     vi->ctorArgs[0]->inferredType.toString() + "'", line, col);
            return;
        }
        sp.kind = IK_VALUE;
        sp.expr = vi->ctorArgs[0];
        return;
    }

    if (vi->initializer) {
        Exp* init = vi->initializer;
        if (init->nodeKind() == NodeKind::BRACED_INIT) {
            BracedInitList* bl = static_cast<BracedInitList*>(init);
            if (t.isRef) {
                addError("Una referencia no puede inicializarse con llaves", line, col);
                return;
            }
            if (t.kind == Type::RECORD && cls && t.arrayDims.empty() && !layoutOf(t)->isAggregate) {
                // T t = {a, b} sobre una clase con constructores: se resuelve como T t(a, b)
                annotate(bl, t);
                vector<ArgInfo> info;
                resolveCtorCall(t, bl->elements, info, sp, line, col, false);
                return;
            }
            checkInitializer(t, init, line, col);
            sp.kind = IK_BRACED;
            sp.expr = init;
            return;
        }
        if (t.isRef) {
            bindRefInit(t, init, sp, line, col, true);
            return;
        }
        Type it = init->accept(this);
        if (it.isError()) return;
        if (t.isArray()) {
            addError("Un arreglo solo puede inicializarse con una lista entre llaves", line, col);
            return;
        }
        if (t.kind == Type::RECORD && cls) {
            Type ib = it.withoutRef();
            if (!ib.isRecord() || ib.recordName != t.recordName) {
                addError("Inicialización incompatible: tipo '" + t.toString() + "' inicializado con '" +
                         it.toString() + "' (los constructores de conversión no están soportados)", line, col);
                return;
            }
            RecordLayout* L = layoutOf(t);
            if (L->copyCtorAvail == Avail::DELETED) {
                addError("El constructor de copia de '" + t.recordName + "' no está disponible", line, col);
                return;
            }
            sp.kind = IK_COPY;
            sp.expr = init;
            sp.ctor = L->copyCtor;
            return;
        }
        if (!assignable(t, init)) {
            addError("Inicialización incompatible: tipo '" + t.toString() + "' inicializado con '" +
                     it.toString() + "'", line, col);
            return;
        }
        sp.kind = IK_VALUE;
        sp.expr = init;
        return;
    }

    if (t.isRef) return;   // el parser ya lo rechaza

    if (t.isConstObj()) {
        Type et = t;
        et.arrayDims.clear();
        bool ok = false;
        if (et.kind == Type::STRING && isClassObj(et)) ok = true;
        else if (et.kind == Type::RECORD && isClassObj(et)) ok = layoutOf(et)->defaultCtorAvail == Avail::USER;
        if (!ok) {
            addError("El objeto const '" + vi->name + "' debe inicializarse", line, col);
            return;
        }
    }
    if (cls) {
        Type et = t;
        et.arrayDims.clear();
        if (et.kind == Type::RECORD) {
            if (!needsDefaultInit(et, "La declaración de '" + vi->name + "'", line, col)) return;
            sp.kind = IK_DEFAULT;
            sp.ctor = layoutOf(et)->defaultCtor;
        } else {
            sp.kind = IK_DEFAULT;
        }
    }
}

void TypeChecker::visit(VarDecl* decl) {
    bool global = (currentFunction == nullptr);

    for (VarInit* vi : decl->inits) {
        bool redecl = env.check_current_level(vi->name);
        if (redecl) {
            addError("Redeclaración de variable en el mismo ámbito: '" + vi->name + "'", vi->line, vi->col);
        }

        Type t = declaredType(decl->baseType, vi->ptrCount, vi->isRef, vi->ptrConst, vi->arrayDims, vi->line, vi->col);

        if (!t.isError()) {
            if (t.kind == Type::VOID && t.ptrLevel == 0) {
                addError("Variable de tipo void: '" + vi->name + "'", vi->line, vi->col);
                t = Type::makeError();
            } else if (t.isStream() || t.kind == Type::ENDL) {
                t = Type::makeError();
            } else if (t.isRef && t.isArray()) {
                addError("Arreglo de referencias no permitido: '" + vi->name + "'", vi->line, vi->col);
                t = Type::makeError();
            }
        }

        vi->spec = InitSpec();
        if (t.isError()) {
            if (vi->initializer && vi->initializer->nodeKind() != NodeKind::BRACED_INIT) vi->initializer->accept(this);
            for (Exp* a : vi->ctorArgs) a->accept(this);
        } else {
            checkVarInit(vi, t);
        }

        VarInfo info;
        if (t.isError()) {
            info.type = t;
        } else {
            info = makeVarInfo(t, global, vi->name);
            bool intLike = t.ptrLevel == 0 && !t.isRef && t.arrayDims.empty() &&
                           (t.kind == Type::INT || t.kind == Type::CHAR || t.kind == Type::BOOL);
            if (intLike && t.isConstObj() && vi->initializer && vi->initializer->nodeKind() != NodeKind::BRACED_INIT) {
                long long v;
                if (constEval(vi->initializer, v)) {
                    info.hasConstVal = true;
                    info.constVal = v;
                }
            }
        }
        vi->info = info;
        if (!redecl) env.add_var(vi->name, info);
    }
}

void TypeChecker::visit(Param* param) {
    VarInfo& slot = currentFunction->layout.params[paramIndex];
    const AbiLoc abi = currentFunction->layout.paramAbi[paramIndex];
    Type t = slot.type;
    paramIndex++;

    if (env.check_current_level(param->name)) {
        addError("Nombre de parámetro duplicado: '" + param->name + "'", param->line, param->col);
    }
    VarInfo info;
    if (t.isError()) {
        info.type = t;
    } else if (abi.pass == Pass::STACK) {
        // el parametro vive en el area de argumentos del llamador
        info.type = t;
        info.size = sizeOf(t);
        info.align = alignOf(t);
        info.storage = Storage::LOCAL;
        info.offset = 16 + abi.stackOffset;
    } else if (abi.pass == Pass::INVISIBLE_REF) {
        info = allocLocal(Type(Type::VOID, 1, false));
        info.type = t;
        info.indirect = true;
        if (abi.reg[0] < 0) {
            // el puntero llega por la pila: se guarda igualmente en un slot local
        }
    } else {
        info = allocLocal(t);
    }
    slot = info;
    param->info = info;
    env.add_var(param->name, info);
}

// Plan de inicializacion de los campos de un constructor, en orden de declaracion
void TypeChecker::planCtor(FunDecl* fun, RecordDecl* rec) {
    RecordLayout& L = rec->layout;
    fun->fieldInits.clear();
    bool isCopy = fun->isImplicit && !fun->params.empty();

    // la lista del usuario: nombres validos y sin repeticiones
    unordered_map<string, MemberInit*> listed;
    for (MemberInit& mi : fun->memberInits) {
        if (L.fieldIndex(mi.name) < 0) {
            addError("'" + mi.name + "' no es un campo de '" + rec->name + "'", mi.line, mi.col);
            for (Exp* a : mi.args) a->accept(this);
            continue;
        }
        if (listed.count(mi.name)) {
            addError("El campo '" + mi.name + "' se inicializa más de una vez", mi.line, mi.col);
            for (Exp* a : mi.args) a->accept(this);
            continue;
        }
        listed[mi.name] = &mi;
    }

    for (size_t i = 0; i < L.fields.size(); i++) {
        const FieldInfo& f = L.fields[i];
        FieldInit fi;
        fi.fieldIndex = static_cast<int>(i);
        InitSpec& sp = fi.spec;
        Type et = f.type;
        et.arrayDims.clear();
        bool cls = isClassObj(f.type);

        auto it = listed.find(f.name);
        if (isCopy) {
            sp.kind = IK_COPY;
        } else if (it != listed.end()) {
            MemberInit& mi = *it->second;
            if (f.type.isArray()) {
                addError("Inicializar el arreglo '" + f.name + "' en la lista de inicialización no está soportado",
                         mi.line, mi.col);
                for (Exp* a : mi.args) a->accept(this);
            } else if (f.type.isRef) {
                for (Exp* a : mi.args) a->accept(this);
                if (mi.args.size() != 1) {
                    addError("La referencia '" + f.name + "' se inicializa con un solo valor", mi.line, mi.col);
                } else {
                    bindRefInit(f.type, mi.args[0], sp, mi.line, mi.col, false);
                }
            } else if (f.type.kind == Type::RECORD && cls) {
                vector<ArgInfo> info;
                resolveCtorCall(f.type, mi.args, info, sp, mi.line, mi.col, false);
            } else {
                for (Exp* a : mi.args) a->accept(this);
                if (mi.args.empty()) {
                    if (cls) {
                        sp.kind = IK_DEFAULT;
                    } else {
                        sp.kind = IK_VALUE;   // sin expresion: valor cero
                        sp.expr = nullptr;
                    }
                } else if (mi.args.size() > 1) {
                    addError("El campo '" + f.name + "' se inicializa con un solo valor", mi.line, mi.col);
                } else if (!mi.args[0]->inferredType.isError()) {
                    if (!assignable(f.type, mi.args[0])) {
                        addError("Inicialización incompatible del campo '" + f.name + "': tipo '" +
                                 f.type.toString() + "' inicializado con '" +
                                 mi.args[0]->inferredType.toString() + "'", mi.line, mi.col);
                    } else {
                        sp.kind = IK_VALUE;
                        sp.expr = mi.args[0];
                    }
                }
            }
        } else if (f.classInit) {
            sp.kind = (f.classInit->nodeKind() == NodeKind::BRACED_INIT) ? IK_BRACED : IK_VALUE;
            sp.expr = f.classInit;
        } else if (f.type.isRef) {
            addError("El campo de referencia '" + f.name + "' debe inicializarse en la lista de inicialización de '" +
                     rec->name + "'", fun->line, fun->col);
        } else if (f.type.isConstObj() && !cls) {
            addError("El campo const '" + f.name + "' debe inicializarse en la lista de inicialización de '" +
                     rec->name + "'", fun->line, fun->col);
        } else if (cls) {
            if (needsDefaultInit(f.type, "El campo '" + f.name + "' (inicializar en la lista si no tiene constructor por defecto)",
                                 fun->line, fun->col)) {
                sp.kind = IK_DEFAULT;
                RecordLayout* fl = layoutOf(et);
                sp.ctor = fl ? fl->defaultCtor : nullptr;
            }
        } else {
            sp.kind = IK_NONE;
        }
        fun->fieldInits.push_back(fi);
    }
}

void TypeChecker::visit(FunDecl* fun) {
    currentFunction = fun;
    frameUsed = 0;
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

    // slots del retorno: puntero oculto o valor para retorno por registros
    if (fun->layout.retAbi.pass == Pass::HIDDEN_PTR) {
        fun->layout.retPtrOffset = allocLocal(Type(Type::VOID, 1, false)).offset;
    } else if (fun->layout.retAbi.pass == Pass::AGG_REGS) {
        frameUsed = alignUp(frameUsed, 8) + 16;
        fun->layout.retValOffset = -frameUsed;
    }

    if (fun->funKind == FunKind::CTOR && currentRecord) {
        planCtor(fun, currentRecord);
    }

    if (fun->body) {
        // los items se visitan en el mismo nivel de los parametros
        for (ASTNode* item : fun->body->items) {
            item->accept(this);
        }
    }

    env.remove_level();

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
    for (FunDecl* f : record->implicitFuns) {
        f->accept(this);
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

    // Primer pase en orden fuente: constantes globales, layout de records y firmas de funciones
    globalConsts.clear();
    for (ASTNode* decl : prog->declarations) {
        if (decl->nodeKind() == NodeKind::VAR_DECL) {
            VarDecl* vd = static_cast<VarDecl*>(decl);
            if (!(vd->baseType.constBits & 1u)) continue;
            Type::Kind k = vd->baseType.kind;
            if (k != Type::INT && k != Type::CHAR && k != Type::BOOL) continue;
            for (VarInit* vi : vd->inits) {
                long long v;
                if (vi->ptrCount == 0 && !vi->isRef && vi->arrayDims.empty() && vi->initializer &&
                    vi->initializer->nodeKind() != NodeKind::BRACED_INIT && constEval(vi->initializer, v)) {
                    globalConsts[vi->name] = v;
                }
            }
        } else if (decl->nodeKind() == NodeKind::RECORD_DECL) {
            declareRecord(static_cast<RecordDecl*>(decl));
        } else if (decl->nodeKind() == NodeKind::FUN_DECL) {
            declareFunction(static_cast<FunDecl*>(decl), nullptr);
        }
    }

    // Segundo pase en orden fuente: globales, cuerpos de funciones y metodos
    for (ASTNode* decl : prog->declarations) {
        if (decl->nodeKind() == NodeKind::VAR_DECL) {
            // las temporales de los inicializadores globales viven en el marco de la rutina de inicio
            frameUsed = initFrameUsed;
            decl->accept(this);
            initFrameUsed = frameUsed;
        } else {
            decl->accept(this);
        }
    }
}
