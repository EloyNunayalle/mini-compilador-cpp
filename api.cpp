#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include "api.h"
#include "scanner.h"
#include "parser.h"
#include "type_checker.h"
#include "gencode.h"
#include "optimizer.h"

using namespace std;

namespace {

string jstr(const string& s) {
    string r = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    r += buf;
                } else {
                    r += static_cast<char>(c);
                }
        }
    }
    return r + "\"";
}

// Nodo generico del AST en JSON: el frontend lo muestra sin conocer cada clase de nodo
struct JNode {
    string kind;
    int line = 0;
    int col = 0;
    string label;
    string type;
    bool hasType = false;
    bool isConst = false;
    vector<pair<string, string>> attrs;   // valores ya codificados en JSON
    vector<JNode> children;

    void attr(const string& k, int v) { attrs.push_back({k, to_string(v)}); }
    void attr(const string& k, const string& v) { attrs.push_back({k, jstr(v)}); }
    void attr(const string& k, const char* v) { attrs.push_back({k, jstr(v)}); }
    void attr(const string& k, bool v) { attrs.push_back({k, v ? "true" : "false"}); }
};

void emitNode(const JNode& n, ostream& o) {
    o << "{\"kind\":" << jstr(n.kind) << ",\"line\":" << n.line << ",\"col\":" << n.col
      << ",\"label\":" << jstr(n.label);
    if (n.hasType) o << ",\"type\":" << jstr(n.type) << ",\"const\":" << (n.isConst ? "true" : "false");
    else o << ",\"type\":null,\"const\":false";
    o << ",\"attrs\":{";
    for (size_t i = 0; i < n.attrs.size(); i++) {
        if (i) o << ",";
        o << jstr(n.attrs[i].first) << ":" << n.attrs[i].second;
    }
    o << "},\"children\":[";
    for (size_t i = 0; i < n.children.size(); i++) {
        if (i) o << ",";
        emitNode(n.children[i], o);
    }
    o << "]}";
}

class AstJson {
public:
    explicit AstJson(bool typed) : typed(typed) {}

    JNode program(Program* p) {
        JNode n = base("Program", p);
        for (IncludeDirective* i : p->includes) {
            JNode c = base("IncludeDirective", i);
            c.label = "#include <" + i->library + ">";
            n.children.push_back(c);
        }
        for (UsingDirective* u : p->usings) {
            JNode c = base("UsingDirective", u);
            c.label = "using " + string(u->isNamespace ? "namespace " : "") + u->getFullName();
            n.children.push_back(c);
        }
        for (ASTNode* d : p->declarations) n.children.push_back(decl(d));
        if (typed) n.attr("init_frame_size", p->initFrameSize);
        return n;
    }

private:
    bool typed;

    template <typename T>
    JNode base(const char* kind, const T* node) {
        JNode n;
        n.kind = kind;
        n.line = node->line;
        n.col = node->col;
        return n;
    }

    void setType(JNode& n, const Type& t) {
        if (!typed) return;
        n.hasType = true;
        n.type = t.toString();
        n.isConst = t.isConstObj();
    }

    void varAttrs(JNode& n, const VarInfo& v) {
        if (!typed) return;
        const char* st = "none";
        if (v.storage == Storage::LOCAL) st = "local";
        else if (v.storage == Storage::GLOBAL) st = "global";
        else if (v.storage == Storage::FIELD) st = "field";
        n.attr("storage", st);
        n.attr("offset", v.offset);
        n.attr("size", v.size);
        n.attr("align", v.align);
        if (v.indirect) n.attr("indirect", true);
        if (v.storage == Storage::GLOBAL) n.attr("symbol", v.label);
    }

    JNode decl(ASTNode* d) {
        switch (d->nodeKind()) {
            case NodeKind::VAR_DECL: return varDecl(static_cast<VarDecl*>(d));
            case NodeKind::FUN_DECL: return funDecl(static_cast<FunDecl*>(d), false);
            case NodeKind::RECORD_DECL: return recordDecl(static_cast<RecordDecl*>(d));
            default: return base("Unknown", d);
        }
    }

