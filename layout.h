#ifndef LAYOUT_H
#define LAYOUT_H

#include <string>
#include <vector>
#include <unordered_map>
#include "type.h"

using namespace std;

class FunDecl;
class Exp;

// Donde vive una variable en tiempo de ejecucion
enum class Storage {
    NONE,
    LOCAL,   // offset respecto a %rbp (negativo para locales, positivo para parametros en pila)
    GLOBAL,  // etiqueta en .bss, se accede con etiqueta(%rip)
    FIELD    // offset dentro del record, via this implicito
};

// Resultado del analisis de una variable, parametro o campo
struct VarInfo {
    Type type;
    int offset = 0;
    int size = 0;
    int align = 1;
    Storage storage = Storage::NONE;
    string label;
    bool indirect = false;   // el slot guarda un puntero al objeto (parametro por referencia invisible)
    bool hasConstVal = false;   // constante entera conocida en compilacion
    long long constVal = 0;
};

struct FieldInfo {
    string name;
    Type type;
    int offset = 0;
    int size = 0;
    int align = 1;
    bool isPrivate = false;
    Exp* classInit = nullptr;    // inicializador en la clase, si existe (no es dueno)
    int line = 0;
    int col = 0;
};

struct MethodInfo {
    FunDecl* fun = nullptr;
    bool isPrivate = false;
};

// Clasificacion System V AMD64 de un eightbyte
enum class AbiClass { NO_CLASS, INTEGER, SSE, MEMORY };

// Como viaja un parametro o el valor de retorno
enum class Pass {
    INT_REG,         // escalar entero, puntero, referencia o this en registro entero
    SSE_REG,         // float o double en registro XMM
    AGG_REGS,        // record trivial pequeno repartido en registros por eightbytes
    STACK,           // en la pila del llamador (escalar o record clasificado MEMORY)
    INVISIBLE_REF,   // record no trivial: se pasa un puntero a una copia
    EMPTY,           // record vacio: no consume registros ni pila
    VOID_RET,
    HIDDEN_PTR       // retorno por puntero oculto (primer registro entero)
};

struct AbiLoc {
    Pass pass = Pass::INT_REG;
    int size = 0;                 // bytes del objeto
    int neight = 0;               // eightbytes en registros (AGG_REGS)
    AbiClass cls[2] = {AbiClass::NO_CLASS, AbiClass::NO_CLASS};
    int reg[2] = {-1, -1};        // parametros: posicion absoluta en rdi..r9 o xmm0..xmm7
                                  // retorno: 0 o 1 dentro de (rax, rdx) o (xmm0, xmm1)
    int stackOffset = 0;          // offset dentro del area de argumentos en pila
    int stackBytes = 0;           // bytes que ocupa en pila (multiplo de 8)
};

// Informacion ABI de un tipo record o string
struct RecordAbi {
    bool trivialForCalls = true;
    bool empty = false;
    bool memory = false;          // clasificado MEMORY (mas de 16 bytes u otra razon)
    int neight = 0;
    AbiClass cls[2] = {AbiClass::NO_CLASS, AbiClass::NO_CLASS};
};

// Disponibilidad de una operacion especial
enum class Avail {
    NONE,        // no existe (por ejemplo, ctor por defecto suprimido por otro ctor)
    USER,        // declarada por el usuario
    IMPLICIT,    // generada por el compilador
    DELETED      // implicita pero no utilizable
};

struct RecordLayout {
    string name;
    int size = 0;
    int align = 1;
    bool complete = false;
    vector<FieldInfo> fields;
    unordered_map<string, MethodInfo> methods;
    bool hasString = false;       // contiene un string, directa o indirectamente

    Avail defaultCtorAvail = Avail::NONE;
    Avail paramCtorAvail = Avail::NONE;
    Avail copyCtorAvail = Avail::IMPLICIT;
    Avail copyAssignAvail = Avail::IMPLICIT;
    Avail dtorAvail = Avail::IMPLICIT;
    FunDecl* defaultCtor = nullptr;   // null si es trivial o no existe
    FunDecl* paramCtor = nullptr;
    FunDecl* copyCtor = nullptr;      // null si la copia es bit a bit
    FunDecl* copyAssign = nullptr;    // null si la asignacion es bit a bit
    FunDecl* dtor = nullptr;          // null si no hay nada que destruir

    bool defaultTrivial = true;   // construir por defecto no ejecuta codigo
    bool copyTrivial = true;      // copiar es bit a bit
    bool assignTrivial = true;
    bool dtorTrivial = true;
    bool isAggregate = true;
    RecordAbi abi;

    const FieldInfo* findField(const string& n) const {
        for (const FieldInfo& f : fields) {
            if (f.name == n) return &f;
        }
        return nullptr;
    }
    int fieldIndex(const string& n) const {
        for (size_t i = 0; i < fields.size(); i++) {
            if (fields[i].name == n) return static_cast<int>(i);
        }
        return -1;
    }
};

struct FunctionLayout {
    string label;
    int frameSize = 0;            // multiplo de 16
    vector<VarInfo> params;       // en un metodo, params[0] es this
    Type returnType;
    bool isMethod = false;
    string ownerRecord;

    vector<AbiLoc> paramAbi;      // paralelo a params
    AbiLoc retAbi;
    int stackArgBytes = 0;        // bytes de argumentos pasados en pila
    int retPtrOffset = 0;         // slot donde el callee guarda el puntero oculto de retorno
    int retValOffset = 0;         // slot de 16 bytes para retorno de records por registros
};

#endif // LAYOUT_H
