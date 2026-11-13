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
        OSTREAM,    // resultado de cout << x (solo para encadenar)
        ISTREAM,    // resultado de cin >> x
        ENDL,       // manipulador endl
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

    // Calificadores const por nivel: el bit 0 es el tipo base, el bit k (k >= 1) es el
    // k-esimo puntero. El objeto que nombra el tipo es const si lo es el bit ptrLevel.
    unsigned constBits;

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
    bool isArray() const;
    bool isIntegral() const;   // int, char o bool sin puntero ni arreglo
    bool isStream() const;

    Type elementType() const;  // quita la dimension mas externa del arreglo
    Type withoutRef() const;
    bool sameBase(const Type& other) const; // mismo tipo base y nivel de puntero, ignora ref y dimensiones

    bool constAt(int level) const { return (constBits >> level) & 1u; }
    bool isConstObj() const { return constAt(ptrLevel); }   // el objeto en si es const
    bool isPointeeConst() const { return ptrLevel > 0 && constAt(ptrLevel - 1); }
    Type withTopConst(bool c = true) const;     // marca el objeto como const
    Type pointee() const;                       // quita un nivel de puntero
    Type addressOf() const;                     // agrega un nivel de puntero
    Type withoutConst() const;                  // quita el const del objeto
    bool sameConstBits(const Type& other) const { return constBits == other.constBits; }

    bool operator==(const Type& other) const;
    bool operator!=(const Type& other) const;

    string toString() const;
    friend ostream& operator<<(ostream& outs, const Type& t);
};

#endif // TYPE_H
