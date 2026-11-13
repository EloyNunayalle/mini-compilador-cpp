#include <cstdio>
#include <stdexcept>
#include "gencode.h"

using namespace std;

static const char* R64[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static const char* R32[6] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static const char* R8[6]  = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};


Gencode::Gencode(const unordered_map<string, RecordDecl*>& recs)
    : records(recs), cur(nullptr), curFun(nullptr), program(nullptr), pushDepth(0), labelCount(0), constCount(0) {}

void Gencode::generate(Program* prog, ostream& out) {
    cur = &text;
    program = prog;
    prog->accept(this);

    out << text.str();
    if (!bss.str().empty()) {
        out << "\t.bss\n" << bss.str();
    }
    if (!rodata.str().empty()) {
        out << "\t.section .rodata\n" << rodata.str();
    }
    out << "\t.section .note.GNU-stack,\"\",@progbits\n";
}

// Emision

void Gencode::emit(const string& s) {
    *cur << "\t" << s << "\n";
}

void Gencode::label(const string& name) {
    *cur << name << ":\n";
}

string Gencode::newLabel(const string& hint) {
    return ".L" + hint + to_string(labelCount++);
}

string Gencode::escapeString(const string& s) const {
    string r;
    for (unsigned char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c >= 32 && c < 127) r += static_cast<char>(c);
        else {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\%03o", c);
            r += buf;
        }
    }
    return r;
}

string Gencode::internString(const string& s) {
    auto it = stringLabels.find(s);
    if (it != stringLabels.end()) return it->second;
    string lab = ".LS" + to_string(stringLabels.size());
    stringLabels[s] = lab;
    rodata << lab << ":\n\t.string \"" << escapeString(s) << "\"\n";
    return lab;
}

// Tipos de maquina

Gencode::Cls Gencode::cls(const Type& t) {
    if (t.isArray()) return C_AGG;
    if (t.ptrLevel > 0) return C_PTR;
    switch (t.kind) {
        case Type::INT:
        case Type::BOOL:
        case Type::CHAR:   return C_INT;
        case Type::FLOAT:  return C_FLOAT;
        case Type::DOUBLE: return C_DOUBLE;
        case Type::STRING:
        case Type::RECORD: return C_AGG;
        default:           return C_NONE;
    }
}

int Gencode::accessSize(const Type& t) {
    if (t.isRef || t.ptrLevel > 0) return 8;
    switch (t.kind) {
        case Type::BOOL:
        case Type::CHAR:   return 1;
        case Type::INT:
        case Type::FLOAT:  return 4;
        default:           return 8;
    }
}

string Gencode::varMem(const VarInfo& v, int extra) const {
    if (v.storage == Storage::GLOBAL) {
        if (extra == 0) return v.label + "(%rip)";
        return v.label + "+" + to_string(extra) + "(%rip)";
    }
    return to_string(v.offset + extra) + "(%rbp)";
}

int Gencode::thisOffset() const {
    return curFun->layout.params[0].offset;
}

// Movimiento de datos

void Gencode::loadFrom(const Type& t, const string& mem) {
    switch (cls(t)) {
        case C_INT:
            if (t.kind == Type::CHAR) emit("movsbl " + mem + ", %eax");
            else if (t.kind == Type::BOOL) emit("movzbl " + mem + ", %eax");
            else emit("movl " + mem + ", %eax");
            break;
        case C_FLOAT:  emit("movss " + mem + ", %xmm0"); break;
        case C_DOUBLE: emit("movsd " + mem + ", %xmm0"); break;
        case C_PTR:    emit("movq " + mem + ", %rax"); break;
        case C_AGG:
            if (mem != "(%rax)") emit("leaq " + mem + ", %rax");
            break;
        case C_NONE: break;
    }
}

void Gencode::storeTo(const Type& t, const string& mem) {
    switch (cls(t)) {
        case C_INT:
            if (t.kind == Type::INT) emit("movl %eax, " + mem);
            else emit("movb %al, " + mem);
            break;
        case C_FLOAT:  emit("movss %xmm0, " + mem); break;
        case C_DOUBLE: emit("movsd %xmm0, " + mem); break;
        case C_PTR:    emit("movq %rax, " + mem); break;
        default: break;
    }
}

void Gencode::pushTemp(const Type& t) {
    Cls c = cls(t);
    if (c == C_FLOAT || c == C_DOUBLE) {
        emit("subq $8, %rsp");
        emit("movsd %xmm0, (%rsp)");
    } else {
        emit("pushq %rax");
    }
    pushDepth++;
}

// Recupera el ultimo temporal en %rax o %xmm0 segun la clase de t
void Gencode::popToResult(const Type& t) {
    Cls c = cls(t);
    if (c == C_FLOAT || c == C_DOUBLE) {
        emit("movsd (%rsp), %xmm0");
        emit("addq $8, %rsp");
    } else {
        emit("popq %rax");
    }
    pushDepth--;
}

void Gencode::convert(const Type& from, const Type& to) {
    Cls f = cls(from), t = cls(to);
    if (f == C_NONE || t == C_NONE || f == C_AGG || t == C_AGG) return;

    if (to.isBool()) {
        if (from.isBool()) return;
        switch (f) {
            case C_INT:
                emit("testl %eax, %eax");
                emit("setne %al");
                emit("movzbl %al, %eax");
                break;
            case C_PTR:
                emit("testq %rax, %rax");
                emit("setne %al");
                emit("movzbl %al, %eax");
                break;
            case C_FLOAT:
            case C_DOUBLE:
                emit("xorps %xmm1, %xmm1");
                emit(string(f == C_FLOAT ? "ucomiss" : "ucomisd") + " %xmm1, %xmm0");
                emit("setne %al");
                emit("setp %cl");
                emit("orb %cl, %al");
                emit("movzbl %al, %eax");
                break;
            default: break;
        }
        return;
    }

    switch (t) {
        case C_INT:
            if (f == C_INT) {
                if (to.isChar() && !from.isChar()) emit("movsbl %al, %eax");
            } else if (f == C_FLOAT || f == C_DOUBLE) {
                emit(string(f == C_FLOAT ? "cvttss2si" : "cvttsd2si") + " %xmm0, %eax");
                if (to.isChar()) emit("movsbl %al, %eax");
            }
            break;
        case C_FLOAT:
            if (f == C_INT) emit("cvtsi2ssl %eax, %xmm0");
            else if (f == C_DOUBLE) emit("cvtsd2ss %xmm0, %xmm0");
            break;
        case C_DOUBLE:
            if (f == C_INT) emit("cvtsi2sdl %eax, %xmm0");
            else if (f == C_FLOAT) emit("cvtss2sd %xmm0, %xmm0");
            break;
        case C_PTR:
            if (f == C_INT) emit("movslq %eax, %rax");
            break;
        default: break;
    }
}

// Copia size bytes entre direcciones dadas en registros, con %r10 como intermedio
void Gencode::copyBytes(const string& src, const string& dst, int size) {
    int off = 0;
    while (size - off >= 8) {
        emit("movq " + to_string(off) + "(" + src + "), %r10");
        emit("movq %r10, " + to_string(off) + "(" + dst + ")");
        off += 8;
    }
    if (size - off >= 4) {
        emit("movl " + to_string(off) + "(" + src + "), %r10d");
        emit("movl %r10d, " + to_string(off) + "(" + dst + ")");
        off += 4;
    }
    if (size - off >= 2) {
        emit("movw " + to_string(off) + "(" + src + "), %r10w");
        emit("movw %r10w, " + to_string(off) + "(" + dst + ")");
        off += 2;
    }
    if (size - off >= 1) {
        emit("movb " + to_string(off) + "(" + src + "), %r10b");
        emit("movb %r10b, " + to_string(off) + "(" + dst + ")");
    }
}

// Alinea %rsp a 16 antes de call: los pushes pendientes (8 bytes) deben ser pares
void Gencode::emitCall(const string& target) {
    bool pad = (pushDepth % 2) != 0;
    if (pad) emit("subq $8, %rsp");
    emit("call " + target);
    if (pad) emit("addq $8, %rsp");
}

void Gencode::callRuntime(const string& fn) {
    emitCall(fn + "@PLT");
}

RecordLayout* Gencode::layoutOf(const Type& t) const {
    if (t.kind != Type::RECORD) return nullptr;
    auto it = records.find(t.recordName);
    return it == records.end() ? nullptr : &it->second->layout;
}

bool Gencode::needsDtor(const Type& t0) const {
    Type t = t0;
    t.arrayDims.clear();
    if (t.ptrLevel > 0 || t.isRef) return false;
    if (t.kind == Type::STRING) return true;
    RecordLayout* L = layoutOf(t);
    return L && !L->dtorTrivial;
}

int Gencode::objSize(const Type& t) const {
    long long count = 1;
    for (int d : t.arrayDims) count *= d;
    int es = 8;
    if (t.isRef || t.ptrLevel > 0) es = 8;
    else if (t.kind == Type::STRING) es = 16;
    else if (t.kind == Type::RECORD) es = layoutOf(t)->size;
    else es = accessSize(t);
    return static_cast<int>(es * count);
}

// Lugares de memoria

