# Compilador C++ (Subconjunto EBNF)

Compilador implementado en **C++17** para un subconjunto formal del lenguaje C++ definido en [`gramatica.ebnf`](gramatica.ebnf). El proyecto cubre el análisis léxico, análisis sintáctico con generación de Árbol de Sintaxis Abstracta (AST) bajo el patrón Visitor, y análisis semántico con comprobación de tipos y control de ámbitos.

---

## 1. Arquitectura y Módulos

El código está estructurado de manera modular y desacoplada:

| Módulo | Archivos | Responsabilidad |
|---|---|---|
| **Léxico** | `token.h`, `token.cpp`<br>`scanner.h`, `scanner.cpp` | Convierte el flujo de caracteres en tokens tipados (`KW_IF`, `ID`, `NUM`, etc.), rastrea la ubicación exacta (`línea`, `columna`) y descarta comentarios (`//` y `/* ... */`). |
| **AST** | `ast.h`, `ast.cpp` | Define la jerarquía orientada a objetos de los nodos del árbol sintáctico (declaraciones, clases/structs, sentencias y expresiones con 14 niveles de precedencia) con soporte para el patrón Visitor. |
| **Sintáctico** | `parser.h`, `parser.cpp` | Parser descendente recursivo que valida la gramática EBNF, construye el AST y resuelve ambigüedades como el *dangling else* asociándolo al `if` más cercano. |
| **Tipos** | `type.h`, `type.cpp` | Modela tipos primitivos (`int`, `float`, `double`, `bool`, `char`, `void`), punteros (`*`), referencias (`&`), arreglos y registros (`struct`/`class`). |
| **Ámbitos** | `environment.h` | Tabla de símbolos jerárquica con pila de niveles (`ribs`) para soportar ámbitos léxicos (scopes). |
| **Visitor Base** | `visitor.h` | Interfaz abstracta para recorrer nodos del AST separando la estructura de los algoritmos. |
| **Visualizador** | `print_visitor.h`, `print_visitor.cpp` | Visitor que recorre el AST e imprime una representación jerárquica en consola con caracteres de árbol (`├── `, `└── `). |
| **Semántico** | `type_checker.h`, `type_checker.cpp` | Visitor que realiza la comprobación de tipos, valida signaturas de funciones y detecta identificadores no declarados o redeclarados. |
| **Driver CLI** | `main.cpp` | Punto de entrada del compilador que coordina las fases según los argumentos de línea de comandos. |

---

## 2. Requisitos y Compilación

- Compilador de C++ con soporte C++17 (`g++` o `clang++`).
- `make` para compilación automatizada.
- `python3` (opcional, para correr la suite de pruebas).

### Compilar el proyecto:
```bash
make
```
Genera el binario ejecutable `./compiler`.

### Limpiar archivos generados:
```bash
make clean
```

---

## 3. Uso del Compilador

```bash
./compiler <archivo.cpp> [opciones]
```

### Opciones disponibles:
- `(sin opción)`: Ejecuta el análisis sintáctico y la comprobación semántica de tipos.
- `--scan`: Ejecuta únicamente el análisis léxico, listando los tokens con su tipo, lexema y posición.
- `--ast`: Muestra el Árbol de Sintaxis Abstracta (AST) formateado en la terminal.
- `--check`: Ejecuta el análisis semántico y comprobación de tipos detallada.
- `--all`: Ejecuta todas las fases, muestra el AST y exporta los archivos `<archivo>_tokens.txt` y `<archivo>_ast.txt`.

---

## 4. Demos de Funcionamiento

### Demo 1: Análisis Léxico (`--scan`)
Convierte el código fuente en tokens con coordenadas de línea y columna.

**Comando:**
```bash
./compiler inputs/01_arithmetic.cpp --scan
```

**Salida:**
```text
=== [FASE 1: ANÁLISIS LÉXICO] ===
  TOKEN(HASH, "#", L1:1)
  TOKEN(KW_INCLUDE, "include", L1:2)
  TOKEN(LT, "<", L1:10)
  TOKEN(LIB_IOSTREAM, "iostream", L1:11)
  TOKEN(GT, ">", L1:19)
  TOKEN(KW_USING, "using", L2:1)
  TOKEN(KW_NAMESPACE, "namespace", L2:7)
  TOKEN(ID, "std", L2:17)
  TOKEN(SEMICOL, ";", L2:20)
  TOKEN(KW_INT, "int", L4:1)
  TOKEN(ID, "main", L4:5)
  TOKEN(LPAREN, "(", L4:9)
  TOKEN(RPAREN, ")", L4:10)
  TOKEN(LBRACE, "{", L4:12)
  TOKEN(KW_INT, "int", L5:5)
  TOKEN(ID, "a", L5:9)
  TOKEN(ASSIGN, "=", L5:11)
  TOKEN(NUM, "10", L5:13)
  TOKEN(SEMICOL, ";", L5:15)
  ...
  TOKEN(END, "", L11:1)

Tokens guardados en inputs/01_arithmetic_tokens.txt
```

