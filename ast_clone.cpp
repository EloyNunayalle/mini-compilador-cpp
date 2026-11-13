#include "ast.h"

using namespace std;

static vector<Exp*> cloneList(const vector<Exp*>& v) {
    vector<Exp*> out;
    out.reserve(v.size());
    for (const Exp* e : v) out.push_back(cloneExp(e));
    return out;
}

// Copia superficial del nodo y reemplazo de cada hijo por su clon
Exp* cloneExp(const Exp* e) {
    if (!e) return nullptr;
    switch (e->nodeKind()) {
        case NodeKind::BINARY_EXP: {
            const BinaryExp* s = static_cast<const BinaryExp*>(e);
            BinaryExp* c = new BinaryExp(*s);
            c->left = cloneExp(s->left);
            c->right = cloneExp(s->right);
            return c;
        }
        case NodeKind::UNARY_EXP: {
            const UnaryExp* s = static_cast<const UnaryExp*>(e);
            UnaryExp* c = new UnaryExp(*s);
            c->exp = cloneExp(s->exp);
            return c;
        }
        case NodeKind::ASSIGN_EXP: {
            const AssignExp* s = static_cast<const AssignExp*>(e);
            AssignExp* c = new AssignExp(*s);
            c->target = cloneExp(s->target);
            c->value = cloneExp(s->value);
            return c;
        }
        case NodeKind::NUMBER_EXP:
            return new NumberExp(*static_cast<const NumberExp*>(e));
        case NodeKind::REAL_EXP:
            return new RealExp(*static_cast<const RealExp*>(e));
        case NodeKind::BOOL_EXP:
            return new BoolExp(*static_cast<const BoolExp*>(e));
        case NodeKind::CHAR_EXP:
            return new CharExp(*static_cast<const CharExp*>(e));
        case NodeKind::STRING_EXP:
            return new StringExp(*static_cast<const StringExp*>(e));
        case NodeKind::NULLPTR_EXP:
            return new NullptrExp(*static_cast<const NullptrExp*>(e));
        case NodeKind::ID_EXP:
            return new IdExp(*static_cast<const IdExp*>(e));
        case NodeKind::POSTFIX_EXP: {
            const PostfixExp* s = static_cast<const PostfixExp*>(e);
            PostfixExp* c = new PostfixExp(*s);
            c->base = cloneExp(s->base);
            c->indexExp = cloneExp(s->indexExp);
            c->args = cloneList(s->args);
            return c;
        }
        case NodeKind::NEW_EXP: {
            const NewExp* s = static_cast<const NewExp*>(e);
            NewExp* c = new NewExp(*s);
            c->arrayDims = cloneList(s->arrayDims);
            c->ctorArgs = cloneList(s->ctorArgs);
            c->spec = InitSpec();
            return c;
        }
        case NodeKind::DELETE_EXP: {
            const DeleteExp* s = static_cast<const DeleteExp*>(e);
            DeleteExp* c = new DeleteExp(*s);
            c->exp = cloneExp(s->exp);
            return c;
        }
        case NodeKind::BRACED_INIT: {
            const BracedInitList* s = static_cast<const BracedInitList*>(e);
            BracedInitList* c = new BracedInitList(*s);
            c->elements = cloneList(s->elements);
            c->omitted.clear();
            return c;
        }
        default:
            return nullptr;
    }
}