Gencode::Place Gencode::placeOf(const VarInfo& v) const {
    Place p;
    if (v.storage == Storage::GLOBAL) {
        p.kind = Place::GLOBAL;
        p.label = v.label;
    } else if (v.indirect) {
        p.kind = Place::SLOT_PTR;
        p.off = v.offset;
    } else {
        p.kind = Place::FRAME;
        p.off = v.offset;
    }
    return p;
}

Gencode::Place Gencode::frameAt(int off) const {
    Place p;
    p.kind = Place::FRAME;
    p.off = off;
    return p;
}

Gencode::Place Gencode::thisPlace(int extra) const {
    Place p;
    p.kind = Place::THIS_FIELD;
    p.extra = extra;
    return p;
}

Gencode::Place Gencode::plus(const Place& p, int delta) const {
    Place q = p;
    q.extra += delta;
    return q;
}

bool Gencode::isDirect(const Place& p) const {
    return p.kind == Place::FRAME || p.kind == Place::GLOBAL;
}

string Gencode::placeMem(const Place& p) const {
    if (p.kind == Place::GLOBAL) {
        if (p.extra == 0) return p.label + "(%rip)";
        return p.label + "+" + to_string(p.extra) + "(%rip)";
    }
    return to_string(p.off + p.extra) + "(%rbp)";
}

void Gencode::loadAddr(const Place& p, const string& reg) {
    switch (p.kind) {
        case Place::FRAME:
            emit("leaq " + to_string(p.off + p.extra) + "(%rbp), " + reg);
            break;
        case Place::GLOBAL:
            emit("leaq " + placeMem(p) + ", " + reg);
            break;
        case Place::THIS_FIELD:
            emit("movq " + to_string(thisOffset()) + "(%rbp), " + reg);
            if (p.extra != 0) emit("addq $" + to_string(p.extra) + ", " + reg);
            break;
        case Place::SLOT_PTR:
            emit("movq " + to_string(p.off) + "(%rbp), " + reg);
            if (p.extra != 0) emit("addq $" + to_string(p.extra) + ", " + reg);
            break;
    }
}

// Guarda el valor de %rax o %xmm0 en el lugar dado
void Gencode::storeToPlace(const Type& t, const Place& p) {
    if (isDirect(p)) {
        storeTo(t, placeMem(p));
        return;
    }
    pushTemp(t);
    loadAddr(p, "%rcx");
    popToResult(t);
    storeTo(t, "(%rcx)");
}

void Gencode::zeroFill(const Place& p, int size) {
    loadAddr(p, "%rdi");
    emit("xorl %eax, %eax");
    emit("movq $" + to_string(size) + ", %rcx");
    emit("rep stosb");
}

// Carga n bytes (1 a 8) desde off(base) en %rcx, con los bytes altos en cero
void Gencode::loadPartial(const string& base, int off, int n) {
    auto m = [&](int d) { return to_string(off + d) + "(" + base + ")"; };
    switch (n) {
        case 8: emit("movq " + m(0) + ", %rcx"); break;
        case 4: emit("movl " + m(0) + ", %ecx"); break;
        case 2: emit("movzwl " + m(0) + ", %ecx"); break;
        case 1: emit("movzbl " + m(0) + ", %ecx"); break;
        case 3:
            emit("movzwl " + m(0) + ", %ecx");
            emit("movzbl " + m(2) + ", %r10d");
            emit("shll $16, %r10d");
            emit("orl %r10d, %ecx");
            break;
        case 5:
            emit("movl " + m(0) + ", %ecx");
            emit("movzbl " + m(4) + ", %r10d");
            emit("shlq $32, %r10");
            emit("orq %r10, %rcx");
            break;
        case 6:
            emit("movl " + m(0) + ", %ecx");
            emit("movzwl " + m(4) + ", %r10d");
            emit("shlq $32, %r10");
            emit("orq %r10, %rcx");
            break;
        default:
            emit("movl " + m(0) + ", %ecx");
            emit("movzwl " + m(4) + ", %r10d");
            emit("movzbl " + m(6) + ", %r11d");
            emit("shll $16, %r11d");
            emit("orl %r11d, %r10d");
            emit("shlq $32, %r10");
            emit("orq %r10, %rcx");
            break;
    }
}

// Escribe los n bytes bajos de src64 (1 a 8) en off(base) sin tocar mas memoria
void Gencode::storePartial(const string& src64, const string& base, int off, int n) {
    emit("movq " + src64 + ", %r10");
    int pos = 0;
    int rem = n;
    while (rem > 0) {
        int c = rem >= 8 ? 8 : rem >= 4 ? 4 : rem >= 2 ? 2 : 1;
        string mem = to_string(off + pos) + "(" + base + ")";
        if (c == 8) emit("movq %r10, " + mem);
        else if (c == 4) emit("movl %r10d, " + mem);
        else if (c == 2) emit("movw %r10w, " + mem);
        else emit("movb %r10b, " + mem);
        rem -= c;
        pos += c;
        if (rem > 0) emit("shrq $" + to_string(c * 8) + ", %r10");
    }
}

// Ciclo de vida de objetos

// Aplica la operacion sobre el objeto de tipo t: %rdi es el destino y %rsi el origen
void Gencode::genOp(OpKind op, const Type& t) {
    if (t.isArray()) {
        Type et = t.elementType();
        int esz = objSize(et);
        int n = t.arrayDims[0];
        bool classElem = (et.kind == Type::RECORD || et.kind == Type::STRING) && et.ptrLevel == 0 && !et.isRef;
        RecordLayout* el = layoutOf(et);
        bool trivial = true;
        if (classElem) {
            if (et.kind == Type::STRING) trivial = false;
            else if (op == OP_DEFAULT) trivial = el->defaultTrivial;
            else if (op == OP_COPY) trivial = el->copyTrivial;
            else if (op == OP_ASSIGN) trivial = el->assignTrivial;
            else trivial = el->dtorTrivial;
        }
        if (et.isArray()) trivial = false;
        if (trivial) {
            if (op == OP_COPY || op == OP_ASSIGN) copyBytes("%rsi", "%rdi", objSize(t));
            return;
        }
        string lloop = newLabel("aop");
        string ldone = newLabel("aopend");
        bool twoPtr = (op == OP_COPY || op == OP_ASSIGN);
        emit("pushq %rdi");
        emit("pushq %rsi");
        pushDepth += 2;
        if (op == OP_DESTROY) {
            emit("pushq $" + to_string(n));
            pushDepth++;
            label(lloop);
            emit("cmpq $0, (%rsp)");
            emit("je " + ldone);
            emit("decq (%rsp)");
        } else {
            emit("pushq $0");
            pushDepth++;
            label(lloop);
            emit("cmpq $" + to_string(n) + ", (%rsp)");
            emit("jge " + ldone);
        }
        emit("movq (%rsp), %rax");
        emit("imulq $" + to_string(esz) + ", %rax");
        emit("movq 16(%rsp), %rdi");
        emit("addq %rax, %rdi");
        if (twoPtr) {
            emit("movq 8(%rsp), %rsi");
            emit("addq %rax, %rsi");
        }
        genOp(op, et);
        if (op != OP_DESTROY) emit("incq (%rsp)");
        emit("jmp " + lloop);
        label(ldone);
        emit("addq $24, %rsp");
        pushDepth -= 3;
        return;
    }

    bool isClass = (t.kind == Type::RECORD || t.kind == Type::STRING) && t.ptrLevel == 0 && !t.isRef;
    if (!isClass) {
        if (op == OP_COPY || op == OP_ASSIGN) copyBytes("%rsi", "%rdi", objSize(t));
        return;
    }
    if (t.kind == Type::STRING) {
        switch (op) {
            case OP_DEFAULT: callRuntime("rt_str_init"); break;
            case OP_COPY:    callRuntime("rt_str_copy_init"); break;
            case OP_ASSIGN:  callRuntime("rt_str_assign"); break;
            case OP_DESTROY: callRuntime("rt_str_free"); break;
        }
        return;
    }
    RecordLayout* L = layoutOf(t);
    FunDecl* f = nullptr;
    switch (op) {
        case OP_DEFAULT: f = L->defaultCtor; break;
        case OP_COPY:    f = L->copyCtor; break;
        case OP_ASSIGN:  f = L->copyAssign; break;
        case OP_DESTROY: f = L->dtor; break;
    }
    if (f) {
        emitCall(f->layout.label);
    } else if (op == OP_COPY || op == OP_ASSIGN) {
        copyBytes("%rsi", "%rdi", L->size);
    }
}

void Gencode::registerTemp(int offset, const Type& t) {
    if (needsDtor(t)) tempStack.push_back({frameAt(offset), t});
}

void Gencode::saveLive(const Type* live) {
    if (!live) return;
    pushTemp(*live);
}

void Gencode::restoreLive(const Type* live) {
    if (!live) return;
    popToResult(*live);
}

void Gencode::emitCleanupItems(const vector<CleanupItem>& items) {
    for (size_t i = items.size(); i > 0; i--) {
        const CleanupItem& it = items[i - 1];
        loadAddr(it.place, "%rdi");
        genOp(OP_DESTROY, it.type);
    }
}

// Destruye los temporales creados desde mark, en orden inverso. live es el valor
// que hay que conservar en %rax o %xmm0 mientras corren los destructores.
void Gencode::flushTemps(size_t mark, const Type* live) {
    if (tempStack.size() <= mark) return;
    saveLive(live);
    vector<CleanupItem> items(tempStack.begin() + mark, tempStack.end());
    tempStack.resize(mark);
    emitCleanupItems(items);
    restoreLive(live);
}

