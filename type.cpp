#include <sstream>
#include "type.h"

using namespace std;

Type::Type()
    : kind(VOID), ptrLevel(0), isRef(false), recordName(""),
      intValue(0), doubleValue(0.0), boolValue(false), charValue('\0'),
      stringValue(""), isConstExpr(false) { }

Type::Type(Kind k, int ptr, bool ref)
    : kind(k), ptrLevel(ptr), isRef(ref), recordName(""),
      intValue(0), doubleValue(0.0), boolValue(false), charValue('\0'),
      stringValue(""), isConstExpr(false) { }

Type::Type(int val)
    : kind(INT), ptrLevel(0), isRef(false), recordName(""),
      intValue(val), doubleValue(val), boolValue(val != 0), charValue((char)val),
      stringValue(to_string(val)), isConstExpr(true) { }

Type::Type(double val, bool isFloat)
    : kind(isFloat ? FLOAT : DOUBLE), ptrLevel(0), isRef(false), recordName(""),
      intValue((int)val), doubleValue(val), boolValue(val != 0.0), charValue('\0'),
      stringValue(to_string(val)), isConstExpr(true) { }

Type::Type(bool val)
    : kind(BOOL), ptrLevel(0), isRef(false), recordName(""),
      intValue(val ? 1 : 0), doubleValue(val ? 1.0 : 0.0), boolValue(val), charValue('\0'),
      stringValue(val ? "true" : "false"), isConstExpr(true) { }

Type::Type(char val)
    : kind(CHAR), ptrLevel(0), isRef(false), recordName(""),
      intValue((int)val), doubleValue(0.0), boolValue(val != '\0'), charValue(val),
      stringValue(string(1, val)), isConstExpr(true) { }

Type::Type(const string& str)
    : kind(STRING), ptrLevel(0), isRef(false), recordName(""),
      intValue(0), doubleValue(0.0), boolValue(true), charValue('\0'),
      stringValue(str), isConstExpr(true) { }

Type Type::makeRecord(const string& name, int ptr, bool ref) {
    Type t(RECORD, ptr, ref);
    t.recordName = name;
    return t;
}

Type Type::makeError() {
    return Type(ERROR);
}

bool Type::isVoid() const      { return kind == VOID && ptrLevel == 0; }
bool Type::isInt() const       { return kind == INT && ptrLevel == 0; }
bool Type::isFloat() const     { return kind == FLOAT && ptrLevel == 0; }
bool Type::isDouble() const    { return kind == DOUBLE && ptrLevel == 0; }
bool Type::isBool() const      { return kind == BOOL && ptrLevel == 0; }
bool Type::isChar() const      { return kind == CHAR && ptrLevel == 0; }
bool Type::isString() const    { return kind == STRING && ptrLevel == 0; }
bool Type::isRecord() const    { return kind == RECORD && ptrLevel == 0; }
bool Type::isPointer() const   { return ptrLevel > 0; }
bool Type::isReference() const { return isRef; }
bool Type::isError() const     { return kind == ERROR; }

bool Type::isNumeric() const {
    if (ptrLevel > 0) return false;
    return kind == INT || kind == FLOAT || kind == DOUBLE || kind == CHAR || kind == BOOL;
}

bool Type::isCompatibleWith(const Type& other) const {
    if (isError() || other.isError()) return false;
    if (*this == other) return true;

    // Conversiones numéricas implícitas permitidas en C++
    if (isNumeric() && other.isNumeric()) return true;

    // Asignación de nullptr a cualquier puntero
    if (isPointer() && other.kind == INT && other.intValue == 0 && other.isConstExpr) return true;
    if (isPointer() && other.isPointer() && other.kind == VOID) return true; // void* en C++ permisivo
    if (kind == VOID && ptrLevel > 0 && other.isPointer()) return true;

    return false;
}

bool Type::operator==(const Type& other) const {
    if (kind != other.kind) return false;
    if (ptrLevel != other.ptrLevel) return false;
    if (isRef != other.isRef) return false;
    if (kind == RECORD && recordName != other.recordName) return false;
    return true;
}

bool Type::operator!=(const Type& other) const {
    return !(*this == other);
}

string Type::toString() const {
    string res = "";
    switch (kind) {
        case VOID:   res = "void"; break;
        case INT:    res = "int"; break;
        case FLOAT:  res = "float"; break;
        case DOUBLE: res = "double"; break;
        case BOOL:   res = "bool"; break;
        case CHAR:   res = "char"; break;
        case STRING: res = "string"; break;
        case RECORD: res = recordName.empty() ? "record" : recordName; break;
        case ERROR:  res = "<error-type>"; break;
    }
    for (int i = 0; i < ptrLevel; i++) res += "*";
    if (isRef) res += "&";
    for (int dim : arrayDims) {
        res += "[" + (dim > 0 ? to_string(dim) : "") + "]";
    }
    return res;
}

ostream& operator<<(ostream& outs, const Type& t) {
    return outs << t.toString();
}
