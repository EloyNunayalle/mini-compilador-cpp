Gramatica **EBNF** (llaves `{}` para repetición y corchetes `[]` para opcional).

---

## Programa

```ebnf
Program ::= [ IncludeList ]
            [ UsingList ]
            Decl { Decl }

IncludeList ::= Include { Include }

Include ::= #include "<" Library ">"

Library ::= iostream
          | string
          | cmath

UsingList ::= Using { Using }

Using ::= using UsingSpec ";"

UsingSpec ::= namespace QualifiedId
            | QualifiedId

QualifiedId ::= id { "::" id }
```

---

## Tipos

```ebnf
NonVoidBaseType ::= int
                  | float
                  | double
                  | bool
                  | char
                  | QualifiedId

BaseType ::= NonVoidBaseType
           | void

ReturnType ::= NonVoidBaseType { "*" } [ "&" ]
             | void [ VoidPointerSuffix ]

VoidPointerSuffix ::= "*" { "*" } [ "&" ]
```

---

## Declaraciones

```ebnf
Decl         ::= VarDecl
               | FunDecl
               | RecordDecl
```

---

## Variables

```ebnf
VarDecl ::= BaseType VarList ";"

VarList ::= VarInit { "," VarInit }

VarInit ::= { "*" }
            ( id [ ArrayDims ] [ "=" Initializer ]
            | "&" id "=" Initializer )

Declarator ::= { "*" }
               ( id [ ArrayDims ]
               | "&" id )

Initializer ::= CExp
              | BracedInitList

BracedInitList ::= "{" [ InitList ] "}"

InitList ::= Initializer { "," Initializer }

ArrayDims ::= "[" CExp "]"
              { "[" CExp "]" }
```

Ejemplos:

```cpp
// Variables simples
int x;
float precio;
string nombre;

// Variables con inicialización
int x = 5;
int resultado = a + b * 2;
float precio = 10.5;

// Declaración de múltiples variables
int x, y;
int x = 3, y = 4;
int a, b = 10, c = 20;

// Arreglos unidimensionales
int numeros[5];
int numeros[3] = {1, 2, 3};
int valores[3] = {x + 1, y * 2, a - b};

// Arreglos multidimensionales
int matriz[2][3];
int matriz[2][2] = {{1, 2}, {3, 4}};
int matriz[2][2] = {{a + 1, b * 2}, {c - 3, d / 2}};

// Listas de inicialización anidadas
int cubo[2][2][2] = {
    {{1, 2}, {3, 4}},
    {{5, 6}, {7, 8}}
};

// Lista de inicialización vacía
int numeros[3] = {};

// Dimensiones con expresiones constantes
int numeros[5];
int matriz[2 + 1][3];
int arreglo[2 + 3 * 4];
//
int n = 10;
int arreglo2[n];   // no es C++ estándar si n no es constante
```

---

## Funciones

```ebnf
FunDecl      ::= ReturnType id "(" [ ParamDeclList ] ")" Block
```

---

## Parámetros

```ebnf
ParamDeclList ::= Param { "," Param }

Param ::= BaseType Declarator
```

---

## Clases y structs

```ebnf
RecordDecl ::= ClassKey id "{"
                 { ClassMember }
               "}" ";"

ClassKey ::= class
           | struct

ClassMember ::= AccessSpecifier
              | VarDecl
              | FunDecl

AccessSpecifier ::= public ":"
                  | private ":"
```

---

## Block

```ebnf
Block ::= "{"
            { VarDecl | Stmt }
         "}"
```

---

## Sentencias

```ebnf
Stmt ::= IfStmt
       | WhileStmt
       | ForStmt
       | DoWhileStmt
       | BreakStmt
       | ContinueStmt
       | ReturnStmt
       | ExprStmt
       | Block
```

---

### If

```ebnf
IfStmt ::= if "(" CExp ")" Stmt
           [ else Stmt ]
```

**Nota:** Para resolver la ambigüedad del `else`, el parser asociará cada `else` al `if` más cercano que todavía no tenga uno, siguiendo la regla convencional de C++.

---

### While

```ebnf
WhileStmt ::= while "(" CExp ")" Stmt
```

---

### For