// Un temporal enlazado a una referencia const vive hasta el fin del scope
void Gencode::moveTempsToScope(size_t mark) {
    for (size_t i = mark; i < tempStack.size(); i++) {
        if (scopes.empty()) globalDtors.push_back(tempStack[i]);
        else scopes.back().push_back(tempStack[i]);
    }
    tempStack.resize(mark);
}

// Destruye los objetos de los scopes desde el mas interno hasta downTo, sin cerrarlos
void Gencode::emitScopeCleanup(size_t downTo, const Type* live) {
    bool any = false;
    for (size_t s = downTo; s < scopes.size(); s++) {
        if (!scopes[s].empty()) any = true;
    }
    if (!any) return;
    saveLive(live);
    for (size_t s = scopes.size(); s > downTo; s--) {
        emitCleanupItems(scopes[s - 1]);
    }
    restoreLive(live);
}

void Gencode::pushScope() {
    scopes.push_back(vector<CleanupItem>());
}

void Gencode::popScope() {
    scopes.pop_back();
}

// Inicializa un objeto nuevo de tipo t en pl segun el plan del TypeChecker
void Gencode::genInitObject(const Type& t, const InitSpec& sp, const Place& pl) {
    switch (sp.kind) {
        case IK_NONE:
            return;
        case IK_VALUE:
            if (!sp.expr) {
                zeroFill(pl, objSize(t));
            } else {
                genInitFromExpr(t, sp.expr, pl);
            }
            return;
        case IK_REF: {
            Type pt(Type::VOID, 1, false);
            if (sp.tempOffset != 0) {
                Type tb = t.withoutRef().withoutConst();
                if (tb.isString()) {
                    sp.expr->accept(this);
                    emit("movq %rax, %rsi");
                    emit("leaq " + to_string(sp.tempOffset) + "(%rbp), %rdi");
                    callRuntime("rt_str_init_cstr");
                    registerTemp(sp.tempOffset, tb);
                } else {
                    Type vt = sp.expr->accept(this);
                    convert(vt, tb);
                    storeTo(tb, to_string(sp.tempOffset) + "(%rbp)");
                }
                emit("leaq " + to_string(sp.tempOffset) + "(%rbp), %rax");
            } else {
                genObjAddr(sp.expr);
            }
            storeToPlace(pt, pl);
            return;
        }
        case IK_DEFAULT:
            loadAddr(pl, "%rdi");
            genOp(OP_DEFAULT, t);
            return;
        case IK_COPY:
            genObjAddr(sp.expr);
            emit("movq %rax, %rsi");
            loadAddr(pl, "%rdi");
            genOp(OP_COPY, t);
            return;
        case IK_CTOR: {
            vector<ArgSpec> args;
            const FunctionLayout& L = sp.ctor->layout;
            ArgSpec th;
            th.kind = AK_PLACE;
            th.place = pl;
            th.abi = &L.paramAbi[0];
            args.push_back(th);
            for (size_t i = 0; i < sp.args.size(); i++) {
                ArgSpec a;
                a.kind = AK_VALUE;
                a.exp = sp.args[i];
                a.info = sp.argInfo[i];
                a.abi = &L.paramAbi[i + 1];
                args.push_back(a);
            }
            genCallWith(args, L.stackArgBytes, L.label);
            return;
        }
        case IK_BRACED:
            genBraced(t, static_cast<BracedInitList*>(sp.expr), pl, true);
            return;
    }
}

// Construye un objeto de tipo t en pl copiando o convirtiendo desde la expresion e
void Gencode::genInitFromExpr(const Type& t, Exp* e, const Place& pl) {
    if (e->nodeKind() == NodeKind::BRACED_INIT) {
        genBraced(t, static_cast<BracedInitList*>(e), pl, false);
        return;
    }
    if (t.isString()) {
        Type vt = e->accept(this);
        emit("movq %rax, %rsi");
        loadAddr(pl, "%rdi");
        callRuntime(vt.isString() ? "rt_str_copy_init" : "rt_str_init_cstr");
    } else if (t.isRecord()) {
        genObjAddr(e);
        emit("movq %rax, %rsi");
        loadAddr(pl, "%rdi");
        genOp(OP_COPY, t);
    } else {
        Type vt = e->accept(this);
        convert(vt, t);
        storeToPlace(t, pl);
    }
}

void Gencode::genBraced(const Type& t, BracedInitList* bl, const Place& pl, bool top) {
    if (top) zeroFill(pl, objSize(t));
    for (size_t i = 0; i < bl->elements.size() && i < bl->elementTypes.size(); i++) {
        genInitFromExpr(bl->elementTypes[i], bl->elements[i], plus(pl, bl->elementOffsets[i]));
    }
    for (const BracedInitList::Omitted& om : bl->omitted) {
        Place sub = plus(pl, om.offset);
        if (om.spec.kind == IK_DEFAULT) {
            Type at = om.type;
            if (om.count > 1) at.arrayDims = {om.count};
            loadAddr(sub, "%rdi");
            genOp(OP_DEFAULT, at);
        } else if (om.spec.expr) {
            genInitFromExpr(om.type, om.spec.expr, sub);
        }
    }
}

// Direcciones (lvalues)

void Gencode::genObjAddr(Exp* e) {
    if (e->isLValue) genAddr(e);
    else e->accept(this);
}

void Gencode::genAddr(Exp* e) {
    switch (e->nodeKind()) {
        case NodeKind::ID_EXP: {
            IdExp* id = static_cast<IdExp*>(e);
            if (id->idKind == ID_FIELD) {
                emit("movq " + to_string(thisOffset()) + "(%rbp), %rax");
                emit("addq $" + to_string(id->var.offset) + ", %rax");
                if (id->isRefVar) emit("movq (%rax), %rax");
            } else if (id->idKind == ID_VAR) {
                emit(string(id->isRefVar ? "movq " : "leaq ") + varMem(id->var) + ", %rax");
            } else {
                throw logic_error("Gencode: identificador sin direccion");
            }
            return;
        }
        case NodeKind::UNARY_EXP: {
            UnaryExp* u = static_cast<UnaryExp*>(e);
            u->exp->accept(this);
            return;
        }
        case NodeKind::ASSIGN_EXP:
            e->accept(this);
            return;
        case NodeKind::POSTFIX_EXP: {
            PostfixExp* pf = static_cast<PostfixExp*>(e);
            switch (pf->kind) {
                case ARRAY_ACCESS: {
                    if (pf->base->inferredType.isString()) {
                        genObjAddr(pf->base);
                        emit("movq (%rax), %rax");
                    } else {
                        pf->base->accept(this);
                    }
                    pushTemp(Type(Type::VOID, 1, false));
                    pf->indexExp->accept(this);
                    emit("movslq %eax, %rax");
                    emit("imulq $" + to_string(pf->elemSize) + ", %rax");
                    emit("popq %rcx");
                    pushDepth--;
                    emit("addq %rcx, %rax");
                    return;
                }
                case DOT_ACCESS:
                    genObjAddr(pf->base);
                    emit("addq $" + to_string(pf->fieldOffset) + ", %rax");
                    if (pf->fieldIsRef) emit("movq (%rax), %rax");
                    return;
                case ARROW_ACCESS:
                    pf->base->accept(this);
                    emit("addq $" + to_string(pf->fieldOffset) + ", %rax");
                    if (pf->fieldIsRef) emit("movq (%rax), %rax");
                    return;
                case CALL_ACCESS:
                    genCallExp(pf);
                    return;
                default: break;
            }
            break;
        }
        default: break;
    }
    throw logic_error("Gencode: expresion sin direccion");
}

void Gencode::genBool(Exp* e) {
    Type t = e->accept(this);
    convert(t, Type(Type::BOOL, 0, false));
}

// Condicion: los temporales de la expresion se destruyen antes de decidir el salto
void Gencode::genCond(Exp* e) {
    size_t m = tempStack.size();
    genBool(e);
    Type b(Type::BOOL, 0, false);
    flushTemps(m, &b);
}

// Expresiones

Type Gencode::visit(NumberExp* exp) {
    emit("movl $" + to_string(static_cast<int>(exp->value)) + ", %eax");
    return exp->inferredType;
}

Type Gencode::visit(RealExp* exp) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", exp->value);
    string lab = ".LC" + to_string(constCount++);
    rodata << "\t.balign 8\n" << lab << ":\n\t.double " << buf << "\n";
    emit("movsd " + lab + "(%rip), %xmm0");
    return exp->inferredType;
}

Type Gencode::visit(BoolExp* exp) {
    emit(string("movl $") + (exp->value ? "1" : "0") + ", %eax");
    return exp->inferredType;
}

Type Gencode::visit(CharExp* exp) {
    emit("movl $" + to_string(static_cast<int>(exp->value)) + ", %eax");
    return exp->inferredType;
}

Type Gencode::visit(StringExp* exp) {
    emit("leaq " + internString(exp->value) + "(%rip), %rax");
    return exp->inferredType;
}

Type Gencode::visit(NullptrExp* exp) {
    emit("movq $0, %rax");
    return exp->inferredType;
}

Type Gencode::visit(IdExp* exp) {
    if (exp->idKind == ID_COUT || exp->idKind == ID_CIN || exp->idKind == ID_ENDL) {
        return exp->inferredType;
    }
    genAddr(exp);
    loadFrom(exp->inferredType, "(%rax)");
    return exp->inferredType;
}