    JNode recordDecl(RecordDecl* r) {
        JNode n = base("RecordDecl", r);
        n.label = r->name;
        n.attr("record_kind", r->kind == RECORD_CLASS ? "class" : "struct");
        if (typed) {
            n.attr("size", r->layout.size);
            n.attr("align", r->layout.align);
            n.attr("trivial_for_calls", r->layout.abi.trivialForCalls);
        }
        for (RecordMember* m : r->members) {
            JNode c = base("RecordMember", m);
            c.attr("access", m->access == ACCESS_PRIVATE ? "private" : "public");
            if (m->varDecl) c.children.push_back(varDecl(m->varDecl));
            if (m->funDecl) c.children.push_back(funDecl(m->funDecl, false));
            c.label = m->varDecl ? "campo" : "metodo";
            n.children.push_back(c);
        }
        if (typed) {
            for (FunDecl* f : r->implicitFuns) n.children.push_back(funDecl(f, true));
        }
        return n;
    }

    JNode funDecl(FunDecl* f, bool implicit) {
        JNode n = base("FunDecl", f);
        n.label = f->name;
        const char* fk = "function";
        switch (f->funKind) {
            case FunKind::CTOR: fk = "constructor"; break;
            case FunKind::DTOR: fk = "destructor"; break;
            case FunKind::COPY_ASSIGN: fk = "copy_assign"; break;
            default: if (f->layout.isMethod) fk = "method"; break;
        }
        n.attr("fun_kind", fk);
        n.attr("const_method", f->isConst);
        n.attr("implicit", implicit || f->isImplicit);
        n.attr("return_type", f->returnType.toString());
        if (typed) {
            n.hasType = true;
            n.type = f->layout.returnType.toString();
            n.attr("symbol", f->layout.label);
            n.attr("frame_size", f->layout.frameSize);
        }
        for (Param* p : f->params) {
            JNode c = base("Param", p);
            c.label = p->name;
            Type t = p->type;
            t.ptrLevel = p->ptrCount;
            t.isRef = p->isRef;
            t.constBits = (p->type.constBits & 1u) | p->ptrConst;
            c.hasType = true;
            c.type = t.toString();
            c.isConst = t.isConstObj();
            varAttrs(c, p->info);
            n.children.push_back(c);
        }
        for (const MemberInit& mi : f->memberInits) {
            JNode c;
            c.kind = "MemberInit";
            c.line = mi.line;
            c.col = mi.col;
            c.label = mi.name;
            for (Exp* a : mi.args) c.children.push_back(exp(a));
            n.children.push_back(c);
        }
        if (f->body) n.children.push_back(stmt(f->body));
        return n;
    }

    JNode varDecl(VarDecl* v) {
        JNode n = base("VarDecl", v);
        n.label = v->baseType.toString();
        for (VarInit* vi : v->inits) {
            JNode c = base("VarInit", vi);
            c.label = vi->name;
            if (typed) {
                c.hasType = true;
                c.type = vi->info.type.toString();
                c.isConst = vi->info.type.isConstObj();
                varAttrs(c, vi->info);
            }
            for (Exp* a : vi->ctorArgs) c.children.push_back(exp(a));
            if (vi->initializer) c.children.push_back(exp(vi->initializer));
            if (vi->directInit) c.attr("direct_init", true);
            n.children.push_back(c);
        }
        return n;
    }

