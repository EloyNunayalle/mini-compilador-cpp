#ifndef TYPE_H
#define TYPE_H

#include <string>
#include <vector>
#include <ostream>

using namespace std;

// Representación de un tipo en el compilador (básico, puntero, referencia, arreglo, clase/struct)
class Type {
public:
    enum Kind {
        VOID,
        INT,
        FLOAT,
        DOUBLE,
        BOOL,
        CHAR,
        STRING,
        RECORD,
        ERROR
    };

    Kind kind;
    int ptrLevel;          // 0 = valor directo, 1 = T*, 2 = T**, etc.
    bool isRef;            // true si es T&
    string recordName;     // Nombre si es tipo RECORD / clase / struct
    vector<int> arrayDims; // Dimensiones de arreglo si son constantes

    // Valores calculados en tiempo de compilación para evaluación constante / optimizaciones
    int intValue;
    double doubleValue;
    bool boolValue;
    char charValue;
    string stringValue;
    bool isConstExpr;

    Type();                                     // VOID
    Type(Kind k, int ptr = 0, bool ref = false);
    Type(int val);                              // INT literal
    Type(double val, bool isFloat = false);     // DOUBLE / FLOAT literal
    Type(bool val);                             // BOOL literal
    Type(char val);                             // CHAR literal
    Type(const string& str);                    // STRING literal
    static Type makeRecord(const string& name, int ptr = 0, bool ref = false);
    static Type makeError();

    bool isVoid() const;
    bool isInt() const;
    bool isFloat() const;
    bool isDouble() const;
    bool isBool() const;
    bool isChar() const;
    bool isString() const;
    bool isRecord() const;
    bool isPointer() const;
    bool isReference() const;
    bool isNumeric() const;
    bool isError() const;

    bool isCompatibleWith(const Type& other) const;
    bool operator==(const Type& other) const;
    bool operator!=(const Type& other) const;

    string toString() const;
    friend ostream& operator<<(ostream& outs, const Type& t);
};

#endif // TYPE_H