Type Gencode::visit(BracedInitList* exp) {
    throw logic_error("Gencode: lista de inicializacion fuera de una declaracion");
    return exp->inferredType;
}

Type Gencode::visit(UnaryExp* exp) {
    Type rt = exp->inferredType;
    switch (exp->op) {
        case PLUS_UNARY: {
            Type t = exp->exp->accept(this);
            convert(t, rt);
            break;
        }
        case MINUS_UNARY: {
            Type t = exp->exp->accept(this);
            convert(t, rt);
            switch (cls(rt)) {
                case C_INT:
                    emit("negl %eax");
                    break;
                case C_FLOAT:
                    emit("movd %xmm0, %eax");
                    emit("xorl $0x80000000, %eax");
                    emit("movd %eax, %xmm0");
                    break;
                case C_DOUBLE:
                    emit("movq %xmm0, %rax");
                    emit("btcq $63, %rax");
                    emit("movq %rax, %xmm0");
                    break;
                default: break;
            }
            break;
        }
        case NOT_UNARY:
            genBool(exp->exp);
            emit("xorl $1, %eax");
            break;
        case BIT_NOT_UNARY:
            exp->exp->accept(this);
            emit("notl %eax");
            break;
        case DEREF_UNARY:
            exp->exp->accept(this);
            loadFrom(rt, "(%rax)");
            break;
        case ADDR_UNARY:
            genAddr(exp->exp);
            break;
        case PRE_INC_UNARY:
            genIncDec(exp->exp, true, false, exp->ptrScale);
            break;
        case PRE_DEC_UNARY:
            genIncDec(exp->exp, false, false, exp->ptrScale);
            break;
    }
    return rt;
}

// ++ y --: el valor resultante es el anterior (post) o el nuevo (pre)
void Gencode::genIncDec(Exp* target, bool inc, bool post, int ptrScale) {
    Type t = target->inferredType;
    genAddr(target);
    emit("movq %rax, %rcx");
    const char* op = inc ? "add" : "sub";
    string delta = inc ? "1" : "-1";

    switch (cls(t)) {
        case C_INT: {
            bool isChar = (t.kind == Type::CHAR);
            emit(string(isChar ? "movsbl" : "movl") + " (%rcx), %eax");
            if (post) {
                emit("leal " + delta + "(%rax), %edx");
                emit(string(isChar ? "movb %dl" : "movl %edx") + ", (%rcx)");
            } else {
                emit(string(op) + "l $1, %eax");
                emit(string(isChar ? "movb %al" : "movl %eax") + ", (%rcx)");
            }
            break;
        }
        case C_PTR: {
            emit("movq (%rcx), %rax");
            if (post) {
                string d = inc ? to_string(ptrScale) : "-" + to_string(ptrScale);
                emit("leaq " + d + "(%rax), %rdx");
                emit("movq %rdx, (%rcx)");
            } else {
                emit(string(op) + "q $" + to_string(ptrScale) + ", %rax");
                emit("movq %rax, (%rcx)");
            }
            break;
        }
        case C_FLOAT:
        case C_DOUBLE: {
            bool dbl = (cls(t) == C_DOUBLE);
            string sfx = dbl ? "sd" : "ss";
            emit("mov" + sfx + " (%rcx), %xmm0");
            emit("movl $1, %edx");
            emit(string(dbl ? "cvtsi2sdl" : "cvtsi2ssl") + " %edx, %xmm1");
            if (post) {
                emit("movaps %xmm0, %xmm2");
                emit(string(op) + sfx + " %xmm1, %xmm2");
                emit("mov" + sfx + " %xmm2, (%rcx)");
            } else {
                emit(string(op) + sfx + " %xmm1, %xmm0");
                emit("mov" + sfx + " %xmm0, (%rcx)");
            }
            break;
        }
        default:
            throw logic_error("Gencode: ++/-- sobre un tipo no escalar");
    }
}

Type Gencode::visit(AssignExp* exp) {
    Type tt = exp->target->inferredType;
    Type vt = exp->value->inferredType;

    if (tt.isString()) {
        genAssignString(exp);
        return exp->inferredType;
    }

    if (tt.isRecord()) {
        // valor, luego destino; el resultado es la direccion del destino
        genObjAddr(exp->value);
        pushTemp(Type(Type::VOID, 1, false));
        genAddr(exp->target);
        pushTemp(Type(Type::VOID, 1, false));
        emit("movq (%rsp), %rdi");
        emit("movq 8(%rsp), %rsi");
        genOp(OP_ASSIGN, tt);
        emit("popq %rax");
        emit("addq $8, %rsp");
        pushDepth -= 2;
        return exp->inferredType;
    }

    exp->value->accept(this);
    convert(vt, tt);
    pushTemp(tt);
    genAddr(exp->target);
    emit("movq %rax, %rcx");
    popToResult(tt);
    storeTo(tt, "(%rcx)");
    return exp->inferredType;
}

void Gencode::genAssignString(AssignExp* e) {
    genAddr(e->target);
    pushTemp(Type(Type::VOID, 1, false));
    Type vt = e->value->accept(this);
    emit("movq %rax, %rsi");
    emit("movq (%rsp), %rdi");
    callRuntime(vt.isString() ? "rt_str_assign" : "rt_str_assign_cstr");
    emit("popq %rax");
    pushDepth--;
}

Type Gencode::visit(NewExp* exp) {
    Type t = exp->allocType;
    bool isArr = !exp->arrayDims.empty();
    Type ptrT(Type::VOID, 1, false);
    bool classLike = (exp->ptrTemp != 0);

    if (!isArr) {
        emit("movl $" + to_string(exp->elemSize) + ", %edi");
        callRuntime("malloc");
        if (!classLike) return exp->inferredType;
        string slot = to_string(exp->ptrTemp) + "(%rbp)";
        emit("movq %rax, " + slot);
        Place obj;
        obj.kind = Place::SLOT_PTR;
        obj.off = exp->ptrTemp;
        if (exp->spec.kind == IK_VALUE) {
            Type vt = exp->spec.expr->accept(this);
            convert(vt, t);
            storeToPlace(t, obj);
        } else {
            if (exp->valueInit) zeroFill(obj, exp->elemSize);
            if (exp->spec.kind != IK_NONE) {
                size_t mark = tempStack.size();
                genInitObject(t, exp->spec, obj);
                flushTemps(mark, nullptr);
            }
        }
        emit("movq " + slot + ", %rax");
        return exp->inferredType;
    }

    // new T[n]
    exp->arrayDims[0]->accept(this);
    emit("movslq %eax, %rax");
    if (!classLike) {
        emit("imulq $" + to_string(exp->elemSize) + ", %rax");
        emit("movq %rax, %rdi");
        callRuntime("malloc");
        return exp->inferredType;
    }
    emit("pushq %rax");
    pushDepth++;
    emit("movq %rax, %rdi");
    emit("imulq $" + to_string(exp->elemSize) + ", %rdi");
    if (exp->hasCookie) emit("addq $16, %rdi");
    callRuntime("malloc");
    if (exp->hasCookie) {
        emit("movq (%rsp), %rcx");
        emit("movq %rcx, (%rax)");
        emit("addq $16, %rax");
    }
    emit("movq %rax, " + to_string(exp->ptrTemp) + "(%rbp)");
    if (exp->spec.kind == IK_DEFAULT) {
        string lloop = newLabel("newloop");
        string ldone = newLabel("newend");
        emit("pushq %rax");
        emit("pushq $0");
        pushDepth += 2;
        label(lloop);
        emit("movq (%rsp), %rax");
        emit("cmpq 16(%rsp), %rax");
        emit("jge " + ldone);
        emit("imulq $" + to_string(exp->elemSize) + ", %rax");
        emit("movq 8(%rsp), %rdi");
        emit("addq %rax, %rdi");
        genOp(OP_DEFAULT, t);
        emit("incq (%rsp)");
        emit("jmp " + lloop);
        label(ldone);
        emit("addq $16, %rsp");
        pushDepth -= 2;
    }
    emit("addq $8, %rsp");
    pushDepth--;
    emit("movq " + to_string(exp->ptrTemp) + "(%rbp), %rax");
    return exp->inferredType;
}

Type Gencode::visit(DeleteExp* exp) {
    exp->exp->accept(this);
    Type pt = exp->pointeeType;
    bool dtor = needsDtor(pt);

    if (!dtor) {
        emit("movq %rax, %rdi");
        callRuntime("free");
        return exp->inferredType;
    }

    string ldone = newLabel("delend");
    emit("testq %rax, %rax");
    emit("je " + ldone);
    emit("pushq %rax");
    pushDepth++;
    if (!exp->isArray) {
        emit("movq %rax, %rdi");
        genOp(OP_DESTROY, pt);
        emit("popq %rdi");
        pushDepth--;
        callRuntime("free");
    } else if (exp->hasCookie) {
        string lloop = newLabel("delloop");
        string lfree = newLabel("delfree");
        int esz = objSize(pt);
        emit("movq -16(%rax), %rcx");
        emit("pushq %rcx");
        pushDepth++;
        label(lloop);
        emit("cmpq $0, (%rsp)");
        emit("je " + lfree);
        emit("decq (%rsp)");
        emit("movq (%rsp), %rax");
        emit("imulq $" + to_string(esz) + ", %rax");
        emit("movq 8(%rsp), %rdi");
        emit("addq %rax, %rdi");
        genOp(OP_DESTROY, pt);
        emit("jmp " + lloop);
        label(lfree);
        emit("addq $8, %rsp");
        pushDepth--;
        emit("popq %rdi");
        pushDepth--;
        emit("subq $16, %rdi");
        callRuntime("free");
    } else {
        emit("popq %rdi");
        pushDepth--;
        callRuntime("free");
    }
    label(ldone);
    return exp->inferredType;
}