    JNode stmt(ASTNode* s) {
        switch (s->nodeKind()) {
            case NodeKind::BLOCK_STMT: {
                BlockStmt* b = static_cast<BlockStmt*>(s);
                JNode n = base("BlockStmt", b);
                for (ASTNode* i : b->items) n.children.push_back(stmt(i));
                return n;
            }
            case NodeKind::VAR_DECL:
                return varDecl(static_cast<VarDecl*>(s));
            case NodeKind::EXPR_STMT: {
                ExprStmt* e = static_cast<ExprStmt*>(s);
                JNode n = base("ExprStmt", e);
                if (e->expr) n.children.push_back(exp(e->expr));
                return n;
            }
            case NodeKind::IF_STMT: {
                IfStmt* i = static_cast<IfStmt*>(s);
                JNode n = base("IfStmt", i);
                n.children.push_back(exp(i->condition));
                n.children.push_back(stmt(i->thenStmt));
                if (i->elseStmt) n.children.push_back(stmt(i->elseStmt));
                return n;
            }
            case NodeKind::WHILE_STMT: {
                WhileStmt* w = static_cast<WhileStmt*>(s);
                JNode n = base("WhileStmt", w);
                n.children.push_back(exp(w->condition));
                n.children.push_back(stmt(w->body));
                return n;
            }
            case NodeKind::FOR_STMT: {
                ForStmt* f = static_cast<ForStmt*>(s);
                JNode n = base("ForStmt", f);
                if (f->initDecl) n.children.push_back(varDecl(f->initDecl));
                for (Exp* e : f->initExprs) n.children.push_back(exp(e));
                if (f->condition) n.children.push_back(exp(f->condition));
                for (Exp* e : f->updateExprs) n.children.push_back(exp(e));
                n.children.push_back(stmt(f->body));
                return n;
            }
            case NodeKind::DO_WHILE_STMT: {
                DoWhileStmt* d = static_cast<DoWhileStmt*>(s);
                JNode n = base("DoWhileStmt", d);
                n.children.push_back(stmt(d->body));
                n.children.push_back(exp(d->condition));
                return n;
            }
            case NodeKind::BREAK_STMT: return base("BreakStmt", s);
            case NodeKind::CONTINUE_STMT: return base("ContinueStmt", s);
            case NodeKind::RETURN_STMT: {
                ReturnStmt* r = static_cast<ReturnStmt*>(s);
                JNode n = base("ReturnStmt", r);
                if (r->expr) n.children.push_back(exp(r->expr));
                return n;
            }
            default:
                return base("Unknown", s);
        }
    }

