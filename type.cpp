#include <sstream>
#include "type.h"

using namespace std;

Type::Type()
    : kind(VOID), ptrLevel(0), isRef(false), recordName(""),
      intValue(0), doubleValue(0.0), boolValue(false), charValue('\0'),
      stringValue(""), isConstExpr(false), constBits(0) { }

Type::Type(Kind k, int ptr, bool ref)
    : kind(k), ptrLevel(ptr), isRef(ref), recordName(""),
      intValue(0), doubleValue(0.0), boolValue(false), charValue('\0'),
      stringValue(""), isConstExpr(false), constBits(0) { }

Type::Type(int val)
    : kind(INT), ptrLevel(0), isRef(false), recordName(""),
      intValue(val), doubleValue(val), boolValue(val != 0), charValue((char)val),
      stringValue(to_string(val)), isConstExpr(true), constBits(0) { }

Type::Type(double val, bool isFloat)
    : kind(isFloat ? FLOAT : DOUBLE), ptrLevel(0), isRef(false), recordName(""),
      intValue((int)val), doubleValue(val), boolValue(val != 0.0), charValue('\0'),
      stringValue(to_string(val)), isConstExpr(true), constBits(0) { }

Type::Type(bool val)
    : kind(BOOL), ptrLevel(0), isRef(false), recordName(""),
      intValue(val ? 1 : 0), doubleValue(val ? 1.0 : 0.0), boolValue(val), charValue('\0'),
      stringValue(val ? "true" : "false"), isConstExpr(true), constBits(0) { }

Type::Type(char val)
    : kind(CHAR), ptrLevel(0), isRef(false), recordName(""),
      intValue((int)val), doubleValue(0.0), boolValue(val != '\0'), charValue(val),
      stringValue(string(1, val)), isConstExpr(true), constBits(0) { }

Type::Type(const string& str)
    : kind(STRING), ptrLevel(0), isRef(false), recordName(""),
      intValue(0), doubleValue(0.0), boolValue(true), charValue('\0'),
      stringValue(str), isConstExpr(true), constBits(0) { }

Type Type::makeRecord(const string& name, int ptr, bool ref) {
    Type t(RECORD, ptr, ref);
    t.recordName = name;
    return t;
}

Type Type::makeError() {
    return Type(ERROR);
}

bool Type::isVoid() const      { return kind == VOID && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isInt() const       { return kind == INT && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isFloat() const     { return kind == FLOAT && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isDouble() const    { return kind == DOUBLE && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isBool() const      { return kind == BOOL && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isChar() const      { return kind == CHAR && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isString() const    { return kind == STRING && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isRecord() const    { return kind == RECORD && ptrLevel == 0 && arrayDims.empty(); }
bool Type::isPointer() const   { return ptrLevel > 0 && arrayDims.empty(); }
bool Type::isReference() const { return isRef; }
bool Type::isError() const     { return kind == ERROR; }
bool Type::isArray() const     { return !arrayDims.empty(); }
bool Type::isStream() const    { return (kind == OSTREAM || kind == ISTREAM) && ptrLevel == 0; }

bool Type::isNumeric() const {
    if (ptrLevel > 0 || !arrayDims.empty()) return false;
    return kind == INT || kind == FLOAT || kind == DOUBLE || kind == CHAR || kind == BOOL;
}

bool Type::isIntegral() const {
    return isInt() || isChar() || isBool();
}

Type Type::elementType() const {
    Type t = *this;
    t.isRef = false;
    if (!t.arrayDims.empty()) t.arrayDims.erase(t.arrayDims.begin());
    return t;
}

Type Type::withTopConst(bool c) const {
    Type t = *this;
    if (c) t.constBits |= (1u << ptrLevel);
    else t.constBits &= ~(1u << ptrLevel);
    return t;
}

Type Type::withoutConst() const {
    return withTopConst(false);
}

Type Type::pointee() const {
    Type t = *this;
    t.arrayDims.clear();
    t.isRef = false;
    if (t.ptrLevel > 0) t.ptrLevel--;
    t.constBits &= (1u << (t.ptrLevel + 1)) - 1u;
    return t;
}

Type Type::addressOf() const {
    Type t = *this;
    t.isRef = false;
    t.ptrLevel++;
    t.constBits &= ~(1u << t.ptrLevel);
    return t;
}

Type Type::withoutRef() const {
    Type t = *this;
    t.isRef = false;
    return t;
}

bool Type::sameBase(const Type& other) const {
    if (kind != other.kind || ptrLevel != other.ptrLevel) return false;
    if (kind == RECORD && recordName != other.recordName) return false;
    return true;
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
    string res = constAt(0) ? "const " : "";
    switch (kind) {
        case VOID:   res = "void"; break;
        case INT:    res = "int"; break;
        case FLOAT:  res = "float"; break;
        case DOUBLE: res = "double"; break;
        case BOOL:   res = "bool"; break;
        case CHAR:   res = "char"; break;
        case STRING: res = "string"; break;
        case RECORD: res = recordName.empty() ? "record" : recordName; break;
        case OSTREAM: res = "ostream"; break;
        case ISTREAM: res = "istream"; break;
        case ENDL:   res = "endl"; break;
        case ERROR:  res = "<error-type>"; break;
    }
    for (int i = 0; i < ptrLevel; i++) {
        res += "*";
        if (constAt(i + 1)) res += " const";
    }
    if (isRef) res += "&";
    for (int dim : arrayDims) {
        res += "[" + (dim > 0 ? to_string(dim) : "") + "]";
    }
    return res;
}

ostream& operator<<(ostream& outs, const Type& t) {
    return outs << t.toString();
}
