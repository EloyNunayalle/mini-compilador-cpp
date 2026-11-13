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
| **Layout** | `layout.h` | Estructuras `VarInfo`, `RecordLayout`, `FunctionLayout` con offsets, tamaños y alineaciones calculados por el TypeChecker. |
| **Optimizador** | `optimizer.h`, `optimizer.cpp` | Pases opcionales sobre el AST (`FoldVisitor`, `InlineVisitor`, `SethiVisitor`) y análisis de pureza. Solo con `-O1`. |
| **API JSON** | `api.h`, `api.cpp` | Modo `--api`: tokens, AST (con y sin tipos), layout/ABI, ensamblador y estadísticas de optimización en un objeto JSON. |
| **Gencode** | `gencode.h`, `gencode.cpp` | Visitor que emite ensamblador GNU AS (AT&T) para Linux x86-64, ABI System V, sin optimizaciones. Solo lee las anotaciones del AST. |
| **Runtime** | `runtime/runtime.c` | `string` dinámico, `cout` y `cin` en C puro (sin libstdc++). |
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
- `-O0` (por defecto) / `-O1`: con `-O1` corren los pases de optimización antes del Gencode. `--passes inline,fold,sethi` elige pases y `--report-opt` lista las transformaciones.
- `--api --mode <scan|ast|check|asm|all>`: lee el código por stdin y responde JSON (lo usa el editor web).
- `--asm [salida.s]`: Además del análisis, genera ensamblador x86-64 AT&T (por defecto `<archivo>.s`).

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

--- ERRORES SEMÁNTICOS (1) ---
  Error Semántico [Línea 3, Columna 17] Identificador no declarado: 'variableInexistente'

[FALLO SEMÁNTICO] Se encontraron 1 error(es) semántico(s).
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
  Error Semántico [Línea 7, Columna 10] Inicialización incompatible: tipo 'int*' inicializado con 'int'

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


---

## 6. Generación de Código (Linux x86-64, System V)

```bash
./compiler programa.cpp --asm programa.s
gcc programa.s runtime/runtime.c -o programa
./programa
```

### Decisiones de diseño

- **El Gencode no optimiza**: cada operación del fuente emite su instrucción, y cada copia, constructor y destructor que exige la semántica se emite también. Sin movimiento ni elisión de copias (ver más abajo).
- **Resultados**: `int/char/bool` en `%eax`, punteros y direcciones en `%rax`, `float/double` en `%xmm0`. Un record o `string` devuelto por valor vive en un temporal del marco y la expresión produce su dirección. Temporales con `push/pop` y alineación de `%rsp` a 16 antes de cada `call`.
- **Frame**: `pushq %rbp; movq %rsp,%rbp; subq $N,%rsp` con `N` múltiplo de 16. Cada variable y cada temporal tienen un offset único (sin reutilizar slots).
- **Layout**: modelo LP64 (`char/bool` 1, `int/float` 4, `double`/puntero/referencia 8), padding por alineación natural, `string` de 16 bytes.
- **Globales**: en `.bss`; se construyen en orden de declaración dentro de `__init_globals` (llamada al inicio de `main`) y se destruyen en orden inverso en `__fini_globals` al terminar `main`.
- **Etiquetas**: funciones `f_nombre`, métodos `m_Record_nombre`, constructores `ctor_Record_default|args|copy`, destructor `dtor_Record`, asignación `assign_Record`, globales `g_nombre`.
- **Includes y `using`**: `cout`, `cin`, `endl` requieren `<iostream>` y `using namespace std` o `std::`; `string` requiere `<string>`; `sqrt` requiere `<cmath>`.

### Clases: constructores, copia, destructor y asignación

- Constructor por defecto, un constructor con parámetros y constructor de copia (`T(const T&)` o `T(T&)`). Si se declara cualquier constructor desaparece el constructor por defecto implícito.
- Lista de inicialización (`T(int a) : x(a), s("hola") {}`); los campos se inicializan en orden de declaración, no en el de la lista. Precedencia: lista, inicializador en la clase, inicialización por defecto.
- Inicializadores de campo en la clase (`int x = 1;`), limitados a expresiones constantes.
- Destructor `~T()`; los campos se destruyen en orden inverso después del cuerpo. Los destructores corren al salir de bloques, en `return`, `break` y `continue`, con `delete`/`delete[]`, al final de temporales y de parámetros por valor, y al terminar el programa para los globales.
- Asignación de copia solo como `T& operator=(const T&)`. Si no se declara se genera una miembro a miembro; los `string` se copian en profundidad. Es la única sobrecarga de operador permitida.
- Creación: `T t;`, `T t(args);`, `T t = otro;`, `T t = {a, b}` (agregados), `T(args)` como temporal, `new T`, `new T(args)`, `new T[n]` (con `delete[]`).
- Reglas de generación implícita de C++ (constructor por defecto suprimido, copia y asignación eliminadas con campos `const` o referencias).

### const

