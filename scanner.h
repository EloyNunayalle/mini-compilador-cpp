#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include <vector>
#include "token.h"

using namespace std;

// Analizador léxico: recorre la entrada en C++ y produce tokens
class Scanner {
private:
    string input;
    size_t current;
    int line;
    int col;

    char peek() const;
    char peekNext() const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespaceAndComments();

public:
    Scanner(const char* in_s);
    Scanner(const string& in_s);
    Token* nextToken();
    vector<Token*> tokenizeAll();
    ~Scanner();
};

void ejecutar_scanner(Scanner* scanner, const string& InputFile);

#endif // SCANNER_H