// Operadores binarios

Type Gencode::visit(BinaryExp* exp) {
    switch (exp->mode) {
        case BM_COUT:
            genCout(exp);
            return exp->inferredType;
        case BM_CIN:
            genCin(exp);
            return exp->inferredType;
        case BM_STR_STR:
        case BM_STR_CSTR:
        case BM_CSTR_STR:
            genStrCompare(exp);
            return exp->inferredType;
        case BM_PTR_PLUS_INT:
        case BM_PTR_MINUS_INT: {
            exp->left->accept(this);
            pushTemp(Type(Type::VOID, 1, false));
            exp->right->accept(this);
            emit("movslq %eax, %rax");
            emit("imulq $" + to_string(exp->ptrScale) + ", %rax");
            emit("movq %rax, %rcx");
            emit("popq %rax");
            pushDepth--;
            emit(string(exp->mode == BM_PTR_PLUS_INT ? "addq" : "subq") + " %rcx, %rax");
            return exp->inferredType;
        }
        case BM_INT_PLUS_PTR: {
            exp->left->accept(this);
            emit("movslq %eax, %rax");
            emit("imulq $" + to_string(exp->ptrScale) + ", %rax");
            pushTemp(Type(Type::VOID, 1, false));
            exp->right->accept(this);
            emit("popq %rcx");
            pushDepth--;
            emit("addq %rcx, %rax");
            return exp->inferredType;
        }
        case BM_PTR_DIFF: {
            exp->left->accept(this);
            pushTemp(Type(Type::VOID, 1, false));
            exp->right->accept(this);
            emit("movq %rax, %rcx");
            emit("popq %rax");
            pushDepth--;
            emit("subq %rcx, %rax");
            emit("cqto");
            emit("movq $" + to_string(exp->ptrScale) + ", %rcx");
            emit("idivq %rcx");
            return exp->inferredType;
        }
        case BM_NORMAL:
            break;
    }

    switch (exp->op) {
        case AND_OP:
        case OR_OP:
            genLogic(exp);
            break;
        case EQ_OP:
        case NE_OP:
        case LT_OP:
        case LE_OP:
        case GT_OP:
        case GE_OP:
            genCompare(exp);
            break;
        default:
            genArith(exp);
            break;
    }
    return exp->inferredType;
}

void Gencode::genLogic(BinaryExp* e) {
    string lshort = newLabel("sc");
    string lend = newLabel("scend");
    bool isAnd = (e->op == AND_OP);

    genBool(e->left);
    emit("testl %eax, %eax");
    emit(string(isAnd ? "je " : "jne ") + lshort);
    genCond(e->right);
    emit("testl %eax, %eax");
    emit(string(isAnd ? "je " : "jne ") + lshort);
    emit(string("movl $") + (isAnd ? "1" : "0") + ", %eax");
    emit("jmp " + lend);
    label(lshort);
    emit(string("movl $") + (isAnd ? "0" : "1") + ", %eax");
    label(lend);
}

void Gencode::genCompare(BinaryExp* e) {
    Type ot = e->operandType;
    Cls c = cls(ot);

    Type lt = e->left->accept(this);
    convert(lt, ot);
    pushTemp(ot);
    Type rt = e->right->accept(this);
    convert(rt, ot);

    if (c == C_FLOAT || c == C_DOUBLE) {
        string ucom = (c == C_FLOAT) ? "ucomiss" : "ucomisd";
        emit("movaps %xmm0, %xmm1");
        popToResult(ot);
        switch (e->op) {
            case GT_OP:
                emit(ucom + " %xmm1, %xmm0");
                emit("seta %al");
                break;
            case GE_OP:
                emit(ucom + " %xmm1, %xmm0");
                emit("setae %al");
                break;
            case LT_OP:
                emit(ucom + " %xmm0, %xmm1");
                emit("seta %al");
                break;
            case LE_OP:
                emit(ucom + " %xmm0, %xmm1");
                emit("setae %al");
                break;
            case EQ_OP:
                emit(ucom + " %xmm1, %xmm0");
                emit("sete %al");
                emit("setnp %cl");
                emit("andb %cl, %al");
                break;
            default:
                emit(ucom + " %xmm1, %xmm0");
                emit("setne %al");
                emit("setp %cl");
                emit("orb %cl, %al");
                break;
        }
        emit("movzbl %al, %eax");
        return;
    }

    bool isPtr = (c == C_PTR);
    if (isPtr) {
        emit("movq %rax, %rcx");
        popToResult(ot);
        emit("cmpq %rcx, %rax");
    } else {
        emit("movl %eax, %ecx");
        popToResult(ot);
        emit("cmpl %ecx, %eax");
    }
    const char* cc = "e";
    switch (e->op) {
        case EQ_OP: cc = "e"; break;
        case NE_OP: cc = "ne"; break;
        case LT_OP: cc = isPtr ? "b" : "l"; break;
        case LE_OP: cc = isPtr ? "be" : "le"; break;
        case GT_OP: cc = isPtr ? "a" : "g"; break;
        case GE_OP: cc = isPtr ? "ae" : "ge"; break;
        default: break;
    }
    emit(string("set") + cc + " %al");
    emit("movzbl %al, %eax");
}

void Gencode::genArith(BinaryExp* e) {
    Type ot = e->operandType;
    Cls c = cls(ot);

    Type lt = e->left->accept(this);
    convert(lt, ot);
    pushTemp(ot);
    Type rt = e->right->accept(this);
    convert(rt, ot);

    if (c == C_FLOAT || c == C_DOUBLE) {
        string sfx = (c == C_FLOAT) ? "ss" : "sd";
        emit("movaps %xmm0, %xmm1");
        popToResult(ot);
        const char* op = "add";
        switch (e->op) {
            case PLUS_OP:  op = "add"; break;
            case MINUS_OP: op = "sub"; break;
            case MUL_OP:   op = "mul"; break;
            case DIV_OP:   op = "div"; break;
            default: throw logic_error("Gencode: operador invalido para flotantes");
        }
        emit(string(op) + sfx + " %xmm1, %xmm0");
        return;
    }

    emit("movl %eax, %ecx");
    popToResult(ot);
    switch (e->op) {
        case PLUS_OP:    emit("addl %ecx, %eax"); break;
        case MINUS_OP:   emit("subl %ecx, %eax"); break;
        case MUL_OP:     emit("imull %ecx, %eax"); break;
        case DIV_OP:
            emit("cltd");
            emit("idivl %ecx");
            break;
        case MOD_OP:
            emit("cltd");
            emit("idivl %ecx");
            emit("movl %edx, %eax");
            break;
        case BIT_AND_OP: emit("andl %ecx, %eax"); break;
        case BIT_OR_OP:  emit("orl %ecx, %eax"); break;
        case BIT_XOR_OP: emit("xorl %ecx, %eax"); break;
        case LSHIFT_OP:  emit("sall %cl, %eax"); break;
        case RSHIFT_OP:  emit("sarl %cl, %eax"); break;
        default: throw logic_error("Gencode: operador binario no soportado");
    }
}

void Gencode::genStrCompare(BinaryExp* e) {
    e->left->accept(this);
    pushTemp(Type(Type::VOID, 1, false));
    e->right->accept(this);
    switch (e->mode) {
        case BM_STR_STR:
            emit("movq %rax, %rsi");
            emit("popq %rdi");
            pushDepth--;
            callRuntime("rt_str_eq");
            break;
        case BM_STR_CSTR:
            emit("movq %rax, %rsi");
            emit("popq %rdi");
            pushDepth--;
            callRuntime("rt_str_eq_cstr");
            break;
        default: // cstr == string
            emit("movq %rax, %rdi");
            emit("popq %rsi");
            pushDepth--;
            callRuntime("rt_str_eq_cstr");
            break;
    }
    if (e->op == NE_OP) emit("xorl $1, %eax");
}

void Gencode::genCout(BinaryExp* e) {
    e->left->accept(this);

    Exp* r = e->right;
    if (r->nodeKind() == NodeKind::ID_EXP && static_cast<IdExp*>(r)->idKind == ID_ENDL) {
        callRuntime("rt_print_endl");
        return;
    }

    r->accept(this);
    Type t = e->operandType;
    switch (cls(t)) {
        case C_INT:
            emit("movl %eax, %edi");
            if (t.isBool()) callRuntime("rt_print_bool");
            else if (t.isChar()) callRuntime("rt_print_char");
            else callRuntime("rt_print_int");
            break;
        case C_FLOAT:
            emit("cvtss2sd %xmm0, %xmm0");
            callRuntime("rt_print_double");
            break;
        case C_DOUBLE:
            callRuntime("rt_print_double");
            break;
        case C_PTR:
            emit("movq %rax, %rdi");
            callRuntime("rt_print_cstr");
            break;
        case C_AGG:
            emit("movq %rax, %rdi");
            callRuntime("rt_print_str");
            break;
        default:
            throw logic_error("Gencode: tipo no imprimible");
    }
}

