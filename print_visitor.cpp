#include "print_visitor.h"

using namespace std;

ASTPrinter::ASTPrinter(ostream& out) : out(out), indentLevel(0) {}

void ASTPrinter::printIndent() {
    for (size_t i = 0; i < isLastStack.size(); i++) {
        if (i == isLastStack.size() - 1) {
            out << (isLastStack[i] ? "└── " : "├── ");
        } else {
            out << (isLastStack[i] ? "    " : "│   ");
        }
    }
}

void ASTPrinter::pushNode(bool isLast) {
    isLastStack.push_back(isLast);
    indentLevel++;
}

void ASTPrinter::popNode() {
    if (!isLastStack.empty()) {
        isLastStack.pop_back();
        indentLevel--;
    }
}

void ASTPrinter::print(ASTNode* node) {
    if (!node) return;
    isLastStack.clear();
    indentLevel = 0;
    node->accept(this);
}

// =========================================================================
// Expresiones
// =========================================================================

Type ASTPrinter::visit(BinaryExp* exp) {
    printIndent();
    out << "BinaryExp (" << Exp::binopToString(exp->op) << ") [L" << exp->line << ":" << exp->col << "]\n";

    pushNode(false);
    exp->left->accept(this);
    popNode();

    pushNode(true);
    exp->right->accept(this);
    popNode();

    return Type();
}

Type ASTPrinter::visit(UnaryExp* exp) {
    printIndent();
    out << "UnaryExp (" << Exp::unaryopToString(exp->op) << ") [L" << exp->line << ":" << exp->col << "]\n";

    pushNode(true);
    exp->exp->accept(this);
    popNode();

    return Type();
}

Type ASTPrinter::visit(AssignExp* exp) {
    printIndent();
    out << "AssignExp (=) [L" << exp->line << ":" << exp->col << "]\n";

    pushNode(false);
    exp->target->accept(this);
    popNode();

    pushNode(true);
    exp->value->accept(this);
    popNode();

    return Type();
}

Type ASTPrinter::visit(NumberExp* exp) {
    printIndent();
    out << "NumberExp: " << exp->value << " [L" << exp->line << ":" << exp->col << "]\n";
    return Type(static_cast<int>(exp->value));
}

Type ASTPrinter::visit(RealExp* exp) {
    printIndent();
    out << "RealExp: " << exp->value << " [L" << exp->line << ":" << exp->col << "]\n";
    return Type(exp->value);
}

Type ASTPrinter::visit(BoolExp* exp) {
    printIndent();
    out << "BoolExp: " << (exp->value ? "true" : "false") << " [L" << exp->line << ":" << exp->col << "]\n";
    return Type(exp->value);
}

Type ASTPrinter::visit(CharExp* exp) {
    printIndent();
    out << "CharExp: '" << exp->value << "' [L" << exp->line << ":" << exp->col << "]\n";
    return Type(exp->value);
}

Type ASTPrinter::visit(StringExp* exp) {
    printIndent();
    out << "StringExp: \"" << exp->value << "\" [L" << exp->line << ":" << exp->col << "]\n";
    return Type(exp->value);
}

Type ASTPrinter::visit(NullptrExp* exp) {
    printIndent();
    out << "NullptrExp [L" << exp->line << ":" << exp->col << "]\n";
    return Type();
}

Type ASTPrinter::visit(IdExp* exp) {
    printIndent();
    out << "IdExp: " << exp->getFullName() << " [L" << exp->line << ":" << exp->col << "]\n";
    return Type();
}

Type ASTPrinter::visit(PostfixExp* exp) {
    printIndent();
    string kindStr = "";
    switch (exp->kind) {
        case POST_INC:     kindStr = "POST_INC (++)"; break;
        case POST_DEC:     kindStr = "POST_DEC (--)"; break;
        case ARRAY_ACCESS: kindStr = "ARRAY_ACCESS ([])"; break;
        case DOT_ACCESS:   kindStr = "DOT_ACCESS (." + exp->memberName + ")"; break;
        case ARROW_ACCESS: kindStr = "ARROW_ACCESS (->" + exp->memberName + ")"; break;
        case CALL_ACCESS:  kindStr = "CALL_ACCESS ()"; break;
    }
    out << "PostfixExp (" << kindStr << ") [L" << exp->line << ":" << exp->col << "]\n";

    bool hasSub = (exp->indexExp != nullptr || !exp->args.empty());
    pushNode(!hasSub);
    exp->base->accept(this);
    popNode();

    if (exp->indexExp) {
        pushNode(true);
        exp->indexExp->accept(this);
        popNode();
    }

    for (size_t i = 0; i < exp->args.size(); i++) {
        pushNode(i == exp->args.size() - 1);
        exp->args[i]->accept(this);
        popNode();
    }

    return Type();
}