---

### Demo 2: Visualización del Árbol Sintáctico (`--ast`)
Muestra la estructura jerárquica del programa respetando la precedencia de operadores y estructuras de control.

**Comando:**
```bash
./compiler inputs/02_control_flow.cpp --ast
```

**Salida:**
```text
Program [L1:1]
├── #include <iostream> [L1:1]
├── using namespace std; [L2:1]
└── FunDecl: int main(...) [L4:5]
    └── BlockStmt [L4:12]
        ├── VarDecl (int) [L5:5]
        │   └── VarInit: x [L5:9]
        │       └── NumberExp: 0 [L5:13]
        ├── IfStmt [L6:5]
        │   ├── BinaryExp (==) [L6:11]
        │   │   ├── IdExp: x [L6:9]
        │   │   └── NumberExp: 0 [L6:14]
        │   └── IfStmt [L7:9]
        │       ├── BinaryExp (<) [L7:15]
        │       │   ├── IdExp: x [L7:13]
        │       │   └── NumberExp: 5 [L7:17]
        │       ├── ExprStmt [L8:13]
        │       │   └── AssignExp (=) [L8:15]
        │       └── ExprStmt [L10:13]
        │           └── AssignExp (=) [L10:15]
        ├── WhileStmt [L12:5]
        │   ├── BinaryExp (<) [L12:14]
        │   └── BlockStmt [L12:20]
        │       ├── ExprStmt [L13:9]
        │       │   └── PostfixExp (POST_INC (++)) [L13:10]
        │       ├── IfStmt [L14:9]
        │       │   └── ContinueStmt [L14:21]
        │       └── IfStmt [L15:9]
        │           └── BreakStmt [L15:21]
        ├── ForStmt [L18:5]
        │   ├── VarDecl (int) [L18:10]
        │   │   └── VarInit: i [L18:14]
        │   ├── BinaryExp (<) [L18:23]
        │   ├── PostfixExp (POST_INC (++)) [L18:29]
        │   └── BlockStmt [L18:33]
        ├── DoWhileStmt [L22:5]
        │   ├── BlockStmt [L22:8]
        │   └── BinaryExp (>) [L24:16]
        └── ReturnStmt [L26:5]
            └── IdExp: x [L26:12]
```

---

### Demo 3: Declaración de Clases y Structs
El compilador analiza definiciones de clases y structs con miembros `public:` y `private:`, métodos y atributos.

**Comando:**
```bash
./compiler inputs/05_classes_structs.cpp --ast
```

**Salida:**
```text
Program [L1:1]
├── #include <iostream> [L1:1]
├── using namespace std; [L2:1]
├── RecordDecl (struct Point) [L4:1]
│   ├── RecordMember (public:)
│   │   └── VarDecl (int) [L6:9]
│   │       └── VarInit: x [L6:9]
│   ├── RecordMember (public:)
│   │   └── VarDecl (int) [L7:9]
│   │       └── VarInit: y [L7:9]
│   └── RecordMember (public:)
│       └── FunDecl: int sum(...) [L8:9]
│           └── BlockStmt [L8:15]
│               └── ReturnStmt [L9:9]
│                   └── BinaryExp (+) [L9:18]
│                       ├── IdExp: x [L9:16]
│                       └── IdExp: y [L9:20]
├── RecordDecl (class Rectangle) [L13:1]
│   ├── RecordMember (private:)
│   │   └── VarDecl (int) [L15:9]
│   │       └── VarInit: width [L15:9]
│   ├── RecordMember (private:)
│   │   └── VarDecl (int) [L16:9]
│   │       └── VarInit: height [L16:9]
│   └── RecordMember (public:)
│       └── FunDecl: int getArea(...) [L18:9]
│           └── BlockStmt [L18:19]
└── FunDecl: int main(...) [L23:5]
    └── BlockStmt [L23:12]
        ├── VarDecl (Point) [L24:5]
        │   └── VarInit: pt [L24:11]
        ├── ExprStmt [L25:5]
        │   └── AssignExp (=) [L25:10]
        │       ├── PostfixExp (DOT_ACCESS (.x)) [L25:7]
        │       │   └── IdExp: pt [L25:5]
        │       └── NumberExp: 10 [L25:12]
        ...
```

---

### Demo 4: Detección y Diagnóstico de Errores

El compilador reporta errores sintácticos y semánticos con ubicación exacta (`línea` y `columna`).

