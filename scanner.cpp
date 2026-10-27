#include <iostream>
#include <fstream>
#include <cctype>
#include <unordered_map>
#include "scanner.h"

using namespace std;

static const unordered_map<string, Token::Type> KEYWORDS = {
    {"int",       Token::KW_INT},
    {"float",     Token::KW_FLOAT},
    {"double",    Token::KW_DOUBLE},
    {"bool",      Token::KW_BOOL},
    {"char",      Token::KW_CHAR},
    {"void",      Token::KW_VOID},
    {"if",        Token::KW_IF},
    {"else",      Token::KW_ELSE},
    {"while",     Token::KW_WHILE},
    {"for",       Token::KW_FOR},
    {"do",        Token::KW_DO},
    {"break",     Token::KW_BREAK},
    {"continue",  Token::KW_CONTINUE},
    {"return",    Token::KW_RETURN},
    {"class",     Token::KW_CLASS},
    {"struct",    Token::KW_STRUCT},
    {"public",    Token::KW_PUBLIC},
    {"private",   Token::KW_PRIVATE},
    {"new",       Token::KW_NEW},
    {"delete",    Token::KW_DELETE},
    {"true",      Token::KW_TRUE},
    {"false",     Token::KW_FALSE},
    {"nullptr",   Token::KW_NULLPTR},
    {"include",   Token::KW_INCLUDE},
    {"using",     Token::KW_USING},
    {"namespace", Token::KW_NAMESPACE},
    {"iostream",  Token::LIB_IOSTREAM},
    {"string",    Token::LIB_STRING},
    {"cmath",     Token::LIB_CMATH}
};

Scanner::Scanner(const char* in_s)
    : input(in_s ? in_s : ""), current(0), line(1), col(1) { }

Scanner::Scanner(const string& in_s)
    : input(in_s), current(0), line(1), col(1) { }

Scanner::~Scanner() { }

bool Scanner::isAtEnd() const {
    return current >= input.length();
}

char Scanner::peek() const {
    if (isAtEnd()) return '\0';
    return input[current];
}

char Scanner::peekNext() const {
    if (current + 1 >= input.length()) return '\0';
    return input[current + 1];
}

char Scanner::advance() {
    if (isAtEnd()) return '\0';
    char c = input[current++];
    if (c == '\n') {
        line++;
        col = 1;
    } else {
        col++;
    }
    return c;
}