Type ASTPrinter::visit(NewExp* exp) {
    printIndent();
    out << "NewExp: " << exp->allocType.toString() << " [L" << exp->line << ":" << exp->col << "]\n";
    for (size_t i = 0; i < exp->arrayDims.size(); i++) {
        pushNode(i == exp->arrayDims.size() - 1);
        exp->arrayDims[i]->accept(this);
        popNode();
    }
    return Type();
}

Type ASTPrinter::visit(DeleteExp* exp) {
    printIndent();
    out << "DeleteExp" << (exp->isArray ? "[]" : "") << " [L" << exp->line << ":" << exp->col << "]\n";
    pushNode(true);
    exp->exp->accept(this);
    popNode();
    return Type();
}

Type ASTPrinter::visit(BracedInitList* exp) {
    printIndent();
    out << "BracedInitList {" << exp->elements.size() << " items} [L" << exp->line << ":" << exp->col << "]\n";
    for (size_t i = 0; i < exp->elements.size(); i++) {
        pushNode(i == exp->elements.size() - 1);
        exp->elements[i]->accept(this);
        popNode();
    }
    return Type();
}

// =========================================================================
// Sentencias
// =========================================================================

void ASTPrinter::visit(BlockStmt* stm) {
    printIndent();
    out << "BlockStmt [L" << stm->line << ":" << stm->col << "]\n";
    for (size_t i = 0; i < stm->items.size(); i++) {
        pushNode(i == stm->items.size() - 1);
        stm->items[i]->accept(this);
        popNode();
    }
}

void ASTPrinter::visit(ExprStmt* stm) {
    printIndent();
    out << "ExprStmt [L" << stm->line << ":" << stm->col << "]\n";
    pushNode(true);
    stm->expr->accept(this);
    popNode();
}

void ASTPrinter::visit(IfStmt* stm) {
    printIndent();
    out << "IfStmt [L" << stm->line << ":" << stm->col << "]\n";

    pushNode(false);
    stm->condition->accept(this);
    popNode();

    pushNode(stm->elseStmt == nullptr);
    stm->thenStmt->accept(this);
    popNode();

    if (stm->elseStmt) {
        pushNode(true);
        stm->elseStmt->accept(this);
        popNode();
    }
}

void ASTPrinter::visit(WhileStmt* stm) {
    printIndent();
    out << "WhileStmt [L" << stm->line << ":" << stm->col << "]\n";

    pushNode(false);
    stm->condition->accept(this);
    popNode();

    pushNode(true);
    stm->body->accept(this);
    popNode();
}

void ASTPrinter::visit(ForStmt* stm) {
    printIndent();
    out << "ForStmt [L" << stm->line << ":" << stm->col << "]\n";

    if (stm->initDecl) {
        pushNode(false);
        stm->initDecl->accept(this);
        popNode();
    }
    for (Exp* e : stm->initExprs) {
        pushNode(false);
        e->accept(this);
        popNode();
    }
    if (stm->condition) {
        pushNode(false);
        stm->condition->accept(this);
        popNode();
    }
    for (Exp* e : stm->updateExprs) {
        pushNode(false);
        e->accept(this);
        popNode();
    }

    pushNode(true);
    stm->body->accept(this);
    popNode();
}

void ASTPrinter::visit(DoWhileStmt* stm) {
    printIndent();
    out << "DoWhileStmt [L" << stm->line << ":" << stm->col << "]\n";

    pushNode(false);
    stm->body->accept(this);
    popNode();

    pushNode(true);
    stm->condition->accept(this);
    popNode();
}

void ASTPrinter::visit(BreakStmt* stm) {
    printIndent();
    out << "BreakStmt [L" << stm->line << ":" << stm->col << "]\n";
}