#### Caso A: Variable no declarada
```bash
./compiler inputs/07_sem_error_undeclared.cpp
```
**Salida:**
```text
=== [FASE 3: ANÁLISIS SEMÁNTICO Y COMPROBACIÓN DE TIPOS] ===

--- ERRORES SEMÁNTICOS (2) ---
  Error Semántico [Línea 3, Columna 17] Identificador no declarado: 'variableInexistente'
  Error Semántico [Línea 3, Columna 9] Inicialización incompatible para variable 'b': tipo 'int' inicializado con '<error-type>'

[FALLO SEMÁNTICO] Se encontraron 2 error(es) semántico(s).
```

#### Caso B: Incompatibilidad de tipos en asignación y retorno
```bash
./compiler inputs/08_sem_error_types.cpp
```
**Salida:**
```text
=== [FASE 3: ANÁLISIS SEMÁNTICO Y COMPROBACIÓN DE TIPOS] ===

--- ERRORES SEMÁNTICOS (2) ---
  Error Semántico [Línea 3, Columna 5] Tipo de retorno incompatible en 'suma': se esperaba 'int', pero se devolvió 'int*'
  Error Semántico [Línea 7, Columna 10] Inicialización incompatible para variable 'ptr': tipo 'int*' inicializado con 'int'

[FALLO SEMÁNTICO] Se encontraron 2 error(es) semántico(s).
```

#### Caso C: Sentencia de control fuera de contexto
```bash
./compiler inputs/09_sem_error_break.cpp
```
**Salida:**
```text
[FALLO SINTÁCTICO] Error sintáctico [Línea 4, Columna 5]: 'break' solo es válido dentro de un bucle (while, for, do-while)
```

---

### Demo 5: Suite de Pruebas Automatizada

El repositorio incluye un script en Python que ejecuta de forma secuencial todos los casos de prueba y reporta el estado de cada uno.

**Comando:**
```bash
make test
# o directamente:
python3 run_tests.py
```

**Salida:**
```text
================================================================================
CASO DE PRUEBA                      | EXPECTATIVA     | RESULTADO    | ESTADO    
================================================================================
01_arithmetic.cpp                   | Compilar OK     | Frontend OK  | ✓ PASÓ    
02_control_flow.cpp                 | Compilar OK     | Frontend OK  | ✓ PASÓ    
03_functions.cpp                    | Compilar OK     | Frontend OK  | ✓ PASÓ    
04_arrays_pointers.cpp              | Compilar OK     | Frontend OK  | ✓ PASÓ    
05_classes_structs.cpp              | Compilar OK     | Frontend OK  | ✓ PASÓ    
06_advanced_expressions.cpp         | Compilar OK     | Frontend OK  | ✓ PASÓ    
07_sem_error_undeclared.cpp         | Detectar Error  | Error Detectado | ✓ PASÓ    
08_sem_error_types.cpp              | Detectar Error  | Error Detectado | ✓ PASÓ    
09_sem_error_break.cpp              | Detectar Error  | Error Detectado | ✓ PASÓ    
================================================================================
Resumen de Pruebas: 9/9 pruebas superadas exitosamente (100.0%)
================================================================================

Reporte guardado en outputs/reporte_avance_30.txt
Tokens y ASTs generados en carpeta 'outputs/'
```

---

## 5. Casos de Prueba Disponibles (`inputs/`)

- [`01_arithmetic.cpp`](inputs/01_arithmetic.cpp): Precedencia de operadores aritméticos, lógicos y relacionales.
- [`02_control_flow.cpp`](inputs/02_control_flow.cpp): Estructuras `if-else` anidadas (resolución de dangling-else), bucles `while`, `for`, `do-while`, `break` y `continue`.
- [`03_functions.cpp`](inputs/03_functions.cpp): Funciones con múltiples parámetros y recursión (`mcd`, `fibonacci`, `factorial`).
- [`04_arrays_pointers.cpp`](inputs/04_arrays_pointers.cpp): Declaración de arreglos, indexación, punteros, desreferenciación (`*`), dirección (`&`), `new` y `delete`.
- [`05_classes_structs.cpp`](inputs/05_classes_structs.cpp): Declaración de `struct` y `class` con acceso `public:` y `private:`, métodos y campos.
- [`06_advanced_expressions.cpp`](inputs/06_advanced_expressions.cpp): Operaciones a nivel de bits (`&`, `|`, `^`, `~`), desplazamientos (`<<`, `>>`) y asignaciones encadenadas (`x = y = 42`).
- [`07_sem_error_undeclared.cpp`](inputs/07_sem_error_undeclared.cpp): Caso de error: uso de variable no declarada.
- [`08_sem_error_types.cpp`](inputs/08_sem_error_types.cpp): Caso de error: asignación de entero a puntero y discrepancia en tipo de retorno.
- [`09_sem_error_break.cpp`](inputs/09_sem_error_break.cpp): Caso de error: `break` fuera del ámbito de un bucle.