void Gencode::genCin(BinaryExp* e) {
    e->left->accept(this);
    genAddr(e->right);
    emit("movq %rax, %rdi");
    Type t = e->operandType;
    if (t.isString()) callRuntime("rt_read_string");
    else if (t.isChar()) callRuntime("rt_read_char");
    else if (t.isFloat()) callRuntime("rt_read_float");
    else if (t.isDouble()) callRuntime("rt_read_double");
    else callRuntime("rt_read_int");
}

// Postfijos y llamadas

Type Gencode::visit(PostfixExp* exp) {
    switch (exp->kind) {
        case POST_INC:
            genIncDec(exp->base, true, true, exp->ptrScale);
            break;
        case POST_DEC:
            genIncDec(exp->base, false, true, exp->ptrScale);
            break;
        case ARRAY_ACCESS:
        case DOT_ACCESS:
        case ARROW_ACCESS:
            genAddr(exp);
            loadFrom(exp->inferredType, "(%rax)");
            break;
        case CALL_ACCESS:
            genCallExp(exp);
            if (exp->callKind != CALL_CTOR && exp->callee && exp->callee->layout.returnType.isRef) {
                loadFrom(exp->inferredType, "(%rax)");
            }
            break;
    }
    return exp->inferredType;
}

static bool byValueClass(const Type& t) {
    return (t.kind == Type::RECORD || t.kind == Type::STRING) && t.ptrLevel == 0 && !t.isRef && t.arrayDims.empty();
}

void Gencode::storeReturnedRegs(const AbiLoc& ra, int tempOffset) {
    for (int k = 0; k < ra.neight; k++) {
        int n = ra.size - 8 * k;
        if (n > 8) n = 8;
        string base = "%rbp";
        int off = tempOffset + 8 * k;
        if (ra.cls[k] == AbiClass::SSE) {
            string x = "%xmm" + to_string(ra.reg[k]);
            emit(string(n == 4 ? "movss " : "movsd ") + x + ", " + to_string(off) + "(%rbp)");
        } else {
            storePartial(ra.reg[k] == 0 ? "%rax" : "%rdx", base, off, n);
        }
    }
}

// T(args): construye un temporal del marco
void Gencode::genConstructTemp(PostfixExp* call) {
    Type rec = call->inferredType;
    Place pl = frameAt(call->retTemp);
    switch (call->ctorKind) {
        case IK_COPY:
            genObjAddr(call->args[0]);
            emit("movq %rax, %rsi");
            loadAddr(pl, "%rdi");
            genOp(OP_COPY, rec);
            break;
        case IK_CTOR: {
            vector<ArgSpec> args;
            const FunctionLayout& L = call->callee->layout;
            ArgSpec th;
            th.kind = AK_PLACE;
            th.place = pl;
            th.abi = &L.paramAbi[0];
            args.push_back(th);
            for (size_t i = 0; i < call->args.size(); i++) {
                ArgSpec a;
                a.kind = AK_VALUE;
                a.exp = call->args[i];
                a.info = call->argInfo[i];
                a.abi = &L.paramAbi[i + 1];
                args.push_back(a);
            }
            genCallWith(args, L.stackArgBytes, L.label);
            break;
        }
        default:
            // T(): inicializacion por valor, pone a cero y luego construye
            if (!call->callee || call->callee->isImplicit) zeroFill(pl, objSize(rec));
            if (call->callee) {
                loadAddr(pl, "%rdi");
                emitCall(call->callee->layout.label);
            }
            break;
    }
    registerTemp(call->retTemp, rec);
    emit("leaq " + to_string(call->retTemp) + "(%rbp), %rax");
}

void Gencode::genCallExp(PostfixExp* call) {
    if (call->callKind == CALL_BUILTIN) {
        genBuiltin(call);
        return;
    }
    if (call->callKind == CALL_CTOR) {
        genConstructTemp(call);
        return;
    }

    FunDecl* fd = call->callee;
    const FunctionLayout& L = fd->layout;
    vector<ArgSpec> args;
    size_t off = 0;

    static AbiLoc hiddenAbi;
    hiddenAbi.pass = Pass::INT_REG;
    hiddenAbi.reg[0] = 0;
    if (L.retAbi.pass == Pass::HIDDEN_PTR) {
        ArgSpec h;
        h.kind = AK_TEMP_ADDR;
        h.tempOffset = call->retTemp;
        h.abi = &hiddenAbi;
        args.push_back(h);
    }
    if (call->callKind == CALL_METHOD) {
        PostfixExp* pf = static_cast<PostfixExp*>(call->base);
        ArgSpec th;
        th.kind = (pf->kind == ARROW_ACCESS) ? AK_THIS_PTR : AK_THIS_ADDR;
        th.exp = pf->base;
        th.abi = &L.paramAbi[0];
        args.push_back(th);
        off = 1;
    } else if (call->callKind == CALL_METHOD_IMPLICIT) {
        ArgSpec th;
        th.kind = AK_THIS_IMPLICIT;
        th.abi = &L.paramAbi[0];
        args.push_back(th);
        off = 1;
    }
    for (size_t i = 0; i < call->args.size(); i++) {
        ArgSpec a;
        a.kind = AK_VALUE;
        a.exp = call->args[i];
        a.info = call->argInfo[i];
        a.abi = &L.paramAbi[i + off];
        args.push_back(a);
    }
    genCallWith(args, L.stackArgBytes, L.label);

    const Type& rt = L.returnType;
    if (!rt.isRef && byValueClass(rt)) {
        if (L.retAbi.pass == Pass::AGG_REGS) storeReturnedRegs(L.retAbi, call->retTemp);
        registerTemp(call->retTemp, rt);
        emit("leaq " + to_string(call->retTemp) + "(%rbp), %rax");
    }
}

// Evalua los argumentos en orden y los deja como palabras de 8 bytes en la pila,
// reserva el area de argumentos en pila (alineada) y carga los registros segun la
// clasificacion ABI que dejo el TypeChecker.
void Gencode::genCallWith(vector<ArgSpec>& args, int stackArgBytes, const string& target) {
    int n = static_cast<int>(args.size());
    vector<int> firstG(n, 0), nwords(n, 0);
    int W = 0;

    for (int i = 0; i < n; i++) {
        ArgSpec& a = args[i];
        const AbiLoc& abi = *a.abi;
        int k = 1;
        bool floatVal = false;
        Type ft;
        bool agg = false;

        switch (a.kind) {
            case AK_THIS_ADDR:
                genObjAddr(a.exp);
                break;
            case AK_THIS_PTR:
                a.exp->accept(this);
                break;
            case AK_THIS_IMPLICIT:
                emit("movq " + to_string(thisOffset()) + "(%rbp), %rax");
                break;
            case AK_PLACE:
                loadAddr(a.place, "%rax");
                break;
            case AK_TEMP_ADDR:
                emit("leaq " + to_string(a.tempOffset) + "(%rbp), %rax");
                break;
            case AK_VALUE: {
                const ArgInfo& ai = a.info;
                switch (ai.mode) {
                    case AM_VALUE: {
                        Type vt = a.exp->accept(this);
                        convert(vt, ai.ptype);
                        Cls c = cls(ai.ptype);
                        if (c == C_FLOAT || c == C_DOUBLE) {
                            floatVal = true;
                            ft = ai.ptype;
                        }
                        break;
                    }
                    case AM_REF:
                        genObjAddr(a.exp);
                        break;
                    case AM_REF_TEMP: {
                        Type tb = ai.ptype.withoutRef().withoutConst();
                        if (tb.isString()) {
                            a.exp->accept(this);
                            emit("movq %rax, %rsi");
                            emit("leaq " + to_string(ai.tempOffset) + "(%rbp), %rdi");
                            callRuntime("rt_str_init_cstr");
                            registerTemp(ai.tempOffset, tb);
                        } else {
                            Type vt = a.exp->accept(this);
                            convert(vt, tb);
                            storeTo(tb, to_string(ai.tempOffset) + "(%rbp)");
                        }
                        emit("leaq " + to_string(ai.tempOffset) + "(%rbp), %rax");
                        break;
                    }
                    case AM_COPY: {
                        Type tt = ai.ptype.withoutRef().withoutConst();
                        if (tt.isString() && !a.exp->inferredType.withoutRef().isString()) {
                            a.exp->accept(this);
                            emit("movq %rax, %rsi");
                            emit("leaq " + to_string(ai.tempOffset) + "(%rbp), %rdi");
                            callRuntime("rt_str_init_cstr");
                        } else {
                            genObjAddr(a.exp);
                            emit("movq %rax, %rsi");
                            emit("leaq " + to_string(ai.tempOffset) + "(%rbp), %rdi");
                            genOp(OP_COPY, tt);
                        }
                        registerTemp(ai.tempOffset, tt);
                        emit("leaq " + to_string(ai.tempOffset) + "(%rbp), %rax");
                        break;
                    }
                    case AM_AGG:
                        genObjAddr(a.exp);
                        agg = true;
                        break;
                }
                break;
            }
        }

        if (agg) {
            if (abi.pass == Pass::AGG_REGS) k = abi.neight;
            else if (abi.pass == Pass::STACK) k = abi.stackBytes / 8;
            else k = 0;
            for (int j = k - 1; j >= 0; j--) {
                int nb = abi.size - 8 * j;
                if (nb > 8) nb = 8;
                loadPartial("%rax", 8 * j, nb);
                emit("pushq %rcx");
                pushDepth++;
            }
        } else if (floatVal) {
            pushTemp(ft);
        } else {
            emit("pushq %rax");
            pushDepth++;
        }
        firstG[i] = W;
        nwords[i] = k;
        W += k;
    }

    int pad = ((pushDepth + stackArgBytes / 8) % 2 != 0) ? 8 : 0;
    int S = stackArgBytes + pad;
    if (S > 0) {
        emit("subq $" + to_string(S) + ", %rsp");
        pushDepth += S / 8;
    }

    auto wordAddr = [&](int i, int j) {
        int g = firstG[i] + (nwords[i] - 1 - j);
        return to_string(S + 8 * (W - 1 - g)) + "(%rsp)";
    };

    for (int i = 0; i < n; i++) {
        const AbiLoc& abi = *args[i].abi;
        bool onStack = (abi.pass == Pass::STACK) || (abi.pass == Pass::INVISIBLE_REF && abi.reg[0] < 0);
        if (onStack) {
            for (int j = 0; j < nwords[i]; j++) {
                emit("movq " + wordAddr(i, j) + ", %r10");
                emit("movq %r10, " + to_string(abi.stackOffset + 8 * j) + "(%rsp)");
            }
        }
    }
    for (int i = 0; i < n; i++) {
        const AbiLoc& abi = *args[i].abi;
        switch (abi.pass) {
            case Pass::INT_REG:
            case Pass::INVISIBLE_REF:
                if (abi.reg[0] >= 0) emit("movq " + wordAddr(i, 0) + ", " + R64[abi.reg[0]]);
                break;
            case Pass::SSE_REG:
                emit("movsd " + wordAddr(i, 0) + ", %xmm" + to_string(abi.reg[0]));
                break;
            case Pass::AGG_REGS:
                for (int j = 0; j < abi.neight; j++) {
                    if (abi.cls[j] == AbiClass::SSE) emit("movsd " + wordAddr(i, j) + ", %xmm" + to_string(abi.reg[j]));
                    else emit("movq " + wordAddr(i, j) + ", " + R64[abi.reg[j]]);
                }
                break;
            default:
                break;
        }
    }

    emitCall(target);

    emit("addq $" + to_string(S + 8 * W) + ", %rsp");
    pushDepth -= W + S / 8;
}