void ASTPrinter::visit(ContinueStmt* stm) {
    printIndent();
    out << "ContinueStmt [L" << stm->line << ":" << stm->col << "]\n";
}

void ASTPrinter::visit(ReturnStmt* stm) {
    printIndent();
    out << "ReturnStmt [L" << stm->line << ":" << stm->col << "]\n";
    if (stm->expr) {
        pushNode(true);
        stm->expr->accept(this);
        popNode();
    }
}

// =========================================================================
// Declaraciones y Directivas
// =========================================================================

void ASTPrinter::visit(VarInit* init) {
    printIndent();
    out << "VarInit: ";
    for (int i = 0; i < init->ptrCount; i++) out << "*";
    if (init->isRef) out << "&";
    out << init->name;
    for (size_t i = 0; i < init->arrayDims.size(); i++) out << "[]";
    out << " [L" << init->line << ":" << init->col << "]\n";

    for (Exp* dim : init->arrayDims) {
        pushNode(init->initializer == nullptr && dim == init->arrayDims.back());
        dim->accept(this);
        popNode();
    }
    if (init->initializer) {
        pushNode(true);
        init->initializer->accept(this);
        popNode();
    }
}

void ASTPrinter::visit(VarDecl* decl) {
    printIndent();
    out << "VarDecl (" << decl->baseType.toString() << ") [L" << decl->line << ":" << decl->col << "]\n";
    for (size_t i = 0; i < decl->inits.size(); i++) {
        pushNode(i == decl->inits.size() - 1);
        decl->inits[i]->accept(this);
        popNode();
    }
}

void ASTPrinter::visit(Param* param) {
    printIndent();
    out << "Param: " << param->type.toString() << " ";
    for (int i = 0; i < param->ptrCount; i++) out << "*";
    if (param->isRef) out << "&";
    out << param->name << " [L" << param->line << ":" << param->col << "]\n";
}

void ASTPrinter::visit(FunDecl* fun) {
    printIndent();
    out << "FunDecl: " << fun->returnType.toString() << " " << fun->name << "(...) [L" << fun->line << ":" << fun->col << "]\n";

    for (Param* p : fun->params) {
        pushNode(false);
        p->accept(this);
        popNode();
    }

    pushNode(true);
    fun->body->accept(this);
    popNode();
}

void ASTPrinter::visit(RecordMember* member) {
    printIndent();
    string acc = (member->access == ACCESS_PUBLIC) ? "public:" : "private:";
    out << "RecordMember (" << acc << ")\n";

    pushNode(true);
    if (member->varDecl) member->varDecl->accept(this);
    else if (member->funDecl) member->funDecl->accept(this);
    popNode();
}

void ASTPrinter::visit(RecordDecl* record) {
    printIndent();
    string k = (record->kind == RECORD_CLASS) ? "class" : "struct";
    out << "RecordDecl (" << k << " " << record->name << ") [L" << record->line << ":" << record->col << "]\n";

    for (size_t i = 0; i < record->members.size(); i++) {
        pushNode(i == record->members.size() - 1);
        record->members[i]->accept(this);
        popNode();
    }
}

void ASTPrinter::visit(IncludeDirective* inc) {
    printIndent();
    out << "#include <" << inc->library << "> [L" << inc->line << ":" << inc->col << "]\n";
}

void ASTPrinter::visit(UsingDirective* usg) {
    printIndent();
    out << "using " << (usg->isNamespace ? "namespace " : "") << usg->getFullName()
        << "; [L" << usg->line << ":" << usg->col << "]\n";
}

void ASTPrinter::visit(Program* prog) {
    out << "Program [L" << prog->line << ":" << prog->col << "]\n";
    size_t total = prog->includes.size() + prog->usings.size() + prog->declarations.size();
    size_t idx = 0;

    for (IncludeDirective* inc : prog->includes) {
        pushNode(++idx == total);
        inc->accept(this);
        popNode();
    }
    for (UsingDirective* usg : prog->usings) {
        pushNode(++idx == total);
        usg->accept(this);
        popNode();
    }
    for (ASTNode* decl : prog->declarations) {
        pushNode(++idx == total);
        decl->accept(this);
        popNode();
    }
}