    JNode exp(Exp* e) {
        JNode n;
        n.line = e->line;
        n.col = e->col;
        setType(n, e->inferredType);
        switch (e->nodeKind()) {
            case NodeKind::BINARY_EXP: {
                BinaryExp* b = static_cast<BinaryExp*>(e);
                n.kind = "BinaryExp";
                n.label = Exp::binopToString(b->op);
                n.children.push_back(exp(b->left));
                n.children.push_back(exp(b->right));
                break;
            }
            case NodeKind::UNARY_EXP: {
                UnaryExp* u = static_cast<UnaryExp*>(e);
                n.kind = "UnaryExp";
                n.label = Exp::unaryopToString(u->op);
                n.children.push_back(exp(u->exp));
                break;
            }
            case NodeKind::ASSIGN_EXP: {
                AssignExp* a = static_cast<AssignExp*>(e);
                n.kind = "AssignExp";
                n.label = "=";
                n.children.push_back(exp(a->target));
                n.children.push_back(exp(a->value));
                break;
            }
            case NodeKind::NUMBER_EXP:
                n.kind = "NumberExp";
                n.label = to_string(static_cast<NumberExp*>(e)->value);
                break;
            case NodeKind::REAL_EXP: {
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", static_cast<RealExp*>(e)->value);
                n.kind = "RealExp";
                n.label = buf;
                break;
            }
            case NodeKind::BOOL_EXP:
                n.kind = "BoolExp";
                n.label = static_cast<BoolExp*>(e)->value ? "true" : "false";
                break;
            case NodeKind::CHAR_EXP:
                n.kind = "CharExp";
                n.label = string(1, static_cast<CharExp*>(e)->value);
                break;
            case NodeKind::STRING_EXP:
                n.kind = "StringExp";
                n.label = static_cast<StringExp*>(e)->value;
                break;
            case NodeKind::NULLPTR_EXP:
                n.kind = "NullptrExp";
                n.label = "nullptr";
                break;
            case NodeKind::ID_EXP: {
                IdExp* id = static_cast<IdExp*>(e);
                n.kind = "IdExp";
                n.label = id->getFullName();
                if (typed && id->idKind != ID_NONE) varAttrs(n, id->var);
                break;
            }
            case NodeKind::POSTFIX_EXP: {
                PostfixExp* p = static_cast<PostfixExp*>(e);
                n.kind = "PostfixExp";
                const char* names[] = {"post++", "post--", "[]", ".", "->", "call"};
                n.label = names[p->kind];
                if (p->kind == DOT_ACCESS || p->kind == ARROW_ACCESS) {
                    n.label += " " + p->memberName;
                    if (typed) n.attr("field_offset", p->fieldOffset);
                }
                if (p->kind == CALL_ACCESS && typed) {
                    const char* ck = "none";
                    switch (p->callKind) {
                        case CALL_FUNCTION: ck = "function"; break;
                        case CALL_METHOD: ck = "method"; break;
                        case CALL_METHOD_IMPLICIT: ck = "method_implicit"; break;
                        case CALL_BUILTIN: ck = "builtin"; break;
                        case CALL_CTOR: ck = "constructor"; break;
                        default: break;
                    }
                    n.attr("call_kind", ck);
                    if (p->callee) n.attr("callee", p->callee->layout.label);
                    if (p->retTemp != 0) n.attr("return_temp", p->retTemp);
                    string modes;
                    for (const ArgInfo& ai : p->argInfo) {
                        const char* m = "value";
                        switch (ai.mode) {
                            case AM_REF: m = "ref"; break;
                            case AM_REF_TEMP: m = "ref_temp"; break;
                            case AM_AGG: m = "agg"; break;
                            case AM_COPY: m = "copy"; break;
                            default: break;
                        }
                        if (!modes.empty()) modes += ",";
                        modes += m;
                    }
                    if (!modes.empty()) n.attr("arg_modes", modes);
                }
                if (p->base) n.children.push_back(exp(p->base));
                if (p->indexExp) n.children.push_back(exp(p->indexExp));
                for (Exp* a : p->args) n.children.push_back(exp(a));
                break;
            }
            case NodeKind::NEW_EXP: {
                NewExp* ne = static_cast<NewExp*>(e);
                n.kind = "NewExp";
                n.label = "new " + ne->allocType.toString();
                if (typed) n.attr("cookie", ne->hasCookie);
                for (Exp* d : ne->arrayDims) n.children.push_back(exp(d));
                for (Exp* a : ne->ctorArgs) n.children.push_back(exp(a));
                break;
            }
            case NodeKind::DELETE_EXP: {
                DeleteExp* de = static_cast<DeleteExp*>(e);
                n.kind = "DeleteExp";
                n.label = de->isArray ? "delete[]" : "delete";
                n.children.push_back(exp(de->exp));
                break;
            }
            case NodeKind::BRACED_INIT: {
                BracedInitList* bl = static_cast<BracedInitList*>(e);
                n.kind = "BracedInitList";
                n.label = "{...}";
                for (Exp* x : bl->elements) n.children.push_back(exp(x));
                break;
            }
            default:
                n.kind = "Unknown";
        }
        return n;
    }
};

const char* availName(Avail a) {
    switch (a) {
        case Avail::USER: return "user";
        case Avail::IMPLICIT: return "implicit";
        case Avail::DELETED: return "deleted";
        default: return "none";
    }
}

const char* clsName(AbiClass c) {
    switch (c) {
        case AbiClass::INTEGER: return "INTEGER";
        case AbiClass::SSE: return "SSE";
        case AbiClass::MEMORY: return "MEMORY";
        default: return "NO_CLASS";
    }
}

const char* passName(Pass p) {
    switch (p) {
        case Pass::INT_REG: return "INT_REG";
        case Pass::SSE_REG: return "SSE_REG";
        case Pass::AGG_REGS: return "AGG_REGS";
        case Pass::STACK: return "STACK";
        case Pass::INVISIBLE_REF: return "INVISIBLE_REF";
        case Pass::EMPTY: return "EMPTY";
        case Pass::VOID_RET: return "VOID";
        case Pass::HIDDEN_PTR: return "HIDDEN_PTR";
    }
    return "";
}