void Gencode::genRuntimeCall(const string& fn, vector<ArgSpec>& args) {
    vector<AbiLoc> abis(args.size());
    for (size_t i = 0; i < args.size(); i++) {
        abis[i].pass = Pass::INT_REG;
        abis[i].reg[0] = static_cast<int>(i);
        args[i].abi = &abis[i];
    }
    genCallWith(args, 0, fn + "@PLT");
}

void Gencode::genBuiltin(PostfixExp* call) {
    if (call->builtin == BI_SQRT) {
        Type t = call->args[0]->accept(this);
        convert(t, Type(Type::DOUBLE, 0, false));
        emit("sqrtsd %xmm0, %xmm0");
        return;
    }

    PostfixExp* pf = static_cast<PostfixExp*>(call->base);
    vector<ArgSpec> args;
    ArgSpec th;
    th.kind = (pf->kind == ARROW_ACCESS) ? AK_THIS_PTR : AK_THIS_ADDR;
    th.exp = pf->base;
    args.push_back(th);

    auto valueArg = [&](const Type& pt) {
        ArgSpec a;
        a.kind = AK_VALUE;
        a.exp = call->args[0];
        a.info.mode = AM_VALUE;
        a.info.ptype = pt;
        args.push_back(a);
    };

    string fn;
    switch (call->builtin) {
        case BI_STR_SIZE:  fn = "rt_str_size"; break;
        case BI_STR_EMPTY: fn = "rt_str_empty"; break;
        case BI_STR_CLEAR: fn = "rt_str_clear"; break;
        case BI_STR_C_STR: fn = "rt_str_c_str"; break;
        case BI_STR_APPEND_STR:
            fn = "rt_str_append_str";
            valueArg(Type(Type::CHAR, 1, false));
            break;
        case BI_STR_APPEND_CSTR:
            fn = "rt_str_append_cstr";
            valueArg(Type(Type::CHAR, 1, false));
            break;
        case BI_STR_APPEND_CHAR:
            fn = "rt_str_append_char";
            valueArg(Type(Type::CHAR, 0, false));
            break;
        default:
            throw logic_error("Gencode: builtin desconocido");
    }
    genRuntimeCall(fn, args);
}

// Sentencias

void Gencode::visit(BlockStmt* stm) {
    pushScope();
    for (ASTNode* item : stm->items) {
        item->accept(this);
    }
    emitScopeCleanup(scopes.size() - 1, nullptr);
    popScope();
}

void Gencode::visit(ExprStmt* stm) {
    size_t mark = tempStack.size();
    if (stm->expr) stm->expr->accept(this);
    flushTemps(mark, nullptr);
    if (pushDepth != 0) throw logic_error("Gencode: temporales sin liberar tras una sentencia");
}

void Gencode::visit(IfStmt* stm) {
    string lelse = newLabel("else");
    genCond(stm->condition);
    emit("testl %eax, %eax");
    emit("je " + lelse);
    stm->thenStmt->accept(this);
    if (stm->elseStmt) {
        string lend = newLabel("endif");
        emit("jmp " + lend);
        label(lelse);
        stm->elseStmt->accept(this);
        label(lend);
    } else {
        label(lelse);
    }
}

void Gencode::visit(WhileStmt* stm) {
    string lcond = newLabel("while");
    string lend = newLabel("endwhile");
    label(lcond);
    genCond(stm->condition);
    emit("testl %eax, %eax");
    emit("je " + lend);
    loops.push_back({lend, lcond, scopes.size()});
    stm->body->accept(this);
    loops.pop_back();
    emit("jmp " + lcond);
    label(lend);
}

void Gencode::visit(ForStmt* stm) {
    string lcond = newLabel("for");
    string lupd = newLabel("forupd");
    string lend = newLabel("endfor");

    pushScope();
    if (stm->initDecl) stm->initDecl->accept(this);
    for (Exp* e : stm->initExprs) {
        size_t m = tempStack.size();
        e->accept(this);
        flushTemps(m, nullptr);
    }

    label(lcond);
    if (stm->condition) {
        genCond(stm->condition);
        emit("testl %eax, %eax");
        emit("je " + lend);
    }
    loops.push_back({lend, lupd, scopes.size()});
    stm->body->accept(this);
    loops.pop_back();

    label(lupd);
    for (Exp* e : stm->updateExprs) {
        size_t m = tempStack.size();
        e->accept(this);
        flushTemps(m, nullptr);
    }
    emit("jmp " + lcond);
    label(lend);
    emitScopeCleanup(scopes.size() - 1, nullptr);
    popScope();
}

void Gencode::visit(DoWhileStmt* stm) {
    string lbody = newLabel("do");
    string lcond = newLabel("docond");
    string lend = newLabel("enddo");
    label(lbody);
    loops.push_back({lend, lcond, scopes.size()});
    stm->body->accept(this);
    loops.pop_back();
    label(lcond);
    genCond(stm->condition);
    emit("testl %eax, %eax");
    emit("jne " + lbody);
    label(lend);
}

void Gencode::visit(BreakStmt* /*stm*/) {
    emitScopeCleanup(loops.back().scopeDepth, nullptr);
    emit("jmp " + loops.back().brk);
}

void Gencode::visit(ContinueStmt* /*stm*/) {
    emitScopeCleanup(loops.back().scopeDepth, nullptr);
    emit("jmp " + loops.back().cont);
}

void Gencode::visit(ReturnStmt* stm) {
    const FunctionLayout& L = curFun->layout;
    Type rt = L.returnType;
    size_t mark = tempStack.size();
    Type live;
    bool hasLive = false;

    switch (L.retAbi.pass) {
        case Pass::VOID_RET:
            if (stm->expr) stm->expr->accept(this);
            break;
        case Pass::HIDDEN_PTR:
            genObjAddr(stm->expr);
            emit("movq %rax, %rsi");
            emit("movq " + to_string(L.retPtrOffset) + "(%rbp), %rdi");
            genOp(OP_COPY, rt);
            break;
        case Pass::AGG_REGS:
            genObjAddr(stm->expr);
            emit("movq %rax, %rsi");
            emit("leaq " + to_string(L.retValOffset) + "(%rbp), %rdi");
            genOp(OP_COPY, rt);
            break;
        case Pass::EMPTY:
            genObjAddr(stm->expr);
            break;
        default:
            if (rt.isRef) {
                genAddr(stm->expr);
                live = Type(Type::VOID, 1, false);
            } else {
                Type et = stm->expr->accept(this);
                convert(et, rt);
                live = rt;
            }
            hasLive = true;
            break;
    }
    flushTemps(mark, hasLive ? &live : nullptr);
    emitScopeCleanup(0, hasLive ? &live : nullptr);
    emit("jmp " + endLabel);
}

// Declaraciones