```ebnf
ForStmt ::= for "(" [ForInit] ";" [CExp] ";" [ForUpdate] ")" Stmt

ForInit ::= VarDeclNoSemicolon
          | CExp { "," CExp }

VarDeclNoSemicolon ::= BaseType VarList

ForUpdate ::= CExp { "," CExp }
```

---

### DoWhile

```ebnf
DoWhileStmt ::= do Stmt while "(" CExp ")" ";"
```

---

### Break y Continue

```ebnf
BreakStmt ::= break ";"
ContinueStmt ::= continue ";"
```

---

### Return

```ebnf
ReturnStmt ::= return [ CExp ] ";"
```

---

### Expresión como sentencia

```ebnf
ExprStmt ::= CExp ";"
```

Permite cosas como

```cpp
f();
x++;
a + b;
x << 2;
a < b;
```

# Expresiones

```ebnf
CExp ::= AssignExp

AssignExp ::= LogicOr [ "=" AssignExp ]

LogicOr ::= LogicAnd
            { "||" LogicAnd }

LogicAnd ::= BitOr
             { "&&" BitOr }

BitOr ::= BitXor
          { "|" BitXor }

BitXor ::= BitAnd
           { "^" BitAnd }

BitAnd ::= EqExp
           { "&" EqExp }

EqExp ::= RelExp
          { ("==" | "!=") RelExp }

RelExp ::= ShiftExp { RelOp ShiftExp }

RelOp ::= "<" | "<=" | ">" | ">="

ShiftExp ::= Exp
             { ("<<" | ">>") Exp }

Exp ::= Term
        { ("+" | "-") Term }

Term ::= Unary
         { ("*" | "/" | "%") Unary }
```

### Unary

```ebnf
Unary ::= "+" Unary
        | "-" Unary
        | "!" Unary
        | "~" Unary
        | "*" Unary
        | "&" Unary
        | "++" Unary
        | "--" Unary
        | DeleteExpr
        | NewExpr
        | Postfix
```

---

### Delete

```ebnf
DeleteExpr ::= delete [ "[" "]" ] Unary
```

---

### New

```ebnf
NewExpr ::= new NewType [ ArrayDims ]

NewType ::= NonVoidBaseType { "*" }
          | void "*" { "*" }
```

---

## Postfix

```ebnf
Postfix ::= Primary { PostfixSuffix }

PostfixSuffix ::= "[" CExp "]"
                | "." id
                | "->" id
                | "(" [ ArgList ] ")"
                | "++"
                | "--"

Primary ::= QualifiedId
          | Num
          | Real
          | Bool
          | Char
          | String
          | nullptr
          | "(" CExp ")"
```

```ebnf
ArgList ::= CExp { "," CExp }
```

---

## Booleanos

```ebnf
Bool ::= true
       | false
```

---

## Observación importante: implementación del parser

La gramática EBNF presenta algunas construcciones cuya interpretación requiere reglas adicionales durante la implementación del parser.

**Asociación del `else`.** La producción `IfStmt` permite omitir la cláusula `else`, lo que genera la ambigüedad conocida como *dangling else*. Para resolverla, el parser asociará cada `else` al `if` más cercano que todavía no tenga uno, siguiendo la regla convencional de C++.

**Distinción entre declaraciones y expresiones.** Los tipos definidos por el usuario se reconocen mediante `QualifiedId`, que también puede aparecer en las expresiones. Esto provoca que una secuencia como `a * b;` pueda interpretarse como una multiplicación o como una declaración de un puntero, dependiendo de si `a` representa una variable o un tipo. Para resolver esta situación, el parser consultará la información de tipos declarados y sus ámbitos, distinguiendo las declaraciones de las expresiones durante el análisis sintáctico.

**Distinción entre declaraciones de funciones y variables.** Las producciones `FunDecl` y `VarDecl` pueden compartir un prefijo. Esta coincidencia no implica necesariamente una ambigüedad formal, pero puede requerir anticipación adicional o análisis especulativo para determinar qué producción corresponde al programa de entrada.

Estas decisiones permiten mantener una gramática legible sin incorporar todas las restricciones contextuales en sus producciones. La gramática reconoce la estructura sintáctica, mientras que el parser resuelve las decisiones de interpretación indicadas y el análisis semántico verifica la validez de los tipos, las asignaciones y el uso de los identificadores.