string regsOf(const AbiLoc& a, bool isRet) {
    static const char* R[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    string out = "[";
    auto add = [&](const string& s) {
        if (out.size() > 1) out += ",";
        out += jstr(s);
    };
    if (a.pass == Pass::INT_REG || a.pass == Pass::INVISIBLE_REF) {
        if (isRet) add("rax");
        else if (a.reg[0] >= 0) add(R[a.reg[0]]);
    } else if (a.pass == Pass::SSE_REG) {
        add("xmm" + to_string(isRet ? 0 : a.reg[0]));
    } else if (a.pass == Pass::AGG_REGS) {
        for (int k = 0; k < a.neight; k++) {
            if (a.cls[k] == AbiClass::SSE) add("xmm" + to_string(a.reg[k]));
            else if (isRet) add(a.reg[k] == 0 ? "rax" : "rdx");
            else add(R[a.reg[k]]);
        }
    } else if (a.pass == Pass::HIDDEN_PTR && isRet) {
        add("rax");
    }
    return out + "]";
}

void functionJson(FunDecl* f, const string& owner, ostream& o) {
    const FunctionLayout& L = f->layout;
    o << "{\"name\":" << jstr(owner.empty() ? f->name : owner + "::" + f->name)
      << ",\"label\":" << jstr(L.label) << ",\"frame_size\":" << L.frameSize
      << ",\"is_method\":" << (L.isMethod ? "true" : "false") << ",\"owner\":" << jstr(owner)
      << ",\"fun_kind\":" << jstr(f->funKind == FunKind::CTOR ? "constructor" : f->funKind == FunKind::DTOR ? "destructor" :
                                  f->funKind == FunKind::COPY_ASSIGN ? "copy_assign" : "function")
      << ",\"implicit\":" << (f->isImplicit ? "true" : "false") << ",\"params\":[";
    for (size_t i = 0; i < L.params.size(); i++) {
        if (i) o << ",";
        string pname = "this";
        size_t pi = i;
        if (L.isMethod) {
            if (i > 0) pname = f->params[i - 1]->name;
        } else {
            pname = f->params[pi]->name;
        }
        const AbiLoc& a = L.paramAbi[i];
        o << "{\"name\":" << jstr(pname) << ",\"type\":" << jstr(L.params[i].type.toString())
          << ",\"offset\":" << L.params[i].offset << ",\"pass\":" << jstr(passName(a.pass))
          << ",\"regs\":" << regsOf(a, false) << ",\"stack_offset\":" << a.stackOffset << ",\"size\":" << a.size << "}";
    }
    o << "],\"return\":{\"type\":" << jstr(L.returnType.toString()) << ",\"pass\":" << jstr(passName(L.retAbi.pass))
      << ",\"regs\":" << regsOf(L.retAbi, true) << "},\"stack_arg_bytes\":" << L.stackArgBytes << "}";
}

string layoutJson(Program* prog, TypeChecker& tc) {
    ostringstream o;
    o << "{\"records\":[";
    bool first = true;
    for (ASTNode* d : prog->declarations) {
        if (d->nodeKind() != NodeKind::RECORD_DECL) continue;
        RecordDecl* r = static_cast<RecordDecl*>(d);
        auto it = tc.records.find(r->name);
        if (it == tc.records.end() || it->second != r) continue;
        const RecordLayout& L = r->layout;
        if (!first) o << ",";
        first = false;
        o << "{\"name\":" << jstr(L.name) << ",\"size\":" << L.size << ",\"align\":" << L.align
          << ",\"trivial_for_calls\":" << (L.abi.trivialForCalls ? "true" : "false")
          << ",\"needs_dtor\":" << (L.dtorTrivial ? "false" : "true")
          << ",\"has_string\":" << (L.hasString ? "true" : "false")
          << ",\"aggregate\":" << (L.isAggregate ? "true" : "false") << ",\"fields\":[";
        for (size_t i = 0; i < L.fields.size(); i++) {
            const FieldInfo& f = L.fields[i];
            if (i) o << ",";
            o << "{\"name\":" << jstr(f.name) << ",\"type\":" << jstr(f.type.toString()) << ",\"offset\":" << f.offset
              << ",\"size\":" << f.size << ",\"align\":" << f.align << ",\"const\":" << (f.type.isConstObj() ? "true" : "false")
              << ",\"access\":" << jstr(f.isPrivate ? "private" : "public")
              << ",\"class_init\":" << (f.classInit ? "true" : "false") << "}";
        }
        string abiClass = "INTEGER";
        if (L.abi.empty) abiClass = "EMPTY";
        else if (!L.abi.trivialForCalls) abiClass = "NON_TRIVIAL";
        else if (L.abi.memory) abiClass = "MEMORY";
        else {
            bool anyI = false, anyS = false;
            for (int k = 0; k < L.abi.neight; k++) {
                if (L.abi.cls[k] == AbiClass::SSE) anyS = true;
                else anyI = true;
            }
            abiClass = (anyI && anyS) ? "MIXED" : (anyS ? "SSE" : "INTEGER");
        }
        o << "],\"special\":{\"default_ctor\":" << jstr(availName(L.defaultCtorAvail))
          << ",\"param_ctor\":" << jstr(L.paramCtorAvail == Avail::USER ? "user" : "none")
          << ",\"copy_ctor\":" << jstr(availName(L.copyCtorAvail))
          << ",\"copy_assign\":" << jstr(availName(L.copyAssignAvail))
          << ",\"dtor\":" << jstr(availName(L.dtorAvail)) << "},\"abi\":{\"class\":" << jstr(abiClass) << ",\"eightbytes\":[";
        for (int k = 0; k < L.abi.neight && abiClass != "MEMORY" && abiClass != "NON_TRIVIAL"; k++) {
            if (k) o << ",";
            o << jstr(clsName(L.abi.cls[k]));
        }
        o << "]}}";
    }
    o << "],\"functions\":[";
    first = true;
    auto addFun = [&](FunDecl* f, const string& owner) {
        if (!first) o << ",";
        first = false;
        functionJson(f, owner, o);
    };
    for (ASTNode* d : prog->declarations) {
        if (d->nodeKind() == NodeKind::FUN_DECL) {
            addFun(static_cast<FunDecl*>(d), "");
        } else if (d->nodeKind() == NodeKind::RECORD_DECL) {
            RecordDecl* r = static_cast<RecordDecl*>(d);
            auto it = tc.records.find(r->name);
            if (it == tc.records.end() || it->second != r) continue;
            for (RecordMember* m : r->members) {
                if (m->funDecl) addFun(m->funDecl, r->name);
            }
            for (FunDecl* f : r->implicitFuns) addFun(f, r->name);
        }
    }
    o << "]}";
    return o.str();
}

string diagJson(const vector<Diagnostic>& v) {
    string s = "[";
    for (size_t i = 0; i < v.size(); i++) {
        if (i) s += ",";
        s += "{\"phase\":" + jstr(v[i].phase) + ",\"message\":" + jstr(v[i].message) + ",\"line\":" +
             to_string(v[i].line) + ",\"column\":" + to_string(v[i].col) + "}";
    }
    return s + "]";
}

string nodeJson(Program* p, bool typed) {
    AstJson w(typed);
    JNode n = w.program(p);
    ostringstream o;
    emitNode(n, o);
    return o.str();
}

// "Error sintactico [Linea L, Columna C]: mensaje" -> posicion y mensaje
Diagnostic parseError(const string& what) {
    Diagnostic d;
    d.phase = "parse";
    d.message = what;
    size_t a = what.find("[Línea ");
    if (a != string::npos) {
        size_t b = what.find(", Columna ", a);
        size_t c = what.find("]", b);
        if (b != string::npos && c != string::npos) {
            try {
                d.line = stoi(what.substr(a + strlen("[Línea "), b - a - strlen("[Línea ")));
                d.col = stoi(what.substr(b + strlen(", Columna "), c - b - strlen(", Columna ")));
            } catch (...) {
            }
            size_t colon = what.find("]: ", c);
            if (colon != string::npos) d.message = what.substr(colon + 3);
        }
    }
    return d;
}

}  // namespace