void Gencode::visit(VarInit* /*init*/) {}
void Gencode::visit(Param* /*param*/) {}
void Gencode::visit(RecordMember* /*member*/) {}
void Gencode::visit(IncludeDirective* /*inc*/) {}
void Gencode::visit(UsingDirective* /*usg*/) {}

void Gencode::visit(VarDecl* decl) {
    for (VarInit* vi : decl->inits) {
        genVarInit(vi);
    }
}

void Gencode::genVarInit(VarInit* vi) {
    const VarInfo& info = vi->info;
    Place pl = placeOf(info);
    size_t mark = tempStack.size();
    genInitObject(info.type, vi->spec, pl);
    if (vi->spec.kind == IK_REF) moveTempsToScope(mark);
    else flushTemps(mark, nullptr);
    if (needsDtor(info.type)) scopes.back().push_back({pl, info.type});
    if (pushDepth != 0) throw logic_error("Gencode: temporales sin liberar tras una declaracion");
}

void Gencode::genGlobals(VarDecl* decl) {
    for (VarInit* vi : decl->inits) {
        globals.push_back(vi->info);
        Place pl = placeOf(vi->info);
        if (vi->spec.kind != IK_NONE) {
            cur = &initCode;
            curFun = nullptr;
            pushDepth = 0;
            size_t mark = tempStack.size();
            genInitObject(vi->info.type, vi->spec, pl);
            if (vi->spec.kind == IK_REF) moveTempsToScope(mark);
            else flushTemps(mark, nullptr);
            if (pushDepth != 0) throw logic_error("Gencode: temporales sin liberar en un inicializador global");
            cur = &text;
        }
        if (needsDtor(vi->info.type)) globalDtors.push_back({pl, vi->info.type});
    }
}

void Gencode::visit(FunDecl* fun) {
    genFunction(fun, nullptr);
}

void Gencode::visit(RecordDecl* record) {
    for (RecordMember* m : record->members) {
        if (m->funDecl) genFunction(m->funDecl, record);
    }
    for (FunDecl* f : record->implicitFuns) {
        genFunction(f, record);
    }
}

// Guarda en su slot cada parametro que llega en registros
void Gencode::genParamSpill(FunDecl* fun) {
    const FunctionLayout& L = fun->layout;
    for (size_t i = 0; i < L.params.size(); i++) {
        const VarInfo& p = L.params[i];
        const AbiLoc& a = L.paramAbi[i];
        string slot = to_string(p.offset) + "(%rbp)";
        switch (a.pass) {
            case Pass::INT_REG: {
                int sz = accessSize(p.type);
                if (sz == 8) emit(string("movq ") + R64[a.reg[0]] + ", " + slot);
                else if (sz == 4) emit(string("movl ") + R32[a.reg[0]] + ", " + slot);
                else emit(string("movb ") + R8[a.reg[0]] + ", " + slot);
                break;
            }
            case Pass::SSE_REG:
                emit(string(p.type.kind == Type::FLOAT ? "movss" : "movsd") + " %xmm" + to_string(a.reg[0]) + ", " + slot);
                break;
            case Pass::AGG_REGS:
                for (int k = 0; k < a.neight; k++) {
                    int nb = a.size - 8 * k;
                    if (nb > 8) nb = 8;
                    if (a.cls[k] == AbiClass::SSE) {
                        emit(string(nb == 4 ? "movss" : "movsd") + " %xmm" + to_string(a.reg[k]) + ", " +
                             to_string(p.offset + 8 * k) + "(%rbp)");
                    } else {
                        storePartial(R64[a.reg[k]], "%rbp", p.offset + 8 * k, nb);
                    }
                }
                break;
            case Pass::INVISIBLE_REF:
                if (a.reg[0] >= 0) {
                    emit(string("movq ") + R64[a.reg[0]] + ", " + slot);
                } else {
                    emit("movq " + to_string(16 + a.stackOffset) + "(%rbp), %r10");
                    emit("movq %r10, " + slot);
                }
                break;
            default:
                break;
        }
    }
}

// Inicializa los campos de this en orden de declaracion antes del cuerpo del constructor
void Gencode::genFieldInits(FunDecl* fun, RecordDecl* owner) {
    const RecordLayout& L = owner->layout;
    for (const FieldInit& fi : fun->fieldInits) {
        const FieldInfo& f = L.fields[fi.fieldIndex];
        Place dst = thisPlace(f.offset);
        size_t mark = tempStack.size();
        if (fi.spec.kind == IK_COPY && !fi.spec.expr) {
            Place src;
            src.kind = Place::SLOT_PTR;
            src.off = fun->layout.params[1].offset;
            src.extra = f.offset;
            loadAddr(src, "%rsi");
            loadAddr(dst, "%rdi");
            genOp(OP_COPY, f.type);
        } else {
            genInitObject(f.type, fi.spec, dst);
        }
        if (fi.spec.kind == IK_REF) moveTempsToScope(mark);
        else flushTemps(mark, nullptr);
    }
}

void Gencode::genImplicitAssign(FunDecl* fun, RecordDecl* owner) {
    const RecordLayout& L = owner->layout;
    for (const FieldInfo& f : L.fields) {
        Place src;
        src.kind = Place::SLOT_PTR;
        src.off = fun->layout.params[1].offset;
        src.extra = f.offset;
        loadAddr(src, "%rsi");
        loadAddr(thisPlace(f.offset), "%rdi");
        genOp(OP_ASSIGN, f.type);
    }
    emit("movq " + to_string(thisOffset()) + "(%rbp), %rax");
}

void Gencode::genFieldDtors(RecordDecl* owner) {
    const RecordLayout& L = owner->layout;
    for (size_t i = L.fields.size(); i > 0; i--) {
        const FieldInfo& f = L.fields[i - 1];
        if (!needsDtor(f.type)) continue;
        loadAddr(thisPlace(f.offset), "%rdi");
        genOp(OP_DESTROY, f.type);
    }
}

void Gencode::genFunction(FunDecl* fun, RecordDecl* owner) {
    const FunctionLayout& L = fun->layout;
    curFun = fun;
    cur = &text;
    pushDepth = 0;
    endLabel = newLabel("ret");
    scopes.clear();
    tempStack.clear();
    loops.clear();
    bool isMain = (L.label == "main");

    if (isMain) *cur << "\t.globl main\n";
    *cur << "\t.type " << L.label << ", @function\n";
    label(L.label);
    emit("pushq %rbp");
    emit("movq %rsp, %rbp");
    emit("subq $" + to_string(L.frameSize) + ", %rsp");

    if (L.retAbi.pass == Pass::HIDDEN_PTR) {
        emit("movq %rdi, " + to_string(L.retPtrOffset) + "(%rbp)");
    }
    genParamSpill(fun);

    if (isMain) emit("call __init_globals");

    pushScope();
    if (fun->funKind == FunKind::CTOR && owner) genFieldInits(fun, owner);
    if (fun->funKind == FunKind::COPY_ASSIGN && fun->isImplicit && owner) genImplicitAssign(fun, owner);

    if (fun->body) {
        for (ASTNode* item : fun->body->items) {
            item->accept(this);
        }
    }
    emitScopeCleanup(0, nullptr);
    popScope();

    if (isMain) emit("movl $0, %eax");

    label(endLabel);
    if (fun->funKind == FunKind::DTOR && owner) genFieldDtors(owner);
    if (isMain) {
        emit("pushq %rax");
        pushDepth++;
        emitCall("__fini_globals");
        emit("popq %rax");
        pushDepth--;
    }
    switch (L.retAbi.pass) {
        case Pass::HIDDEN_PTR:
            emit("movq " + to_string(L.retPtrOffset) + "(%rbp), %rax");
            break;
        case Pass::AGG_REGS:
            for (int k = 0; k < L.retAbi.neight; k++) {
                string mem = to_string(L.retValOffset + 8 * k) + "(%rbp)";
                if (L.retAbi.cls[k] == AbiClass::SSE) emit("movsd " + mem + ", %xmm" + to_string(L.retAbi.reg[k]));
                else emit("movq " + mem + ", " + (L.retAbi.reg[k] == 0 ? "%rax" : "%rdx"));
            }
            break;
        default:
            break;
    }
    emit("leave");
    emit("ret");

    curFun = nullptr;
}

void Gencode::visit(Program* prog) {
    program = prog;
    *cur << "\t.text\n";

    for (ASTNode* decl : prog->declarations) {
        if (decl->nodeKind() == NodeKind::VAR_DECL) {
            genGlobals(static_cast<VarDecl*>(decl));
        } else {
            decl->accept(this);
        }
    }

    cur = &text;
    pushDepth = 0;
    *cur << "\t.type __init_globals, @function\n";
    label("__init_globals");
    emit("pushq %rbp");
    emit("movq %rsp, %rbp");
    emit("subq $" + to_string(prog->initFrameSize) + ", %rsp");
    *cur << initCode.str();
    emit("leave");
    emit("ret");

    *cur << "\t.type __fini_globals, @function\n";
    label("__fini_globals");
    emit("pushq %rbp");
    emit("movq %rsp, %rbp");
    scopes.clear();
    emitCleanupItems(globalDtors);
    emit("leave");
    emit("ret");

    for (const VarInfo& g : globals) {
        bss << "\t.balign " << g.align << "\n" << g.label << ":\n\t.zero " << (g.size > 0 ? g.size : 1) << "\n";
    }
}
