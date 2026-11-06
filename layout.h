#ifndef LAYOUT_H
#define LAYOUT_H

#include <string>
#include <vector>
#include <unordered_map>
#include "type.h"

using namespace std;

class FunDecl;

// Donde vive una variable en tiempo de ejecucion
enum class Storage {
    NONE,
    LOCAL,   // offset negativo respecto a %rbp
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
    vector<int> stringOffsets; // offsets relativos de los objetos string contenidos
};

struct FieldInfo {
    string name;
    Type type;
    int offset = 0;
    int size = 0;
    int align = 1;
    bool isPrivate = false;
};

struct MethodInfo {
    FunDecl* fun = nullptr;
    bool isPrivate = false;
};

struct RecordLayout {
    string name;
    int size = 0;
    int align = 1;
    bool complete = false;
    vector<FieldInfo> fields;
    unordered_map<string, MethodInfo> methods;
    vector<int> stringOffsets;

    const FieldInfo* findField(const string& n) const {
        for (const FieldInfo& f : fields) {
            if (f.name == n) return &f;
        }
        return nullptr;
    }
};

struct FunctionLayout {
    string label;
    int frameSize = 0;            // multiplo de 16
    vector<VarInfo> params;       // en un metodo, params[0] es this
    Type returnType;
    bool isMethod = false;
    string ownerRecord;
    vector<int> stringSlots;      // offsets de los objetos string locales
    int retSaveOffset = 0;        // reserva para conservar el retorno mientras se liberan strings
};

#endif // LAYOUT_H