string runApi(const string& source, const ApiOptions& opts) {
    string mode = opts.mode;
    bool wantAst = mode != "scan";
    bool wantCheck = mode == "check" || mode == "asm" || mode == "all";
    bool wantAsm = mode == "asm" || mode == "all";

    string tokens = "null", ast = "null", typedAst = "null", layout = "null", assembly = "null", optim = "null";
    vector<Diagnostic> errors, warnings;
    string phase = "lex";
    bool success = false;

    auto respond = [&]() {
        ostringstream o;
        o << "{\"success\":" << (success ? "true" : "false") << ",\"phase\":" << jstr(phase) << ",\"errors\":"
          << diagJson(errors) << ",\"warnings\":" << diagJson(warnings) << ",\"tokens\":" << tokens
          << ",\"ast\":" << ast << ",\"typed_ast\":" << typedAst << ",\"layout\":" << layout
          << ",\"assembly\":" << assembly << ",\"optimizations\":" << optim << "}";
        return o.str();
    };

    // fase lexica
    {
        Scanner sc(source);
        ostringstream o;
        o << "[";
        bool firstTok = true;
        bool lexFail = false;
        while (true) {
            Token* t = sc.nextToken();
            if (!t || t->type == Token::END) {
                delete t;
                break;
            }
            if (t->type == Token::ERR) {
                lexFail = true;
                errors.push_back({"lex", t->text, t->line, t->col});
                delete t;
                break;
            }
            if (!firstTok) o << ",";
            firstTok = false;
            o << "{\"type\":" << jstr(Token::typeName(t->type)) << ",\"lexeme\":" << jstr(t->text) << ",\"line\":"
              << t->line << ",\"column\":" << t->col << "}";
            delete t;
        }
        o << "]";
        tokens = o.str();
        if (lexFail) return respond();
    }
    if (mode == "scan") {
        phase = "done";
        success = true;
        return respond();
    }

    // fase sintactica
    phase = "parse";
    Program* prog = nullptr;
    auto parseSource = [&](Program*& out) {
        Scanner sc(source);
        Parser parser(&sc);
        try {
            out = parser.parseProgram();
            return true;
        } catch (const exception& e) {
            errors.push_back(parseError(e.what()));
            out = nullptr;
            return false;
        }
    };
    if (!parseSource(prog)) return respond();
    if (wantAst) ast = nodeJson(prog, false);
    if (!wantCheck) {
        delete prog;
        phase = "done";
        success = true;
        return respond();
    }

    // semantica y layout
    phase = "typecheck";
    TypeChecker checker;
    checker.check(prog);
    warnings = checker.warningList;
    if (checker.hasErrors()) {
        errors = checker.errorList;
        delete prog;
        return respond();
    }

    // optimizacion opcional
    OptReport rep;
    bool discarded = false;
    if (opts.optimize && !opts.passes.empty()) {
        string before = nodeJson(prog, false);
        bool ok = runOptimizations(prog, checker, opts.passes, rep);
        if (!ok) {
            discarded = true;
            rep.log.push_back("optimizacion descartada: se compila sin optimizar");
            delete prog;
            prog = nullptr;
            parseSource(prog);
            checker.check(prog);
        }
        string after = nodeJson(prog, false);
        ostringstream o;
        o << "{\"enabled\":" << (discarded ? "false" : "true") << ",\"passes\":[";
        for (size_t i = 0; i < rep.passes.size(); i++) o << (i ? "," : "") << jstr(rep.passes[i]);
        o << "],\"stats\":{";
        for (size_t i = 0; i < rep.results.size(); i++) {
            const PassResult& r = rep.results[i];
            o << (i ? "," : "") << jstr(r.name) << ":{\"count\":" << r.count << ",\"details\":[";
            for (size_t k = 0; k < r.details.size(); k++) o << (k ? "," : "") << jstr(r.details[k]);
            o << "]}";
        }
        o << "},\"log\":[";
        for (size_t i = 0; i < rep.log.size(); i++) o << (i ? "," : "") << jstr(rep.log[i]);
        o << "],\"ast_before\":" << before << ",\"ast_after\":" << after << "}";
        optim = o.str();
    } else {
        optim = "{\"enabled\":false,\"passes\":[],\"stats\":{},\"log\":[],\"ast_before\":null,\"ast_after\":null}";
    }

    typedAst = nodeJson(prog, true);
    layout = layoutJson(prog, checker);
    phase = "layout";
    if (!wantAsm) {
        delete prog;
        phase = "done";
        success = true;
        return respond();
    }

    // generacion de codigo
    phase = "gencode";
    try {
        ostringstream asmOut;
        Gencode gen(checker.records);
        gen.generate(prog, asmOut);
        assembly = jstr(asmOut.str());
    } catch (const exception& e) {
        errors.push_back({"gencode", e.what(), 0, 0});
        delete prog;
        return respond();
    }
    delete prog;
    phase = "done";
    success = true;
    return respond();
}
