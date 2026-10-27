#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <ostream>

using namespace std;

// Unidad léxica producida por el scanner: tipo, lexema y posición (línea, columna)
class Token {
public:
    enum Type {
        // Palabras clave de tipos
        KW_INT,
        KW_FLOAT,
        KW_DOUBLE,
        KW_BOOL,
        KW_CHAR,
        KW_VOID,

        // Palabras clave de control de flujo
        KW_IF,
        KW_ELSE,
        KW_WHILE,
        KW_FOR,
        KW_DO,
        KW_BREAK,
        KW_CONTINUE,
        KW_RETURN,

        // Clases, structs y acceso
        KW_CLASS,
        KW_STRUCT,
        KW_PUBLIC,
        KW_PRIVATE,

        // Memoria
        KW_NEW,
        KW_DELETE,

        // Literales constantes
        KW_TRUE,
        KW_FALSE,
        KW_NULLPTR,

        // Directivas y namespaces
        KW_INCLUDE,
        KW_USING,
        KW_NAMESPACE,

        // Librerías reconocidas
        LIB_IOSTREAM,
        LIB_STRING,
        LIB_CMATH,

        // Identificadores y literales
        ID,
        NUM,            // Enteros
        REAL,           // Flotantes / dobles
        CHAR_LITERAL,   // 'c'
        STRING_LITERAL, // "cadena"

        // Operadores aritméticos
        PLUS,           // +
        MINUS,          // -
        MUL,            // *
        DIV,            // /
        MOD,            // %
        INC,            // ++
        DEC,            // --

        // Operadores relacionales y de igualdad
        ASSIGN,         // =
        EQ,             // ==
        NE,             // !=
        LT,             // <
        LE,             // <=
        GT,             // >
        GE,             // >=

        // Operadores lógicos y de bits
        AND,            // &&
        OR,             // ||
        NOT,            // !
        AMP,            // & (dirección o AND a nivel de bits o referencia)
        BIT_OR,         // |
        BIT_XOR,        // ^
        BIT_NOT,        // ~
        LSHIFT,         // <<
        RSHIFT,         // >>

        // Acceso a miembros y resolución de ámbito
        DOT,            // .
        ARROW,          // ->
        COLON_COLON,    // ::
        COLON,          // :

        // Delimitadores y puntuación
        LPAREN,         // (
        RPAREN,         // )
        LBRACKET,       // [
        RBRACKET,       // ]
        LBRACE,         // {
        RBRACE,         // }
        SEMICOL,        // ;
        COMMA,          // ,
        HASH,           // #

        // Control
        ERR,            // Error léxico
        END             // Fin de archivo (EOF)
    };

    Type type;
    string text;
    int line;
    int col;

    Token(Type type, int line = 1, int col = 1);
    Token(Type type, char c, int line = 1, int col = 1);
    Token(Type type, const string& text, int line = 1, int col = 1);

    static string typeName(Type t);

    friend ostream& operator<<(ostream& outs, const Token& tok);
    friend ostream& operator<<(ostream& outs, const Token* tok);
};

#endif // TOKEN_H