void Scanner::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            // Comentario de una línea // ...
            advance(); // consume '/'
            advance(); // consume '/'
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peekNext() == '*') {
            // Comentario de múltiples líneas /* ... */
            advance(); // consume '/'
            advance(); // consume '*'
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); // consume '*'
                    advance(); // consume '/'
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token* Scanner::nextToken() {
    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return new Token(Token::END, line, col);
    }

    int startLine = line;
    int startCol = col;
    char c = advance();

    // Identificadores y palabras clave
    if (isalpha(c) || c == '_') {
        string text(1, c);
        while (!isAtEnd() && (isalnum(peek()) || peek() == '_')) {
            text += advance();
        }
        auto it = KEYWORDS.find(text);
        if (it != KEYWORDS.end()) {
            return new Token(it->second, text, startLine, startCol);
        }
        return new Token(Token::ID, text, startLine, startCol);
    }

    // Literales numéricos (enteros y reales)
    if (isdigit(c)) {
        string text(1, c);
        while (!isAtEnd() && isdigit(peek())) {
            text += advance();
        }
        if (peek() == '.' && isdigit(peekNext())) {
            text += advance(); // el punto '.'
            while (!isAtEnd() && isdigit(peek())) {
                text += advance();
            }
            return new Token(Token::REAL, text, startLine, startCol);
        }
        return new Token(Token::NUM, text, startLine, startCol);
    }

    // Literales de cadena "..."
    if (c == '"') {
        string text = "";
        while (!isAtEnd() && peek() != '"') {
            if (peek() == '\\') {
                advance();
                if (!isAtEnd()) {
                    char esc = advance();
                    if (esc == 'n') text += '\n';
                    else if (esc == 't') text += '\t';
                    else if (esc == '\\') text += '\\';
                    else if (esc == '"') text += '"';
                    else text += esc;
                }
            } else {
                text += advance();
            }
        }
        if (isAtEnd()) {
            return new Token(Token::ERR, "Cadena sin cerrar", startLine, startCol);
        }
        advance(); // consume closing '"'
        return new Token(Token::STRING_LITERAL, text, startLine, startCol);
    }

    // Literales de carácter '...'
    if (c == '\'') {
        string text = "";
        if (!isAtEnd() && peek() == '\\') {
            advance();
            if (!isAtEnd()) {
                char esc = advance();
                if (esc == 'n') text += '\n';
                else if (esc == 't') text += '\t';
                else if (esc == '\\') text += '\\';
                else if (esc == '\'') text += '\'';
                else text += esc;
            }
        } else if (!isAtEnd() && peek() != '\'') {
            text += advance();
        }
        if (isAtEnd() || peek() != '\'') {
            return new Token(Token::ERR, "Carácter sin cerrar", startLine, startCol);
        }
        advance(); // consume closing '\''
        return new Token(Token::CHAR_LITERAL, text, startLine, startCol);
    }

    // Operadores compuestos y simples
    switch (c) {
        case '+':
            if (peek() == '+') { advance(); return new Token(Token::INC, "++", startLine, startCol); }
            return new Token(Token::PLUS, '+', startLine, startCol);

        case '-':
            if (peek() == '-') { advance(); return new Token(Token::DEC, "--", startLine, startCol); }
            if (peek() == '>') { advance(); return new Token(Token::ARROW, "->", startLine, startCol); }
            return new Token(Token::MINUS, '-', startLine, startCol);

        case '*':
            return new Token(Token::MUL, '*', startLine, startCol);

        case '/':
            return new Token(Token::DIV, '/', startLine, startCol);

        case '%':
            return new Token(Token::MOD, '%', startLine, startCol);

        case '=':
            if (peek() == '=') { advance(); return new Token(Token::EQ, "==", startLine, startCol); }
            return new Token(Token::ASSIGN, '=', startLine, startCol);

        case '!':
            if (peek() == '=') { advance(); return new Token(Token::NE, "!=", startLine, startCol); }
            return new Token(Token::NOT, '!', startLine, startCol);

        case '<':
            if (peek() == '<') { advance(); return new Token(Token::LSHIFT, "<<", startLine, startCol); }
            if (peek() == '=') { advance(); return new Token(Token::LE, "<=", startLine, startCol); }
            return new Token(Token::LT, '<', startLine, startCol);

        case '>':
            if (peek() == '>') { advance(); return new Token(Token::RSHIFT, ">>", startLine, startCol); }
            if (peek() == '=') { advance(); return new Token(Token::GE, ">=", startLine, startCol); }
            return new Token(Token::GT, '>', startLine, startCol);

        case '&':
            if (peek() == '&') { advance(); return new Token(Token::AND, "&&", startLine, startCol); }
            return new Token(Token::AMP, '&', startLine, startCol);

        case '|':
            if (peek() == '|') { advance(); return new Token(Token::OR, "||", startLine, startCol); }
            return new Token(Token::BIT_OR, '|', startLine, startCol);

        case '^':
            return new Token(Token::BIT_XOR, '^', startLine, startCol);

        case '~':
            return new Token(Token::BIT_NOT, '~', startLine, startCol);

        case ':':
            if (peek() == ':') { advance(); return new Token(Token::COLON_COLON, "::", startLine, startCol); }
            return new Token(Token::COLON, ':', startLine, startCol);

        case '.':
            return new Token(Token::DOT, '.', startLine, startCol);

        case '(':
            return new Token(Token::LPAREN, '(', startLine, startCol);
        case ')':
            return new Token(Token::RPAREN, ')', startLine, startCol);
        case '[':
            return new Token(Token::LBRACKET, '[', startLine, startCol);
        case ']':
            return new Token(Token::RBRACKET, ']', startLine, startCol);
        case '{':
            return new Token(Token::LBRACE, '{', startLine, startCol);
        case '}':
            return new Token(Token::RBRACE, '}', startLine, startCol);
        case ';':
            return new Token(Token::SEMICOL, ';', startLine, startCol);
        case ',':
            return new Token(Token::COMMA, ',', startLine, startCol);
        case '#':
            return new Token(Token::HASH, '#', startLine, startCol);

        default:
            return new Token(Token::ERR, string(1, c), startLine, startCol);
    }
}

vector<Token*> Scanner::tokenizeAll() {
    vector<Token*> tokens;
    Token* t = nullptr;
    do {
        t = nextToken();
        tokens.push_back(t);
    } while (t->type != Token::END && t->type != Token::ERR);
    return tokens;
}

void ejecutar_scanner(Scanner* scanner, const string& InputFile) {
    string OutputFileName = InputFile;
    size_t pos = OutputFileName.find_last_of(".");
    if (pos != string::npos) {
        OutputFileName = OutputFileName.substr(0, pos);
    }
    OutputFileName += "_tokens.txt";

    ofstream outFile(OutputFileName);
    if (!outFile.is_open()) {
        cerr << "Error: no se pudo abrir el archivo de tokens " << OutputFileName << endl;
        return;
    }

    outFile << "=== SCANNER TOKENS ===" << endl;
    Token* tok = nullptr;
    do {
        tok = scanner->nextToken();
        outFile << *tok << endl;
        if (tok->type == Token::END || tok->type == Token::ERR) break;
        delete tok;
    } while (true);
    if (tok) delete tok;
}
