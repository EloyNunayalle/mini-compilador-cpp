#include <cstdio>
#include <stdexcept>
#include "gencode.h"

using namespace std;

static const char* R64[6] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static const char* R32[6] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static const char* R8[6]  = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};

static Type ptrType() { return Type(Type::VOID, 1, false); }
static Type boolType() { return Type(Type::BOOL, 0, false); }

Gencode::Gencode(const unordered_map<string, RecordDecl*>& recs)
    : records(recs), cur(nullptr), curFun(nullptr), pushDepth(0), labelCount(0), constCount(0) {}

void Gencode::generate(Program* prog, ostream& out) {
    cur = &text;
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

// Direcciones (lvalues)

void Gencode::genAddr(Exp* e) {
    switch (e->nodeKind()) {
        case NodeKind::ID_EXP: {
            IdExp* id = static_cast<IdExp*>(e);
            if (id->idKind == ID_FIELD) {
                emit("movq " + to_string(thisOffset()) + "(%rbp), %rax");
                emit("addq $" + to_string(id->var.offset) + ", %rax");
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
        case NodeKind::POSTFIX_EXP: {
            PostfixExp* pf = static_cast<PostfixExp*>(e);
            switch (pf->kind) {
                case ARRAY_ACCESS: {
                    if (pf->base->inferredType.isString()) {
                        genAddr(pf->base);
                        emit("movq (%rax), %rax");
                    } else {
                        pf->base->accept(this);
                    }
                    pushTemp(ptrType());
                    pf->indexExp->accept(this);
                    emit("movslq %eax, %rax");
                    emit("imulq $" + to_string(pf->elemSize) + ", %rax");
                    emit("popq %rcx");
                    pushDepth--;
                    emit("addq %rcx, %rax");
                    return;
                }
                case DOT_ACCESS:
                    genAddr(pf->base);
                    emit("addq $" + to_string(pf->fieldOffset) + ", %rax");
                    return;
                case ARROW_ACCESS:
                    pf->base->accept(this);
                    emit("addq $" + to_string(pf->fieldOffset) + ", %rax");
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
    convert(t, boolType());
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
        exp->value->accept(this);
        pushTemp(ptrType());
        genAddr(exp->target);
        emit("movq %rax, %rcx");
        emit("popq %rdx");
        pushDepth--;
        copyBytes("%rdx", "%rcx", records.at(tt.recordName)->layout.size);
        emit("movq %rcx, %rax");
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
    pushTemp(ptrType());
    Type vt = e->value->accept(this);
    emit("movq %rax, %rsi");
    emit("movq (%rsp), %rdi");
    callRuntime(vt.isString() ? "rt_str_assign" : "rt_str_assign_cstr");
    emit("popq %rax");
    pushDepth--;
}

Type Gencode::visit(NewExp* exp) {
    if (exp->arrayDims.empty()) {
        emit("movl $" + to_string(exp->elemSize) + ", %edi");
    } else {
        exp->arrayDims[0]->accept(this);
        emit("movslq %eax, %rax");
        emit("imulq $" + to_string(exp->elemSize) + ", %rax");
        emit("movq %rax, %rdi");
    }
    callRuntime("malloc");
    return exp->inferredType;
}

Type Gencode::visit(DeleteExp* exp) {
    exp->exp->accept(this);
    emit("movq %rax, %rdi");
    callRuntime("free");
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
            pushTemp(ptrType());
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
            pushTemp(ptrType());
            exp->right->accept(this);
            emit("popq %rcx");
            pushDepth--;
            emit("addq %rcx, %rax");
            return exp->inferredType;
        }
        case BM_PTR_DIFF: {
            exp->left->accept(this);
            pushTemp(ptrType());
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
    genBool(e->right);
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
    pushTemp(ptrType());
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
            if (exp->callee && exp->callee->layout.returnType.isRef) {
                loadFrom(exp->inferredType, "(%rax)");
            }
            break;
    }
    return exp->inferredType;
}

void Gencode::genCallExp(PostfixExp* call) {
    if (call->callKind == CALL_BUILTIN) {
        genBuiltin(call);
        return;
    }

    FunDecl* fd = call->callee;
    vector<ArgSpec> args;
    size_t off = 0;

    if (call->callKind == CALL_METHOD) {
        PostfixExp* pf = static_cast<PostfixExp*>(call->base);
        args.push_back({pf->kind == ARROW_ACCESS ? AK_THIS_PTR : AK_THIS_ADDR, pf->base, ptrType(), false});
        off = 1;
    } else if (call->callKind == CALL_METHOD_IMPLICIT) {
        args.push_back({AK_THIS_IMPLICIT, nullptr, ptrType(), false});
        off = 1;
    }
    for (size_t i = 0; i < call->args.size(); i++) {
        Type pt = fd->layout.params[i + off].type;
        args.push_back({AK_VALUE, call->args[i], pt, pt.isRef});
    }
    genCallArgs(args, fd->layout.label);
}

// Evalua los argumentos en orden como temporales en la pila, reserva el area de
// argumentos en pila (alineada) y carga los registros segun System V AMD64.
void Gencode::genCallArgs(vector<ArgSpec>& args, const string& target) {
    int n = static_cast<int>(args.size());

    for (ArgSpec& a : args) {
        switch (a.kind) {
            case AK_THIS_ADDR:
                genAddr(a.exp);
                pushTemp(ptrType());
                break;
            case AK_THIS_PTR:
                a.exp->accept(this);
                pushTemp(ptrType());
                break;
            case AK_THIS_IMPLICIT:
                emit("movq " + to_string(thisOffset()) + "(%rbp), %rax");
                pushTemp(ptrType());
                break;
            case AK_VALUE:
                if (a.byAddr) {
                    genAddr(a.exp);
                    pushTemp(ptrType());
                } else {
                    Type vt = a.exp->accept(this);
                    convert(vt, a.ptype);
                    pushTemp(a.ptype);
                }
                break;
        }
    }

    vector<int> reg(n, -1);
    vector<bool> flt(n, false);
    int ni = 0, nf = 0, nstack = 0;
    for (int i = 0; i < n; i++) {
        const ArgSpec& a = args[i];
        Cls c = cls(a.ptype);
        flt[i] = (a.kind == AK_VALUE && !a.byAddr && (c == C_FLOAT || c == C_DOUBLE));
        if (flt[i]) {
            if (nf < 8) reg[i] = nf++;
            else nstack++;
        } else {
            if (ni < 6) reg[i] = ni++;
            else nstack++;
        }
    }

    int pad = ((pushDepth + nstack) % 2 != 0) ? 8 : 0;
    int S = 8 * nstack + pad;
    if (S > 0) {
        emit("subq $" + to_string(S) + ", %rsp");
        pushDepth += S / 8;
    }

    int j = 0;
    for (int i = 0; i < n; i++) {
        if (reg[i] >= 0) continue;
        emit("movq " + to_string(S + 8 * (n - 1 - i)) + "(%rsp), %r10");
        emit("movq %r10, " + to_string(8 * j) + "(%rsp)");
        j++;
    }
    for (int i = 0; i < n; i++) {
        if (reg[i] < 0) continue;
        string src = to_string(S + 8 * (n - 1 - i)) + "(%rsp)";
        if (flt[i]) emit("movsd " + src + ", %xmm" + to_string(reg[i]));
        else emit("movq " + src + ", " + R64[reg[i]]);
    }

    emitCall(target);

    emit("addq $" + to_string(S + 8 * n) + ", %rsp");
    pushDepth -= n + S / 8;
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
    args.push_back({pf->kind == ARROW_ACCESS ? AK_THIS_PTR : AK_THIS_ADDR, pf->base, ptrType(), false});

    string fn;
    switch (call->builtin) {
        case BI_STR_SIZE:  fn = "rt_str_size"; break;
        case BI_STR_EMPTY: fn = "rt_str_empty"; break;
        case BI_STR_CLEAR: fn = "rt_str_clear"; break;
        case BI_STR_C_STR: fn = "rt_str_c_str"; break;
        case BI_STR_APPEND_STR:
            fn = "rt_str_append_str";
            args.push_back({AK_VALUE, call->args[0], Type(Type::CHAR, 1, false), false});
            break;
        case BI_STR_APPEND_CSTR:
            fn = "rt_str_append_cstr";
            args.push_back({AK_VALUE, call->args[0], Type(Type::CHAR, 1, false), false});
            break;
        case BI_STR_APPEND_CHAR:
            fn = "rt_str_append_char";
            args.push_back({AK_VALUE, call->args[0], Type(Type::CHAR, 0, false), false});
            break;
        default:
            throw logic_error("Gencode: builtin desconocido");
    }
    genCallArgs(args, fn + "@PLT");
}

// Sentencias

void Gencode::visit(BlockStmt* stm) {
    for (ASTNode* item : stm->items) {
        item->accept(this);
    }
}

void Gencode::visit(ExprStmt* stm) {
    if (stm->expr) stm->expr->accept(this);
    if (pushDepth != 0) throw logic_error("Gencode: temporales sin liberar tras una sentencia");
}

void Gencode::visit(IfStmt* stm) {
    string lelse = newLabel("else");
    genBool(stm->condition);
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
    genBool(stm->condition);
    emit("testl %eax, %eax");
    emit("je " + lend);
    breakLabels.push_back(lend);
    continueLabels.push_back(lcond);
    stm->body->accept(this);
    breakLabels.pop_back();
    continueLabels.pop_back();
    emit("jmp " + lcond);
    label(lend);
}

void Gencode::visit(ForStmt* stm) {
    string lcond = newLabel("for");
    string lupd = newLabel("forupd");
    string lend = newLabel("endfor");

    if (stm->initDecl) stm->initDecl->accept(this);
    for (Exp* e : stm->initExprs) e->accept(this);

    label(lcond);
    if (stm->condition) {
        genBool(stm->condition);
        emit("testl %eax, %eax");
        emit("je " + lend);
    }
    breakLabels.push_back(lend);
    continueLabels.push_back(lupd);
    stm->body->accept(this);
    breakLabels.pop_back();
    continueLabels.pop_back();

    label(lupd);
    for (Exp* e : stm->updateExprs) e->accept(this);
    emit("jmp " + lcond);
    label(lend);
}

void Gencode::visit(DoWhileStmt* stm) {
    string lbody = newLabel("do");
    string lcond = newLabel("docond");
    string lend = newLabel("enddo");
    label(lbody);
    breakLabels.push_back(lend);
    continueLabels.push_back(lcond);
    stm->body->accept(this);
    breakLabels.pop_back();
    continueLabels.pop_back();
    label(lcond);
    genBool(stm->condition);
    emit("testl %eax, %eax");
    emit("jne " + lbody);
    label(lend);
}

void Gencode::visit(BreakStmt* /*stm*/) {
    emit("jmp " + breakLabels.back());
}

void Gencode::visit(ContinueStmt* /*stm*/) {
    emit("jmp " + continueLabels.back());
}

void Gencode::visit(ReturnStmt* stm) {
    if (stm->expr) {
        Type rt = curFun->layout.returnType;
        if (rt.isRef) {
            genAddr(stm->expr);
        } else {
            Type et = stm->expr->accept(this);
            convert(et, rt);
        }
    }
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
    if (info.storage == Storage::LOCAL) {
        for (int so : info.stringOffsets) {
            emit("leaq " + to_string(info.offset + so) + "(%rbp), %rdi");
            callRuntime("rt_str_reset");
        }
    }
    if (vi->initializer) {
        genInit(info.type, vi->initializer, info, 0, true);
    }
    if (pushDepth != 0) throw logic_error("Gencode: temporales sin liberar tras una declaracion");
}

// Inicializa el objeto de tipo t ubicado en v+extra
void Gencode::genInit(const Type& t, Exp* init, const VarInfo& v, int extra, bool top) {
    string mem = varMem(v, extra);

    if (init->nodeKind() == NodeKind::BRACED_INIT) {
        BracedInitList* bl = static_cast<BracedInitList*>(init);
        if (top) {
            emit("leaq " + mem + ", %rdi");
            emit("xorl %eax, %eax");
            emit("movq $" + to_string(v.size) + ", %rcx");
            emit("rep stosb");
        }
        for (size_t i = 0; i < bl->elements.size(); i++) {
            genInit(bl->elementTypes[i], bl->elements[i], v, extra + bl->elementOffsets[i], false);
        }
        return;
    }

    if (t.isRef) {
        genAddr(init);
        emit("movq %rax, " + mem);
    } else if (t.isString()) {
        Type vt = init->accept(this);
        emit("movq %rax, %rsi");
        emit("leaq " + mem + ", %rdi");
        callRuntime(vt.isString() ? "rt_str_assign" : "rt_str_assign_cstr");
    } else if (t.isRecord()) {
        init->accept(this);
        emit("movq %rax, %rdx");
        emit("leaq " + mem + ", %rcx");
        copyBytes("%rdx", "%rcx", records.at(t.recordName)->layout.size);
    } else {
        Type vt = init->accept(this);
        convert(vt, t);
        storeTo(t, mem);
    }
}

void Gencode::genGlobals(VarDecl* decl) {
    for (VarInit* vi : decl->inits) {
        globals.push_back(vi->info);
        if (!vi->initializer) continue;
        cur = &initCode;
        curFun = nullptr;
        pushDepth = 0;
        genInit(vi->info.type, vi->initializer, vi->info, 0, true);
        if (pushDepth != 0) throw logic_error("Gencode: temporales sin liberar en un inicializador global");
        cur = &text;
    }
}

void Gencode::visit(FunDecl* fun) {
    genFunction(fun);
}

void Gencode::visit(RecordDecl* record) {
    for (RecordMember* m : record->members) {
        if (m->funDecl) genFunction(m->funDecl);
    }
}

void Gencode::genFunction(FunDecl* fun) {
    const FunctionLayout& L = fun->layout;
    curFun = fun;
    cur = &text;
    pushDepth = 0;
    endLabel = newLabel("ret");
    bool isMain = (L.label == "main");

    if (isMain) *cur << "\t.globl main\n";
    *cur << "\t.type " << L.label << ", @function\n";
    label(L.label);
    emit("pushq %rbp");
    emit("movq %rsp, %rbp");
    emit("subq $" + to_string(L.frameSize) + ", %rsp");

    // Spill de todos los parametros a sus slots; los de pila viven sobre el retorno
    int ni = 0, nf = 0, stackOff = 16;
    for (const VarInfo& p : L.params) {
        Cls c = cls(p.type);
        bool flt = !p.type.isRef && (c == C_FLOAT || c == C_DOUBLE);
        string slot = to_string(p.offset) + "(%rbp)";
        int sz = accessSize(p.type);
        if (flt) {
            if (nf < 8) {
                emit(string(c == C_FLOAT ? "movss" : "movsd") + " %xmm" + to_string(nf) + ", " + slot);
                nf++;
            } else {
                if (sz == 4) {
                    emit("movl " + to_string(stackOff) + "(%rbp), %r10d");
                    emit("movl %r10d, " + slot);
                } else {
                    emit("movq " + to_string(stackOff) + "(%rbp), %r10");
                    emit("movq %r10, " + slot);
                }
                stackOff += 8;
            }
        } else {
            if (ni < 6) {
                if (sz == 8) emit(string("movq ") + R64[ni] + ", " + slot);
                else if (sz == 4) emit(string("movl ") + R32[ni] + ", " + slot);
                else emit(string("movb ") + R8[ni] + ", " + slot);
                ni++;
            } else {
                string src = to_string(stackOff) + "(%rbp)";
                if (sz == 8) {
                    emit("movq " + src + ", %r10");
                    emit("movq %r10, " + slot);
                } else if (sz == 4) {
                    emit("movl " + src + ", %r10d");
                    emit("movl %r10d, " + slot);
                } else {
                    emit("movb " + src + ", %r10b");
                    emit("movb %r10b, " + slot);
                }
                stackOff += 8;
            }
        }
    }

    for (int so : L.stringSlots) {
        emit("movq $0, " + to_string(so) + "(%rbp)");
        emit("movq $0, " + to_string(so + 8) + "(%rbp)");
    }

    if (isMain) emit("call __init_globals");

    if (fun->body) {
        for (ASTNode* item : fun->body->items) {
            item->accept(this);
        }
    }

    if (isMain) emit("movl $0, %eax");

    label(endLabel);
    if (!L.stringSlots.empty()) {
        emit("movq %rax, " + to_string(L.retSaveOffset) + "(%rbp)");
        emit("movsd %xmm0, " + to_string(L.retSaveOffset + 8) + "(%rbp)");
        for (int so : L.stringSlots) {
            emit("leaq " + to_string(so) + "(%rbp), %rdi");
            callRuntime("rt_str_free");
        }
        emit("movq " + to_string(L.retSaveOffset) + "(%rbp), %rax");
        emit("movsd " + to_string(L.retSaveOffset + 8) + "(%rbp), %xmm0");
    }
    emit("leave");
    emit("ret");

    curFun = nullptr;
}

void Gencode::visit(Program* prog) {
    *cur << "\t.text\n";

    for (ASTNode* decl : prog->declarations) {
        if (decl->nodeKind() == NodeKind::VAR_DECL) {
            genGlobals(static_cast<VarDecl*>(decl));
        } else {
            decl->accept(this);
        }
    }

    *cur << "\t.type __init_globals, @function\n";
    label("__init_globals");
    emit("pushq %rbp");
    emit("movq %rsp, %rbp");
    *cur << initCode.str();
    emit("popq %rbp");
    emit("ret");

    for (const VarInfo& g : globals) {
        bss << "\t.balign " << g.align << "\n" << g.label << ":\n\t.zero " << (g.size > 0 ? g.size : 1) << "\n";
    }
}