Variables, parámetros, campos, punteros a const (`const T*`), punteros const (`T* const`), referencias a const, métodos const, objetos y arreglos const y `string` const. El TypeChecker rechaza escrituras y conversiones que quitan `const`; `const int N = 4;` sirve como dimensión de arreglo.

### Paso y retorno de records (System V AMD64 / Itanium)

El TypeChecker calcula la clasificación (`INTEGER`, `SSE`, `MEMORY`) por eightbyte, el reparto en registros enteros y XMM, los argumentos en pila y el retorno; el Gencode solo la consume.

- Record trivial de hasta 16 bytes: en registros según la clase de cada eightbyte. Si faltan registros o supera 16 bytes: en pila.
- Record con constructor de copia o destructor no trivial (incluye los que contienen `string`): se pasa por referencia invisible y se devuelve por puntero oculto en `%rdi`. Orden: puntero de retorno, `this`, argumentos.
- Diferencia con la ABI de Itanium: el llamador crea la copia del argumento y la destruye al final de la expresión completa (igual que Itanium).

### Diferencias con C++17

- No hay constructor ni asignación de movimiento, `std::move`, referencias rvalue ni elisión de copias (ni obligatoria ni opcional): `T t = f();` hace las dos copias de C++14 sin elisión. Los resultados esperados de las pruebas salen de `g++ -std=c++14 -fno-elide-constructors`.
- Sin conversiones implícitas por constructor (`T t = 5;` se rechaza; usar `T t(5);`).
- Un temporal enlazado a una referencia const de una variable vive hasta el final de su scope.
- Un literal de cadena se acepta como `char*` (extensión de GCC) además de `const char*`.

### Limitaciones conocidas

- Sin herencia, funciones virtuales, templates, excepciones, `static`, `friend`, `explicit`, `mutable`, `volatile`, `constexpr`, sobrecarga general ni argumentos por defecto.
- `new T[n]` solo con una dimensión; parámetros de arreglo solo de una dimensión.
- Métodos de `string`: `size`, `length`, `empty`, `clear`, `c_str`, `append`, `push_back`, `[]`, `==`, `!=`. No hay `+` entre strings.
- Las funciones deben estar definidas en el archivo antes de usarse como firma; el parser no admite prototipos sin cuerpo.
- Sin comprobación de límites en arreglos.

## 7. Optimizaciones (`-O1`)

`-O0` es el modo por defecto y no ejecuta ningún pase. Con `-O1` corren, en este orden, tres visitors separados sobre el AST; después de cada uno se vuelve a ejecutar el TypeChecker, de modo que el Gencode recibe tipos, offsets y ABI actuales. Si esa verificación falla, la optimización se descarta y se compila sin optimizar.

- **Inline**: solo funciones libres de una sola sentencia `return expr;`, escalares, sin llamadas, sin variables globales, sin `&&`/`||` y con argumentos puros del mismo tipo. Clona el AST. Rechaza recursión, argumentos con efectos, objetos y strings.
- **Fold**: plegado de constantes enteras (sin overflow ni división por cero), identidades (`x+0`, `x*1`, `x/1`), `x*0` solo si `x` es puro, `x*2` a `x+x` solo si `x` es puro, y cortocircuito solo si el lado eliminado es puro. No toca floats, strings, records ni expresiones con efectos.
- **Sethi**: en expresiones enteras puras intercambia los operandos de operadores conmutativos para evaluar primero el subárbol más pesado (menor profundidad de pila). No toca `&&`/`||`.

No se optimizan constructores, destructores, copias, asignaciones, `new`, `delete` ni inicializadores globales.

```bash
python3 run_opt_tests.py    # compara -O0 y -O1 (salida y código de retorno) y el oráculo
```

## 8. API y editor web (Docker)

```bash
docker compose up --build        # luego abrir http://127.0.0.1:8080
```

Un solo contenedor `linux/amd64` compila el compilador, sirve el API (`/api/compile`, `/api/health`, `/api/status`, `/api/examples`) y el editor. El editor muestra tokens, AST con y sin tipos, layout y ABI, ensamblador, salida, diagnóstico por fase y el efecto de `-O1`. El código se compila con `gcc` junto a `runtime/runtime.c` dentro del contenedor, con tiempo y recursos limitados. Pruebas: `python3 tests_api/test_api.py`, `node tests_api/test_frontend_lib.js` y `tests_api/run_docker_tests.sh`.

### Pruebas del compilador

```bash
python3 run_tests.py                    # frontend
python3 run_gencode_tests.py            # ensambla, ejecuta y compara con tests_gencode/*.expected
python3 run_gencode_tests.py --asan     # con AddressSanitizer y LeakSanitizer
python3 run_gencode_tests.py --update-expected   # regenera esperados con g++ real
python3 run_opt_tests.py                # -O0 contra -O1
```

En Linux x86-64 usan `gcc` local; en otras plataformas usan Docker (`gcc` para `linux/amd64`).
