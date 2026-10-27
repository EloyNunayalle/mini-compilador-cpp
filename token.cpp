#include <iostream>
#include "token.h"

using namespace std;

Token::Token(Type type, int line, int col)
    : type(type), text(""), line(line), col(col) { }

Token::Token(Type type, char c, int line, int col)
    : type(type), text(string(1, c)), line(line), col(col) { }

Token::Token(Type type, const string& text, int line, int col)
    : type(type), text(text), line(line), col(col) { }

string Token::typeName(Type t) {
    switch (t) {
        case KW_INT:         return "KW_INT";
        case KW_FLOAT:       return "KW_FLOAT";
        case KW_DOUBLE:      return "KW_DOUBLE";
        case KW_BOOL:        return "KW_BOOL";
        case KW_CHAR:        return "KW_CHAR";
        case KW_VOID:        return "KW_VOID";
        case KW_IF:          return "KW_IF";
        case KW_ELSE:        return "KW_ELSE";
        case KW_WHILE:       return "KW_WHILE";
        case KW_FOR:         return "KW_FOR";
        case KW_DO:          return "KW_DO";
        case KW_BREAK:       return "KW_BREAK";
        case KW_CONTINUE:    return "KW_CONTINUE";
        case KW_RETURN:      return "KW_RETURN";
        case KW_CLASS:       return "KW_CLASS";
        case KW_STRUCT:      return "KW_STRUCT";
        case KW_PUBLIC:      return "KW_PUBLIC";
        case KW_PRIVATE:     return "KW_PRIVATE";
        case KW_NEW:         return "KW_NEW";
        case KW_DELETE:      return "KW_DELETE";
        case KW_TRUE:        return "KW_TRUE";
        case KW_FALSE:       return "KW_FALSE";
        case KW_NULLPTR:     return "KW_NULLPTR";
        case KW_INCLUDE:     return "KW_INCLUDE";
        case KW_USING:       return "KW_USING";
        case KW_NAMESPACE:   return "KW_NAMESPACE";
        case LIB_IOSTREAM:   return "LIB_IOSTREAM";
        case LIB_STRING:     return "LIB_STRING";
        case LIB_CMATH:      return "LIB_CMATH";
        case ID:             return "ID";
        case NUM:            return "NUM";
        case REAL:           return "REAL";
        case CHAR_LITERAL:   return "CHAR_LITERAL";
        case STRING_LITERAL: return "STRING_LITERAL";
        case PLUS:           return "PLUS";
        case MINUS:          return "MINUS";
        case MUL:            return "MUL";
        case DIV:            return "DIV";
        case MOD:            return "MOD";
        case INC:            return "INC";
        case DEC:            return "DEC";
        case ASSIGN:         return "ASSIGN";
        case EQ:             return "EQ";
        case NE:             return "NE";
        case LT:             return "LT";
        case LE:             return "LE";
        case GT:             return "GT";
        case GE:             return "GE";
        case AND:            return "AND";
        case OR:             return "OR";
        case NOT:            return "NOT";
        case AMP:            return "AMP";
        case BIT_OR:         return "BIT_OR";
        case BIT_XOR:        return "BIT_XOR";
        case BIT_NOT:        return "BIT_NOT";
        case LSHIFT:         return "LSHIFT";
        case RSHIFT:         return "RSHIFT";
        case DOT:            return "DOT";
        case ARROW:          return "ARROW";
        case COLON_COLON:    return "COLON_COLON";
        case COLON:          return "COLON";
        case LPAREN:         return "LPAREN";
        case RPAREN:         return "RPAREN";
        case LBRACKET:       return "LBRACKET";
        case RBRACKET:       return "RBRACKET";
        case LBRACE:         return "LBRACE";
        case RBRACE:         return "RBRACE";
        case SEMICOL:        return "SEMICOL";
        case COMMA:          return "COMMA";
        case HASH:           return "HASH";
        case ERR:            return "ERR";
        case END:            return "END";
    }
    return "UNKNOWN";
}

ostream& operator<<(ostream& outs, const Token& tok) {
    outs << "TOKEN(" << Token::typeName(tok.type)
         << ", \"" << tok.text << "\", L" << tok.line << ":" << tok.col << ")";
    return outs;
}

ostream& operator<<(ostream& outs, const Token* tok) {
    if (!tok) return outs << "TOKEN(NULL)";
    return outs << *tok;
}
