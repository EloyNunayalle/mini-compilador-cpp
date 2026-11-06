# Tipos, representación, alineación y ABI de C++ en x86-64

## Guía de estudio y referencia para generar ensamblador GNU AS

**Entorno principal de esta guía:** Linux x86-64, GCC/G++, formato ELF, GNU Assembler (GAS), sintaxis AT&T y ABI System V AMD64. Los ejemplos se centran en tipos escalares, arreglos, estructuras, clases, funciones y variables globales.

**Relación con el documento anterior:** conviene estudiar primero `memoria-stack-heap-y-ejecucion-x86-64-v3.md`. Ese documento construye el modelo de proceso, memoria virtual, stack, heap, `%rsp`, `%rbp`, `CALL`, `RET`, kernel y MMU. Esta guía utiliza esos conceptos para explicar cómo se representan los tipos de C++ y cómo se pasan los objetos entre funciones.

---

# Mapa de lectura

La secuencia está organizada por dependencias, no por el orden en que surgieron las preguntas:

1. Qué significa un tipo y cuánto ocupa.
2. Cómo se representan los enteros.
3. Cómo se representan los flotantes.
4. Cómo se operan los flotantes en registros XMM.
5. Cómo se cargan constantes flotantes y cómo se organiza un *constant pool*.
6. Qué significa alineación y cómo se calcula el padding.
7. Cómo se distribuyen estructuras, arreglos y variables locales.
8. Cómo se pasan argumentos y resultados según la ABI.
9. Cómo se pasan punteros y referencias.
10. Cómo se implementan los métodos de clases, `this`, `static` y `virtual`.
11. Cómo se representan los nombres de funciones y métodos.
12. Cómo se almacenan e inicializan las variables globales.
13. Cómo inspeccionar el resultado generado por GCC.
14. Qué componentes debe implementar un compilador propio.

---

# 0. Convenciones y alcance

## 0.1. Qué significa cada nivel

En todo el documento distinguiremos cuatro niveles:

| Nivel | Responsabilidad |
|---|---|
| **C++** | Define tipos, objetos, conversiones, referencias, clases, tiempos de vida y reglas del lenguaje. |
| **Compilador (GCC)** | Decide qué instrucciones generar, qué valores mantener en registros, qué posiciones asignar y qué optimizaciones aplicar. |
| **ABI System V AMD64** | Define cómo se representan y pasan los datos entre funciones compiladas para ese entorno, además de otras convenciones binarias. |
| **CPU x86-64** | Ejecuta instrucciones que operan con registros, bits y direcciones de memoria. No conoce los nombres de los tipos de C++. |

No confundas una regla del lenguaje C++ con una decisión de GCC. Dos compiladores pueden generar ensamblador diferente y cumplir el mismo contrato de C++ y la misma ABI.

## 0.2. Sintaxis AT&T mínima

Los ejemplos utilizan sintaxis AT&T:

| Escritura | Significado |
|---|---|
| `%rax` | Registro. |
| `$10` | Valor inmediato. |
| `(%rax)` | Memoria en la dirección contenida en RAX. |
| `8(%rbp)` | Memoria en la dirección RBP + 8. |
| `movl` | Movimiento de 32 bits. |
| `movq` | Movimiento de 64 bits. |
| `movw` | Movimiento de 16 bits. |
| `movb` | Movimiento de 8 bits. |
| `etiqueta:` | Símbolo que identifica una posición. |

El orden habitual de operandos es **fuente, destino**:

```asm
movl $25, %eax       # EAX = 25
addl %ebx, %eax      # EAX = EAX + EBX
movl (%rax), %edx    # EDX = memoria[RAX]
movl %edx, (%rax)    # memoria[RAX] = EDX
```

`$25` es el valor 25; `%rax` es el registro; `(%rax)` es la memoria apuntada por RAX. Esta diferencia es esencial al trabajar con punteros.

## 0.3. Tamaño de una operación no siempre es tamaño del dato en memoria

El sufijo de una instrucción suele indicar el ancho del operando:

| Sufijo | Bits | Bytes |
|---|---:|---:|
| `b` | 8 | 1 |
| `w` | 16 | 2 |
| `l` | 32 | 4 |
| `q` | 64 | 8 |

Para las instrucciones SSE de punto flotante se utilizan otros sufijos, como `ss` (*scalar single*) y `sd` (*scalar double*), explicados más adelante.

---

# PARTE I. TIPOS Y REPRESENTACIÓN DE DATOS

# 1. Qué significa que una variable tenga un tipo

Considera:

```cpp
int x = 25;
float y = 25.0f;
```

Ambas variables expresan el número 25, pero no utilizan la misma representación binaria.

El tipo de C++ determina, entre otras cosas:

- El tamaño y la alineación del objeto en el entorno concreto.
- Qué valores puede representar.
- Cómo se interpretan los bits.
- Qué operaciones y conversiones permite el lenguaje.
- Qué reglas se aplican al pasar el valor a una función.

En ensamblador no existe una instrucción que diga «almacena un `int` de C++». Existen instrucciones que mueven u operan con cantidades de 8, 16, 32 o 64 bits, o que interpretan bits mediante instrucciones de punto flotante.

Por ejemplo:

```asm
movl $25, %eax
```

GAS sabe que `movl` mueve 32 bits. El nombre `int` pertenece al nivel de C++; no está codificado como una etiqueta de tipo en esa instrucción.

# 2. Tamaños de los tipos en C++

## 2.1. Modelo de datos del entorno

En Linux x86-64 con GCC se utiliza normalmente el modelo **LP64**:

- `int`: 32 bits.
- `long`: 64 bits.
- Punteros: 64 bits.

En Windows x86-64 es habitual el modelo **LLP64**, donde `long` continúa teniendo 32 bits. Por eso los tamaños no deben suponerse sin conocer el entorno de compilación.

## 2.2. Tamaños habituales en Linux x86-64

Los siguientes valores son los habituales con GCC para el ABI System V AMD64:

| Tipo C++ | `sizeof` habitual | Bits | Categoría |
|---|---:|---:|---|
| `bool` | 1 byte | 8 | Booleano |
| `char` | 1 byte | 8 | Carácter / entero |
| `signed char` | 1 byte | 8 | Entero con signo |
| `unsigned char` | 1 byte | 8 | Entero sin signo |
| `short` | 2 bytes | 16 | Entero |
| `unsigned short` | 2 bytes | 16 | Entero sin signo |
| `int` | 4 bytes | 32 | Entero |
| `unsigned int` | 4 bytes | 32 | Entero sin signo |
| `long` | 8 bytes | 64 | Entero |
| `unsigned long` | 8 bytes | 64 | Entero sin signo |
| `long long` | 8 bytes | 64 | Entero |
| `float` | 4 bytes | 32 | Punto flotante |
| `double` | 8 bytes | 64 | Punto flotante |
| `long double` | 16 bytes | 128 de almacenamiento | Punto flotante extendido |
| Puntero (`T*`) | 8 bytes | 64 | Dirección |

**Nota sobre `char`:** que `char` tenga 8 bits no determina si es signed o unsigned. Su signedness es una propiedad dependiente de la implementación. Si necesitas un entero de 8 bits con signo o sin signo, utiliza `signed char` o `unsigned char`.

**Nota sobre `long double`:** en el GCC habitual de Linux x86-64, ocupa 16 bytes, pero el valor numérico utiliza el formato extendido x87 de 80 bits; los 16 bytes de almacenamiento incluyen espacio adicional. No son 128 bits de precisión numérica. El formato puede cambiar con opciones de compilación o en otras plataformas.

## 2.3. Comprobar el tamaño y los rangos

`sizeof` devuelve bytes, no bits. Para obtener bits en este entorno:

```cpp
#include <iostream>
#include <climits>
#include <limits>

int main() {
    std::cout << "char: " << sizeof(char) << " bytes\n";
    std::cout << "int: " << sizeof(int) << " bytes\n";
    std::cout << "long: " << sizeof(long) << " bytes\n";
    std::cout << "double: " << sizeof(double) << " bytes\n";
    std::cout << "int bits: " << sizeof(int) * CHAR_BIT << '\n';

    std::cout << std::numeric_limits<int>::min() << '\n';
    std::cout << std::numeric_limits<int>::max() << '\n';
}
```

`CHAR_BIT` permite conocer cuántos bits tiene un byte de C++. En x86-64 es 8.

## 2.4. Tamaño del objeto frente al tamaño del puntero

```cpp
int x = 25;
int* p = &x;
float* pf = nullptr;
double* pd = nullptr;
```

En el entorno indicado:

- `x` ocupa 4 bytes.
- `p`, `pf` y `pd` ocupan 8 bytes cada uno.

El tipo apuntado no cambia el tamaño de un puntero. Lo que cambia es el tipo y tamaño del objeto al que se accede al desreferenciarlo.

---

# 3. Representación de enteros

# 3.1. Enteros sin signo (`unsigned`)

En un entero de N bits sin signo, todos los bits codifican la magnitud:

```text
8 bits:
00000000 = 0
00000001 = 1
00000010 = 2
00000101 = 5
11111111 = 255
```

El rango es:

\[
0 \le x \le 2^N - 1
\]

| Ancho | Rango unsigned |
|---:|---:|
| 8 bits | 0 a 255 |
| 16 bits | 0 a 65 535 |
| 32 bits | 0 a 4 294 967 295 |
| 64 bits | 0 a 18 446 744 073 709 551 615 |

# 3.2. Enteros con signo: complemento a dos

En x86-64, los enteros signed se representan en complemento a dos. Para N bits, el rango es:

\[
-2^{N-1} \le x \le 2^{N-1}-1
\]

Con 8 bits:

\[
-128 \le x \le 127
\]

Ejemplos:

| Decimal | Binario de 8 bits |
|---:|---|
| 0 | `00000000` |
| 1 | `00000001` |
| 5 | `00000101` |
| 10 | `00001010` |
| -1 | `11111111` |
| -2 | `11111110` |
| -5 | `11111011` |
| -128 | `10000000` |

Para obtener -5 desde +5:

```text
+5             00000101
Invertir bits  11111010
Sumar uno      11111011
```

El patrón `11111111` no tiene una etiqueta interna que diga «-1» o «255»:

- Como `signed char`, representa -1.
- Como `unsigned char`, representa 255.

La interpretación depende del tipo y de la operación que el compilador genera.

# 3.3. Extensión de signo y extensión con ceros

Cuando un valor pequeño debe utilizarse como uno más ancho, se necesitan reglas de extensión.

### `MOVSX`: extensión con signo

```cpp
signed char x = -5;
int y = x;
```

El patrón de 8 bits de -5 es:

```text
11111011
```

Al extenderlo a 32 bits replicamos el bit de signo:

```text
11111111 11111111 11111111 11111011
```

En AT&T, por ejemplo:

```asm
movsbl fuente, %eax     # 8 bits -> 32 bits, con signo
movsbq fuente, %rax     # 8 bits -> 64 bits, con signo
movslq %edi, %rax       # 32 bits -> 64 bits, con signo
```

### `MOVZX`: extensión con ceros

```cpp
unsigned char x = 251;
int y = x;
```

El patrón de 8 bits también es `11111011`, pero ahora representa 251. Para ampliarlo a 32 bits agregamos ceros:

```text
00000000 00000000 00000000 11111011
```

En ensamblador:

```asm
movzbl fuente, %eax     # 8 bits -> 32 bits, con ceros
movzbq fuente, %rax     # 8 bits -> 64 bits, con ceros
movzwl fuente, %eax     # 16 bits -> 32 bits, con ceros
```

### Regla especial al escribir en registros de 32 bits

En modo de 64 bits, escribir en un registro de 32 bits limpia automáticamente los 32 bits altos del registro correspondiente:

```asm
movl $10, %eax
```

Después, los 64 bits de RAX contienen el valor 10 con la mitad alta en cero.

En cambio, escribir en `%ax` o `%al` modifica solo esa parte y conserva los bits superiores.

Para convertir `int` signed a `long` signed:

```cpp
int x = -10;
long y = x;
```

una posible instrucción es:

```asm
movslq %edi, %rax
```

Para `unsigned int` a `unsigned long`, escribir el valor en `%eax` deja a cero los 32 bits superiores de RAX:

```asm
movl %edi, %eax
```

# 3.4. Operaciones enteras y signedness

La suma binaria básica no tiene una instrucción distinta para signed y unsigned:

```asm
addl %esi, %edi
```

Puede implementar una suma `int` o `unsigned int`. La diferencia aparece al interpretar el resultado, al comparar y al dividir.

### Comparaciones

`CMP` calcula conceptualmente una resta, actualiza flags y descarta el resultado. La interpretación se selecciona mediante el salto:

| Uso | Saltos habituales |
|---|---|
| Igualdad | `JE`, `JNE` |
| Signed | `JL`, `JLE`, `JG`, `JGE` |
| Unsigned | `JB`, `JBE`, `JA`, `JAE` |

Ejemplo signed:

```asm
cmpl %esi, %edi
jl .Lmenor_signed
```

Ejemplo unsigned:

```asm
cmpl %esi, %edi
jb .Lmenor_unsigned
```

La instrucción `CMP` es la misma. Cambia cómo el salto interpreta los flags.

### Overflow en C++

En `unsigned`, el resultado de operaciones aritméticas se define módulo \(2^N\). En enteros `signed`, el overflow aritmético es comportamiento indefinido en C++. Aunque una instrucción x86 conserve los N bits inferiores y establezca flags, el compilador puede optimizar suponiendo que el programa C++ no realiza overflow signed.

Por ello, no debes asumir que el comportamiento observado en una instrucción ensamblador aislada describe todas las posibilidades del programa C++ optimizado.

---

# 4. Representación de números de punto flotante

# 4.1. Por qué `float` y `double` no usan complemento a dos

Los números flotantes deben representar fracciones y rangos de magnitud muy diferentes. En Linux x86-64 con GCC, `float` y `double` suelen seguir los formatos IEEE 754 binary32 y binary64.

## 4.2. `float`: IEEE 754 binary32

Un `float` ocupa 32 bits:

```text
+-----------+----------------+---------------------------+
| Signo 1 b | Exponente 8 b  | Fracción 23 b             |
+-----------+----------------+---------------------------+
```

Para números normales:

\[
valor=(-1)^s\times(1.f)\times 2^{e-127}
\]

- `s`: bit de signo.
- `e`: exponente almacenado.
- `f`: fracción almacenada.
- 127: sesgo o *bias* del exponente.

La parte significativa tiene 24 bits de precisión para números normales: 23 bits almacenados más el bit inicial implícito.

## 4.3. `double`: IEEE 754 binary64

Un `double` ocupa 64 bits:

```text
+-----------+-----------------+----------------------------------------------+
| Signo 1 b | Exponente 11 b  | Fracción 52 b                               |
+-----------+-----------------+----------------------------------------------+
```

Para números normales:

\[
valor=(-1)^s\times(1.f)\times 2^{e-1023}
\]

La precisión significativa habitual es 53 bits: 52 almacenados más el bit inicial implícito.

| Propiedad | `float` | `double` |
|---|---:|---:|
| Tamaño | 32 bits | 64 bits |
| Signo | 1 bit | 1 bit |
| Exponente | 8 bits | 11 bits |
| Fracción almacenada | 23 bits | 52 bits |
| Precisión significativa normal | 24 bits | 53 bits |
| Dígitos decimales aproximados | 6–9 | 15–17 |

Los formatos también contemplan ceros con signo, subnormales, infinitos y `NaN`; no todos los patrones corresponden a un número normal finito.

## 4.4. Ejemplo: representar 1.5

Convertimos 1.5 a binario:

\[
1.5_{10}=1.1_2=1.1_2\times 2^0
\]

Para binary32:

- Signo: `0`.
- Exponente real: 0.
- Exponente almacenado: 0 + 127 = 127 = `01111111`.
- Fracción: `10000000000000000000000`.

El patrón completo es:

```text
0 01111111 10000000000000000000000
```

En hexadecimal:

```text
0x3FC00000
```

Para binary64, 1.5 tiene el patrón:

```text
0x3FF8000000000000
```

Los patrones son distintos porque los formatos tienen anchos y campos diferentes.

## 4.5. Por qué 0.1 no suele ser exacto

Muchos decimales no tienen una expansión binaria finita. Por eso `0.1` se almacena como una aproximación. Al operar, los pequeños errores de representación pueden propagarse:

```cpp
double x = 0.1;
double y = 0.2;
bool igual = (x + y == 0.3);
```

No debes asumir que `igual` será verdadero. Es habitual comparar flotantes mediante una tolerancia adecuada al problema, no mediante igualdad exacta.

---

# 5. Registros XMM y operaciones escalares o empaquetadas

# 5.1. Qué es un registro XMM

En x86-64 existen registros multimedia/vectoriales como XMM0, XMM1, ..., XMM15. Cada registro XMM tiene **128 bits (16 bytes)**.

Un registro XMM no es un registro `float` ni un registro `double`. Es un contenedor de 128 bits. La instrucción determina cómo interpreta y procesa esos bits.

Puede contener:

- Un `float` escalar en los 32 bits inferiores.
- Un `double` escalar en los 64 bits inferiores.
- Cuatro `float` empaquetados de 32 bits.
- Dos `double` empaquetados de 64 bits.
- Otros tipos enteros o patrones de bits, según la instrucción.

# 5.2. Escalar (*scalar*)

Una operación escalar procesa **un solo valor**:

```asm
addss %xmm1, %xmm0     # suma un float de 32 bits
addsd %xmm1, %xmm0     # suma un double de 64 bits
```

Conceptualmente:

\[
XMM0[31:0]=XMM0[31:0]+XMM1[31:0]\quad\text{para ADDSS}
\]

\[
XMM0[63:0]=XMM0[63:0]+XMM1[63:0]\quad\text{para ADDSD}
\]

La operación aritmética utiliza solo el valor escalar inferior. Los bits restantes no forman parte de ese valor. Su conservación o limpieza exacta depende de la codificación e instrucción (SSE legado o VEX/EVEX); no debe suponerse que todas las instrucciones tratan igual la parte alta.

# 5.3. Empaquetado (*packed*) y SIMD

En una operación empaquetada, una sola instrucción opera con varios elementos independientes dentro del registro. Esto se denomina SIMD: *Single Instruction, Multiple Data*.

Ejemplo con cuatro `float`:

```cpp
float a[4] = {1, 2, 3, 4};
float b[4] = {10, 20, 30, 40};
```

Si A está en XMM0 y B en XMM1, conceptualmente:

```asm
addps %xmm1, %xmm0
```

realiza cuatro sumas por posición y deja `[11, 22, 33, 44]` en los cuatro carriles de XMM0.

Para dos `double` empaquetados:

```asm
addpd %xmm1, %xmm0
```

realiza dos sumas de 64 bits.

| Instrucción | Operación | Valores por XMM de 128 bits |
|---|---|---:|
| `addss` | `float` escalar | 1 |
| `addps` | `float` empaquetado | 4 |
| `addsd` | `double` escalar | 1 |
| `addpd` | `double` empaquetado | 2 |

# 5.4. Excepción importante: `long double`

En el GCC habitual de Linux x86-64, `long double` usa el formato extendido x87 de 80 bits, almacenado en 16 bytes. No se trata como un `double` binary64 y no se maneja con las operaciones escalares `addsd`/`mulsd`.

La ABI System V AMD64 clasifica este formato mediante clases X87/X87UP:

- Un argumento `long double` ordinario se pasa en memoria, no en XMM0–XMM7.
- Un resultado `long double` se devuelve en el registro de pila x87 `ST(0)`.

Por eso no debes generalizar la regla «todos los flotantes van en XMM». Esa regla describe `float` y `double` escalares habituales; `long double` es una excepción de esta plataforma/ABI.

# 5.5. Instrucciones comunes para flotantes

| Operación | `float` escalar | `double` escalar |
|---|---|---|
| Mover desde memoria | `movss` | `movsd` |
| Sumar | `addss` | `addsd` |
| Restar | `subss` | `subsd` |
| Multiplicar | `mulss` | `mulsd` |
| Dividir | `divss` | `divsd` |
| Comparar sin ordenar | `ucomiss` | `ucomisd` |

Las formas empaquetadas correspondientes incluyen `addps`, `subps`, `mulps`, `divps` para cuatro `float`, y `addpd`, `subpd`, `mulpd`, `divpd` para dos `double`.

# 5.6. `movd` no es `movl`

La `d` de `movd` significa *doubleword*: 32 bits. `movd` es una instrucción para mover 32 bits entre un registro general y un registro XMM, o entre memoria y XMM.

```asm
movl $0x3FC00000, %eax
movd %eax, %xmm0
```

- `movl` carga 32 bits en el registro general EAX.
- `movd` copia esos mismos 32 bits a la parte baja de XMM0.

Para copiar 64 bits entre GPR y XMM se usa la forma `movq`:

```asm
movabsq $0x3FF8000000000000, %rax
movq %rax, %xmm0
```

**Estas instrucciones copian patrones de bits; no convierten numéricamente un entero en flotante.** Para convertir numéricamente se utilizan instrucciones `CVTSI2SS` o `CVTSI2SD`, explicadas en la siguiente sección.

# 5.7. Conversiones numéricas y comparación de flotantes

## Entero a `float` o `double`

Para convertir un entero signed a punto flotante se utilizan instrucciones de conversión, no `MOVD`/`MOVQ`:

```asm
cvtsi2ssl %edi, %xmm0    # int32 signed -> float32
cvtsi2sdl %edi, %xmm0    # int32 signed -> float64
cvtsi2ssq %rdi, %xmm0    # int64 signed -> float32
cvtsi2sdq %rdi, %xmm0    # int64 signed -> float64
```

La conversión interpreta el entero como número y calcula su valor de punto flotante. No copia su patrón binario original.

Para un valor `unsigned` de 32 bits, se puede primero extenderlo con ceros a un registro de 64 bits y después utilizar una conversión signed de 64 bits, porque cualquier `uint32_t` cabe en un `int64_t`. Para todo el rango `uint64_t`, los valores mayores que `INT64_MAX` requieren una secuencia especial; no debes aplicar directamente una conversión que interpreta el origen como signed.

## `float` o `double` a entero

Las variantes `CVTT` convierten truncando hacia cero:

```asm
cvttss2sil %xmm0, %eax   # float32 -> int32, truncar hacia cero
cvttsd2sil %xmm0, %eax   # float64 -> int32, truncar hacia cero
cvttss2siq %xmm0, %rax   # float32 -> int64, truncar hacia cero
cvttsd2siq %xmm0, %rax   # float64 -> int64, truncar hacia cero
```

Las variantes sin `TT` (`CVTSS2SI`, `CVTSD2SI`) utilizan el modo de redondeo configurado en MXCSR. Las variantes con `TT` truncan hacia cero.

En C++, la conversión de flotante a entero descarta la parte fraccionaria hacia cero: `15.8` se convierte en `15` y `-15.8` en `-15`. Si el valor truncado no puede representarse en el tipo entero destino, el comportamiento de la conversión C++ es indefinido; no debes interpretar el valor especial que la instrucción x86 produce para una conversión inválida como una garantía del lenguaje.

## Comparar flotantes y el caso `NaN`

Se utilizan instrucciones como:

```asm
ucomiss %xmm1, %xmm0    # compara los float escalares inferiores
ucomisd %xmm1, %xmm0    # compara los double escalares inferiores
```

Una comparación puede estar **desordenada** si uno de los operandos es `NaN`. En ese caso, `UCOMISS`/`UCOMISD` activan `ZF`, `PF` y `CF`; por eso es importante comprobar primero `PF` con `JP`:

```asm
ucomiss %xmm1, %xmm0
jp .Ldesordenado        # al menos un operando es NaN
jb .Lmenor              # XMM0 < XMM1, si la comparación está ordenada
je .Ligual              # XMM0 == XMM1
# Si no saltó, XMM0 > XMM1
```

No compares flotantes como si fueran enteros con `CMP` y `JL`/`JG`: sus bits siguen otro formato. Utiliza instrucciones de comparación de punto flotante y saltos que interpreten los flags resultantes.

---

# 6. Cargar constantes `float` y `double`

# 6.1. ¿Puedo escribir un literal flotante como inmediato SSE?

No hay una forma ordinaria como estas:

```asm
movss $1.5, %xmm0     # No válida
movsd $3.14, %xmm1    # No válida
addss $2.5, %xmm0     # No válida
```

Las instrucciones SSE escalares reciben el valor desde memoria o desde otro registro XMM. Esto es diferente de las instrucciones enteras que tienen formas con inmediato, como `movl $25, %eax` o `addl $10, %eax`.

## 6.2. Método habitual: constante en `.rodata`

En C++:

```cpp
float x = 1.5f;
double y = 3.141592653589793;
```

En ensamblador, una forma de representar las constantes es:

```asm
.section .rodata
.balign 4
.Lfloat_15:
    .float 1.5

.balign 8
.Ldouble_pi:
    .double 3.141592653589793

.section .text
movss .Lfloat_15(%rip), %xmm0
movsd .Ldouble_pi(%rip), %xmm1
```

`.float` y `.double` son **directivas del ensamblador**, no instrucciones ejecutadas por la CPU. Indican a GAS que emita los bytes de una constante de precisión simple o doble.

`movss` lee 32 bits de memoria; `movsd` lee 64 bits. El direccionamiento `etiqueta(%rip)` utiliza una referencia relativa a RIP.

## 6.3. Método alternativo: mover el patrón mediante registros generales

Para `1.5f`, el patrón binary32 es `0x3FC00000`:

```asm
movl $0x3FC00000, %eax
movd %eax, %xmm0
```

Para `1.5` double, el patrón binary64 es `0x3FF8000000000000`:

```asm
movabsq $0x3FF8000000000000, %rax
movq %rax, %xmm0
```

De nuevo, se copian bits. No se convierte el entero 1069547520 en un flotante 1.5; se transporta el patrón que ya representa 1.5 en binary32.

## 6.4. Cargar directamente desde memoria en una operación

No siempre es necesario cargar una constante a un XMM separado:

```asm
addss .LC0(%rip), %xmm0
```

`ADDSS` admite una fuente de memoria de 32 bits. La CPU lee la constante y realiza la suma con el valor escalar inferior de XMM0.

De forma similar, `mulss` y `subss` tienen formas con fuente de memoria. En otros casos, la instrucción puede requerir que el valor ya esté en un registro XMM.

## 6.5. Constantes sencillas y optimización

El compilador puede no necesitar una etiqueta para todas las constantes. Por ejemplo, puede generar cero mediante:

```asm
pxor %xmm0, %xmm0
```

O puede plegar una expresión durante la compilación:

```cpp
float calcular() {
    float x = 1.5f;
    float y = 2.5f;
    return x + y;
}
```

El compilador puede producir directamente el resultado 4.0f y eliminar la carga y la suma en tiempo de ejecución.

## 6.6. Cómo organiza GCC varias constantes

Para:

```cpp
float calcular(float x) {
    return x * 3.25f + 0.75f - 1.5f;
}

float otraFuncion(float x) {
    return x + 3.25f;
}
```

Una salida posible es:

```asm
.section .text
calcular:
    mulss .LC0(%rip), %xmm0
    addss .LC1(%rip), %xmm0
    subss .LC2(%rip), %xmm0
    ret

otraFuncion:
    addss .LC0(%rip), %xmm0
    ret

.section .rodata
.balign 4
.LC0:
    .float 3.25
.LC1:
    .float 0.75
.LC2:
    .float 1.5
```

La misma etiqueta `.LC0` puede reutilizarse en ambas funciones. No se crea otra variable en el stack cada vez que aparece el literal.

GCC puede emitir el patrón flotante directamente como `.long` en lugar de `.float`. Por ejemplo, 1.5f puede aparecer como:

```asm
.LC0:
    .long 1069547520       # 0x3FC00000
```

Ambas formas pueden emitir los mismos 32 bits.

## 6.7. ¿Se puede cambiar de sección durante la generación?

Sí. Puedes cambiar de sección tantas veces como necesites:

```asm
.section .text
funcion:
    movss .LC0(%rip), %xmm0
    ret

.section .rodata
.balign 4
.LC0:
    .float 1.5

.section .text
otra_funcion:
    ret
```

GAS mantiene un contador de posición independiente para cada sección. Al volver a `.text`, continúa en esa sección; no se ejecuta un salto y la CPU nunca ejecuta `.section`.

Sin embargo, es más fácil mantener el generador organizado si emite todas las funciones en `.text` y, al final, todas las constantes en `.rodata`.

## 6.8. Constant pool: conjunto de constantes

Para un compilador propio es conveniente mantener una tabla de constantes:

| Tipo | Bits | Etiqueta |
|---|---|---|
| `float` | patrón de 3.25f | `.LC0` |
| `float` | patrón de 0.75f | `.LC1` |
| `float` | patrón de 1.5f | `.LC2` |

Cuando el visitante encuentra un literal:

1. Determina su tipo (`float` o `double`).
2. Obtiene su representación binaria en el formato correspondiente.
3. Busca si ya hay una entrada con el mismo tipo y patrón.
4. Si existe, reutiliza la etiqueta.
5. Si no existe, crea una entrada y genera una etiqueta nueva.
6. Emite la instrucción que usa esa etiqueta.
7. Al terminar de generar funciones, emite el conjunto en `.rodata`.

La clave de deduplicación debería incluir el formato y los bits, no solamente el texto decimal. `1.5f` y `1.5` son distintos por ser binary32 y binary64. También deben distinguirse `+0.0` y `-0.0`; con `NaN` pueden importar los distintos *payloads*.

Una interfaz conceptual:

```cpp
struct FloatConstant {
    std::string label;
    DataType type;       // Float32 o Float64
    uint64_t bits;
};

class ConstantPool {
public:
    std::string getFloatConstant(float value);
    std::string getDoubleConstant(double value);
    void emit(std::ostream& out);
};
```

`getFloatConstant` registra o encuentra la constante y devuelve su etiqueta. `emit` escribe todas las entradas al final. Esto separa la generación de instrucciones de la emisión de datos.

## 6.9. ¿Las etiquetas implican una reserva dinámica?

No. Una constante en `.rodata` forma parte de los datos del programa. No es una reserva hecha con `malloc` y no se libera con `free` al salir de una función.

La sección permanece mapeada mientras el programa la necesita. Los resultados temporales, en cambio, suelen vivir en registros XMM o en el stack si deben guardarse.

---

# PARTE II. ALINEACIÓN Y DISTRIBUCIÓN EN MEMORIA

# 7. Qué significa alineación

# 7.1. Definición

Un tipo con alineación A exige que la dirección inicial de cada objeto de ese tipo sea múltiplo de A:

\[
\boxed{direccion\bmod A=0}
\]

Si un `double` requiere alineación de 8 bytes, algunas direcciones válidas son:

| Dirección | Divisible entre 8 | Alineada a 8 |
|---|---|---|
| `0x1000` | Sí | Sí |
| `0x1004` | No | No |
| `0x1008` | Sí | Sí |
| `0x1010` | Sí | Sí |
| `0x1014` | No | No |

No se exige una dirección absoluta específica. Cualquier dirección múltiplo de la alineación sirve.

## 7.2. Tamaño y alineación no son lo mismo

- `sizeof(T)`: tamaño del objeto T, en bytes.
- `alignof(T)`: alineación requerida para objetos de tipo T, en bytes.

En el entorno de referencia, valores habituales:

| Tipo | `sizeof` | `alignof` habitual |
|---|---:|---:|
| `char` | 1 | 1 |
| `short` | 2 | 2 |
| `int` | 4 | 4 |
| `float` | 4 | 4 |
| `long` | 8 | 8 |
| `double` | 8 | 8 |
| Puntero | 8 | 8 |
| `long double` | 16 | 16 |

Los valores exactos pertenecen al entorno y pueden cambiar con atributos de alineación u opciones que alteren la ABI. Compruébalos con `sizeof` y `alignof`.

## 7.3. Para qué sirve

La alineación permite que los objetos cumplan los requisitos de representación y acceso del entorno, y facilita accesos eficientes. x86-64 admite muchos accesos desalineados, pero eso no elimina los requisitos de alineación que impone C++ o la ABI al colocar objetos.

# 8. Alineación y padding en estructuras y clases

# 8.1. Regla habitual para una estructura ordinaria

Para una estructura ordinaria, sin `packed`, sin `alignas` especial, sin herencia complicada y sin bit-fields:

\[
\boxed{alignof(S)=\max_i alignof(T_i)}
\]

La alineación de la estructura es, habitualmente, la mayor alineación de sus miembros.

Además, el tamaño total de la estructura debe ser múltiplo de su alineación:

\[
\boxed{sizeof(S)\bmod alignof(S)=0}
\]

No es la alineación la que debe ser múltiplo del tamaño. Es el **tamaño** el que se ajusta a un múltiplo de la **alineación**.

`alignas` puede aumentar la alineación requerida. Herencia, clases no estándar, miembros especiales y bit-fields pueden añadir reglas de layout adicionales.

# 8.2. Regla para colocar un miembro

El compilador procesa los miembros en el orden requerido por el layout de la clase. Para cada miembro, busca el primer offset que satisfaga su alineación después del miembro anterior.

Si `end` es el offset inmediatamente posterior al miembro anterior, entonces:

\[
offset_i=align\_up(end,alignof(T_i))
\]

con:

\[
align\_up(x,a)=\left\lceil\frac{x}{a}\right\rceil a
\]

Después de colocar el miembro, el nuevo final es `offset_i + sizeof(T_i)`.

Al terminar, el tamaño de la estructura se redondea para que sea múltiplo de la alineación de la estructura.

# 8.3. Ejemplo: `char`, `int` y `double`

```cpp
struct Registro {
    char tipo;
    int edad;
    double promedio;
};
```

En el entorno habitual:

| Miembro | Tamaño | Alineación |
|---|---:|---:|
| `tipo` | 1 | 1 |
| `edad` | 4 | 4 |
| `promedio` | 8 | 8 |

Construcción:

1. `tipo` empieza en offset 0 y termina en 1.
2. `edad` debe comenzar en múltiplo de 4. El primer offset válido es 4. Se agregan 3 bytes de padding (offsets 1–3).
3. `edad` ocupa offsets 4–7 y termina en 8.
4. `promedio` necesita alineación 8. El offset 8 ya cumple; ocupa offsets 8–15.
5. El final es 16, que ya es múltiplo de 8.

Resultado:

| Parte | Offset | Bytes |
|---|---:|---:|
| `tipo` | 0 | 1 |
| Padding | 1–3 | 3 |
| `edad` | 4 | 4 |
| `promedio` | 8 | 8 |
| **Total** | | **16** |

Por tanto:

```cpp
sizeof(Registro)  // 16
alignof(Registro) // 8
```

La estructura puede comenzar en cualquier dirección múltiplo de 8: por ejemplo, `0x1000`, `0x1008` o `0x1010`. No necesita comenzar en una dirección múltiplo de 16 ni de 24.

# 8.4. Padding final

```cpp
struct Ejemplo {
    char c;
    double d;
    char estado;
};
```

| Miembro | Offset | Tamaño | Final exclusivo |
|---|---:|---:|---:|
| `c` | 0 | 1 | 1 |
| Padding | 1–7 | 7 | 8 |
| `d` | 8 | 8 | 16 |
| `estado` | 16 | 1 | 17 |

La alineación de la estructura es 8, por el `double`. El final 17 se redondea al siguiente múltiplo de 8: 24.

Quedan 7 bytes de padding final. Así:

```cpp
sizeof(Ejemplo)  // 24
alignof(Ejemplo) // 8
```

## 8.5. Por qué el tamaño total debe ser múltiplo de la alineación

Considera:

```cpp
Ejemplo elementos[3];
```

Si la base está alineada a 8 y cada `Ejemplo` ocupa 24 bytes:

| Elemento | Dirección relativa a la base |
|---|---:|
| `elementos[0]` | +0 |
| `elementos[1]` | +24 |
| `elementos[2]` | +48 |

24 es múltiplo de 8. Por eso cada elemento empieza alineado a 8 si el primero también está alineado.

Para cualquier arreglo de T:

\[
direccion(elemento_i)=base+i\times sizeof(T)
\]

El tamaño de T debe preservar la alineación entre elementos.

# 8.6. El orden de miembros afecta el padding

Compara:

```cpp
struct A {
    char c1;
    double d;
    int i;
    char c2;
};

struct B {
    double d;
    int i;
    char c1;
    char c2;
};
```

Layout habitual de A:

| Miembro | Offset |
|---|---:|
| `c1` | 0 |
| Padding | 1–7 |
| `d` | 8 |
| `i` | 16 |
| `c2` | 20 |
| Padding final | 21–23 |

`sizeof(A) = 24`, `alignof(A) = 8`.

Layout habitual de B:

| Miembro | Offset |
|---|---:|
| `d` | 0 |
| `i` | 8 |
| `c1` | 12 |
| `c2` | 13 |
| Padding final | 14–15 |

`sizeof(B) = 16`, `alignof(B) = 8`.

Agrupar miembros con alineaciones grandes puede reducir el tamaño, pero cambiar el orden puede cambiar la interfaz binaria de la estructura. No lo hagas si otros módulos o archivos dependen del layout original.

# 8.7. `alignas` y `packed`

C++ permite solicitar alineación adicional:

```cpp
struct alignas(64) Datos {
    int x;
};
```

Los objetos `Datos` necesitan alineación de 64 bytes. El tamaño debe permitir que elementos sucesivos de un arreglo sigan alineados; en el ejemplo habitual será 64 bytes.

GCC también ofrece extensiones como `__attribute__((packed))` para reducir el padding:

```cpp
struct __attribute__((packed)) Compacto {
    char c;
    int x;
};
```

`packed` no es una regla portable de C++ estándar. Puede producir miembros desalineados y debe utilizarse con cuidado. Una dirección desalineada puede ser legal para determinadas instrucciones x86, pero puede afectar al rendimiento, a la generación de código y a la portabilidad.

# 8.8. Los bytes de padding no son atributos

El padding no es un campo visible de C++. No almacena un atributo adicional y su contenido no debe suponerse igual a cero. `sizeof` sí incluye el padding dentro del tamaño total del objeto.

## 8.9. Alineación de objetos dinámicos en el heap

La alineación también se aplica a objetos creados dinámicamente:

```cpp
struct alignas(64) Datos {
    int valor;
};

Datos* p = new Datos;
```

`new` debe devolver una dirección que cumpla `alignof(Datos)`, aquí 64 bytes. No tienes que ajustar manualmente el puntero después de `new`; el mecanismo de asignación de C++ debe respetar el requisito de alineación del tipo. Para tipos *over-aligned* (alineación mayor que la alineación fundamental admitida normalmente), C++17 añadió formas de asignación que comunican la alineación mediante `std::align_val_t`.

Puedes comprobarlo en tiempo de ejecución:

```cpp
#include <cstdint>
#include <iostream>

struct alignas(64) Datos { int valor; };

int main() {
    Datos* p = new Datos;
    std::cout << reinterpret_cast<std::uintptr_t>(p) % alignof(Datos) << '\n';
    delete p;
}
```

El resultado correcto es `0`. El heap no tiene una alineación única para todos los objetos: cada asignación debe respetar la alineación requerida por el tipo solicitado.

# 9. Alineación de arreglos

Un arreglo contiene elementos consecutivos. No se añade un padding arbitrario entre elementos: el stride es `sizeof(T)`.

```cpp
int numeros[4] = {10, 20, 30, 40};
```

Como `sizeof(int) = 4`, si la base es `0x1000`:

| Elemento | Dirección |
|---|---|
| `numeros[0]` | `0x1000` |
| `numeros[1]` | `0x1004` |
| `numeros[2]` | `0x1008` |
| `numeros[3]` | `0x100C` |

La dirección del índice i es:

\[
base+i\times sizeof(T)
\]

En AT&T:

```asm
movl (%rax,%rcx,4), %edx
```

RAX contiene la base, RCX el índice y el factor 4 corresponde a `sizeof(int)`.

Para `double`:

```asm
movsd (%rax,%rcx,8), %xmm0
```

porque `sizeof(double) = 8`.

---

# 10. Alineación y almacenamiento de variables locales

# 10.1. ¿Se utiliza el mismo principio que en una estructura?

Sí, se utiliza el mismo **principio de alineación**, pero no necesariamente el mismo algoritmo de layout rígido de una estructura.

Dentro de una función pueden existir:

```cpp
struct Persona {
    char inicial;
    double promedio;
};

void procesar() {
    char letra;
    int edad;
    double nota;
    float descuento;
    Persona persona;
    int* puntero;
    int numeros[3];
}
```

Cada objeto que realmente necesite almacenamiento debe comenzar en una dirección que cumpla la alineación de su tipo. Si un objeto es una estructura, además conserva su layout interno ya calculado.

## 10.2. El compilador no tiene que seguir el orden de las declaraciones

A diferencia de los miembros de una estructura estándar, las variables locales independientes no tienen un layout binario público fijado por el orden del código fuente. El compilador puede:

- Elegir offsets que cumplan la alineación.
- Mantener valores en registros en lugar de memoria.
- Eliminar variables que no necesitan materializarse.
- Reutilizar un mismo slot cuando los tiempos de vida no se superponen.
- Reordenar slots si no cambia el comportamiento observable del programa.

Por eso no debes asumir que las variables locales aparecerán en el stack en el orden en que fueron declaradas.

## 10.3. Ejemplo de offsets relativos a RBP

Supongamos un prólogo clásico:

```asm
pushq %rbp
movq %rsp, %rbp
subq $16, %rsp
```

En la situación ordinaria de la ABI, RBP queda alineado a 16 bytes después del `pushq %rbp`, si el llamador mantuvo la alineación requerida.

El compilador podría asignar:

```asm
movb $65, -1(%rbp)       # char c: 1 byte
movl $20, -8(%rbp)       # int x: 4 bytes
movsd %xmm0, -16(%rbp)   # double d: 8 bytes
```

Si RBP es múltiplo de 16:

- `RBP - 1` cumple alineación 1.
- `RBP - 8` es múltiplo de 4.
- `RBP - 16` es múltiplo de 8.

Los 16 bytes reservados contienen los tres objetos y 3 bytes de hueco entre `c` y `x`:

```text
RBP - 1       c (1 byte)
RBP - 2..-4   padding (3 bytes)
RBP - 5..-8   x (4 bytes)
RBP - 9..-16  d (8 bytes)
```

Este es un layout ilustrativo válido. GCC no está obligado a utilizar esos offsets concretos.

## 10.4. Una estructura local

```cpp
struct S {
    char c;
    double d;
};

void f() {
    S objeto;
}
```

En el entorno habitual:

```cpp
sizeof(S)  // 16
alignof(S) // 8
```

Si el compilador coloca el objeto en `RBP - 16` y RBP está alineado a 16:

- La base del objeto es `RBP - 16`, alineada a 8.
- `c` está en offset interno 0.
- Hay 7 bytes de padding interno.
- `d` está en offset interno 8.

El marco no recalcula la distribución interna de S; usa el tamaño, alineación y offsets del tipo S.

## 10.5. Padding de variables locales frente a padding de una estructura

| Tipo de espacio | Dónde aparece | ¿Parte de `sizeof` del objeto? |
|---|---|---|
| Padding interno de estructura | Entre miembros | Sí |
| Padding final de estructura | Al final de la estructura | Sí |
| Hueco entre slots locales | En el marco de pila | No pertenece a ningún objeto local individual |
| Padding de alineación de la pila | Para cumplir la ABI en llamadas | No pertenece a una variable C++ |

Una sola instrucción puede reservar el marco completo:

```asm
subq $64, %rsp
```

No se necesita una instrucción `SUB` por cada variable o por cada byte de padding. Luego se accede a cada objeto mediante su dirección u offset.

## 10.6. Alineación del objeto y alineación de la pila son requisitos diferentes

La alineación de un objeto depende de `alignof(tipo)`. La alineación del stack en los puntos de llamada depende de la ABI.

En llamadas ordinarias System V AMD64:

- Antes de `call`, RSP debe estar alineado a 16 bytes.
- `call` empuja una dirección de retorno de 8 bytes.
- Al entrar a la función, `RSP + 8` es múltiplo de 16.

Esto no significa que cada variable local tenga alineación de 16 bytes. Un `char` puede estar alineado a 1, un `int` a 4 y un `double` a 8, mientras la pila globalmente conserva la alineación exigida.

La ABI puede exigir fronteras mayores cuando ciertos vectores se pasan por stack: 32 bytes para determinadas llamadas con `__m256` en memoria y 64 bytes con `__m512` en memoria.

---

# PARTE III. FUNCIONES, ARGUMENTOS Y ABI SYSTEM V AMD64

# 11. Qué es una ABI y por qué importa

Una ABI (*Application Binary Interface*) define convenciones que deben coincidir entre módulos compilados por separado: cómo se pasan argumentos, cómo se devuelven valores, qué registros se preservan, cómo se organiza el stack, cómo se nombran símbolos y otros detalles binarios.

Esta guía utiliza la ABI System V AMD64, habitual en Linux x86-64. No debes mezclar sus reglas con las de Windows x64.

La ABI es un contrato entre el código llamador y el llamado. No es una instrucción del CPU.

# 12. Cómo se pasan argumentos escalares

# 12.1. Registros para argumentos enteros y punteros

Los seis registros de propósito general utilizados para los primeros argumentos de clase INTEGER son:

| Orden dentro de la clase INTEGER | Registro de 64 bits | Vista de 32 bits |
|---:|---|---|
| 1 | RDI | EDI |
| 2 | RSI | ESI |
| 3 | RDX | EDX |
| 4 | RCX | ECX |
| 5 | R8 | R8D |
| 6 | R9 | R9D |

Los enteros, booleanos, caracteres, punteros y direcciones de referencias suelen utilizar esta clase para argumentos escalares.

Ejemplo:

```cpp
int suma(int a, int b) {
    return a + b;
}
```

El llamador podría preparar:

```asm
movl $10, %edi
movl $20, %esi
call suma
```

Al entrar en `suma`, `a` está en EDI y `b` en ESI. No se necesita que el llamador los guarde previamente en el stack.

# 12.2. Registros para `float` y `double`

Los argumentos de clase SSE, como `float` y `double` escalares, utilizan registros XMM:

| Orden dentro de la clase SSE | Registro |
|---:|---|
| 1 | XMM0 |
| 2 | XMM1 |
| 3 | XMM2 |
| 4 | XMM3 |
| 5 | XMM4 |
| 6 | XMM5 |
| 7 | XMM6 |
| 8 | XMM7 |

Los grupos INTEGER y SSE se asignan mediante secuencias independientes. No se cuenta el segundo argumento global del prototipo para elegir XMM1: se cuenta el segundo argumento que necesita un registro SSE.

Ejemplo:

```cpp
void funcion(int a, double b, int c, float d);
```

Asignación habitual:

| Parámetro | Ubicación |
|---|---|
| `a` | EDI |
| `b` | XMM0 |
| `c` | ESI |
| `d` | XMM1 |

El segundo entero utiliza ESI, aunque sea el tercer parámetro escrito. El segundo flotante utiliza XMM1.

En funciones variádicas (por ejemplo, `printf`), la ABI tiene reglas adicionales: el llamador indica en `%al` cuántos registros vectoriales se han utilizado para argumentos de la llamada. No generalices los ejemplos de funciones ordinarias a `...`; las promociones por defecto de C también convierten `float` a `double` en argumentos variádicos.

# 12.3. Ejemplos de funciones sin optimización

Un compilador sin optimizaciones puede copiar los parámetros recibidos en registros a slots del stack para facilitar su tratamiento o depuración.

Por ejemplo, una salida conceptual de GCC con `-O0` puede parecerse a:

```asm
suma:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp

    movl %edi, -4(%rbp)   # copia local de a
    movl %esi, -8(%rbp)   # copia local de b

    movl -4(%rbp), %eax
    addl -8(%rbp), %eax

    leave
    ret
```

Las copias no son una exigencia de la ABI. La ABI define dónde llegan los parámetros; el compilador decide si los copia a memoria.

# 12.4. Registros de retorno

Para valores escalares habituales:

| Resultado | Registro habitual |
|---|---|
| Entero o puntero hasta 64 bits | RAX (o su subregistro) |
| `float` | XMM0 |
| `double` | XMM0 |

Los retornos de estructuras y clases tienen reglas adicionales. Algunas pueden devolverse en varios registros según su clasificación; otras utilizan un puntero oculto de retorno (*sret*) que el llamador proporciona.

No asumas que todos los tipos agregados se devuelven en RAX.

---

# 13. Argumentos que no caben en registros: área de argumentos en el stack

# 13.1. Cuándo se utilizan

Si una función necesita más registros de una clase que los disponibles, los argumentos que no pueden asignarse a registros se pasan por memoria.

Ejemplo con siete argumentos enteros:

```cpp
void funcion(int a, int b, int c, int d,
             int e, int f, int g);
```

| Argumento | Ubicación |
|---|---|
| `a` | EDI |
| `b` | ESI |
| `c` | EDX |
| `d` | ECX |
| `e` | R8D |
| `f` | R9D |
| `g` | Stack |

El séptimo argumento se prepara en el área de argumentos de la pila antes de `call`.

# 13.2. El llamador prepara los argumentos

Supongamos que RSP está alineado a 16 bytes antes de preparar esta llamada. Una forma ilustrativa de colocar un único `int` en el stack es:

```asm
# Reservar 16 bytes para el área de llamada.
# En el ejemplo: un slot de argumento y espacio de alineación.
subq $16, %rsp

# g ocupa los 4 bytes bajos de su slot.
movl $7, (%rsp)

# Primeros seis argumentos en registros.
movl $1, %edi
movl $2, %esi
movl $3, %edx
movl $4, %ecx
movl $5, %r8d
movl $6, %r9d

call funcion

# El llamador recupera el área de argumentos.
addq $16, %rsp
```

Para una llamada ordinaria, el área de argumentos debe dejar RSP alineado a 16 bytes inmediatamente antes de `call`. En el ejemplo, el slot del argumento ocupa 8 bytes y el resto es padding de alineación.

`CALL` no coloca los argumentos. Solo guarda la dirección de retorno y cambia RIP.

# 13.3. Qué ve la función al entrar

Supongamos que antes de reservar el área:

```text
RSP = 0x1000
```

Después de `subq $16, %rsp`:

```text
RSP = 0x0FF0
```

El área de salida del llamador es:

```text
0x0FF0–0x0FF7   slot del argumento g (los 4 bytes bajos contienen el int)
0x0FF8–0x0FFF   padding
```

Después de `call funcion`, la CPU coloca la dirección de retorno en 8 bytes por debajo:

```text
RSP = 0x0FE8
```

Al entrar en la función:

```text
0(%rsp)    dirección de retorno
8(%rsp)    slot de g
16(%rsp)   área de padding / siguiente espacio
```

Si la función utiliza el prólogo clásico:

```asm
pushq %rbp
movq %rsp, %rbp
```

queda:

```text
0(%rbp)    RBP anterior guardado
8(%rbp)    dirección de retorno
16(%rbp)   argumento g
```

Por tanto, en este ejemplo se puede leer la parte `int` de g mediante:

```asm
movl 16(%rbp), %eax
```

# 13.4. Dos argumentos en el stack

```cpp
void funcion(int a, int b, int c, int d,
             int e, int f, int g, int h);
```

El llamador puede reservar 16 bytes para los slots de `g` y `h`:

```asm
subq $16, %rsp
movl $7, 0(%rsp)      # g
movl $8, 8(%rsp)      # h

movl $1, %edi
movl $2, %esi
movl $3, %edx
movl $4, %ecx
movl $5, %r8d
movl $6, %r9d

call funcion
addq $16, %rsp
```

Al entrar en la función, antes del prólogo:

```text
0(%rsp)    dirección de retorno
8(%rsp)    slot de g
16(%rsp)   slot de h
```

Con RBP establecido mediante `pushq %rbp; movq %rsp,%rbp`:

```text
16(%rbp)   g
24(%rbp)   h
```

La disposición de los argumentos de pila se construye de modo que el primer argumento que se pasa por memoria quede más cerca de la dirección de retorno. El llamador suele reservar el área completa y escribir los valores en sus posiciones; no está obligado a utilizar una secuencia literal de `push`.

# 13.5. Alineación de los argumentos en el stack

Hay dos requisitos distintos:

1. Cada argumento pasado en memoria debe colocarse respetando su alineación requerida por la ABI.
2. El área de argumentos debe dejar la pila alineada en el límite requerido inmediatamente antes de `call`.

En llamadas ordinarias, ese límite es de 16 bytes. Con determinados argumentos vectoriales pasados por stack, la ABI puede exigir una alineación mayor (32 bytes para `__m256`, 64 bytes para `__m512`).

El padding de la zona de llamada no es una variable C++ ni forma parte de un argumento; solo mantiene las condiciones de alineación.

# 13.6. Argumentos de `float` y `double` cuando se agotan los XMM

Los primeros ocho argumentos de clase SSE que requieren registros se asignan a XMM0–XMM7. Si se requieren más, los que no pueden asignarse se pasan por memoria.

Los registros GP y SSE son bancos independientes. Puede haber registros generales disponibles y, aun así, un `double` tener que ir al stack porque se agotaron los XMM disponibles.

# 13.7. Estructuras y clases pasadas por valor

No se debe aplicar la regla simplista «los primeros seis parámetros van en registros». La ABI clasifica los argumentos en clases, en unidades de ocho bytes (*eightbytes*), y determina si cada parte utiliza registros GP, registros SSE u memoria.

En términos simplificados:

- Un agregado pequeño puede dividirse entre registros.
- Distintas partes pueden utilizar registros de clases diferentes.
- Si no hay suficientes registros para asignar todas las partes necesarias de un agregado, la ABI puede requerir que el argumento completo se pase por memoria.
- Agregados grandes o con clases especiales pueden pasar por memoria directamente.

Para implementar esta parte completamente, hay que seguir el algoritmo de clasificación de agregados de la psABI; no basta con contar parámetros.

# 13.8. Área de argumentos no es lo mismo que el marco local

| Región | Quién la prepara | Para qué sirve |
|---|---|---|
| Argumentos en registros | Llamador | Transmitir parámetros escalares/agregados por registros |
| Área de argumentos en stack | Llamador | Parámetros que la ABI asigna a memoria |
| Marco local del callee | Función llamada | Variables locales, copias de parámetros, temporales y registros salvados |

Los argumentos que llegan por registros no consumen automáticamente espacio en el marco local. Los que llegan por stack ya tienen espacio en el área de argumentos del llamador. El callee puede reservar otras posiciones para copiarlos si lo decide.

Por tanto, no sumes automáticamente todos los tamaños de parámetros al `subq $N, %rsp` del marco local.

---

# 14. Punteros y referencias como argumentos

# 14.1. Puntero

```cpp
void incrementar(int* p) {
    (*p)++;
}

int main() {
    int x = 10;
    incrementar(&x);
}
```

El llamador obtiene la dirección de x y la pasa:

```asm
leaq -4(%rbp), %rdi    # Dirección de x
call incrementar
```

El método recibe la dirección en RDI y accede al objeto:

```asm
incrementar:
    addl $1, (%rdi)
    ret
```

`%rdi` contiene una dirección; `(%rdi)` es el entero almacenado allí.

# 14.2. Referencia

```cpp
void incrementar(int& r) {
    r++;
}

int main() {
    int x = 10;
    incrementar(x);
}
```

A nivel de llamada, la referencia se implementa habitualmente pasando la dirección del objeto referenciado:

```asm
leaq -4(%rbp), %rdi
call incrementar_ref
```

Y dentro de la función:

```asm
incrementar_ref:
    addl $1, (%rdi)
    ret
```

En este ejemplo, el código generado es prácticamente igual al del puntero.

# 14.3. Puntero y referencia: representación frente a semántica

| Característica | `int*` | `int&` |
|---|---|---|
| Cómo llega un parámetro ordinario | Dirección | Dirección del referido |
| Puede ser nulo | Sí | Una referencia válida debe referir a un objeto o función |
| Puede reasignarse | Sí | No puede volver a vincularse después de inicializarse |
| Sintaxis de acceso | `*p` | `r` |
| Cambiar el valor del objeto | `*p = ...` | `r = ...` |

Ejemplo:

```cpp
int a = 10;
int b = 20;

int* p = &a;
int& r = a;

p = &b;  // p ahora apunta a b
r = b;   // asigna 20 a a; r sigue referenciando a
```

**Precisión importante:** a nivel de ABI, un parámetro `int&` se transmite mediante una dirección. Eso no convierte a la referencia en un objeto puntero de C++ ni significa que `sizeof(int&)` sea 8; `sizeof` aplicado a una expresión de tipo referencia obtiene el tamaño del objeto referido. La referencia no es un objeto independiente que el programa pueda redirigir.

# 14.4. Referencia a `double` frente a `double` por valor

```cpp
void porValor(double x);
void porReferencia(double& x);
void porPuntero(double* x);
```

En una llamada ordinaria:

| Parámetro | Qué se transmite | Ubicación habitual si hay registro disponible |
|---|---|---|
| `double x` | Valor binary64 | XMM0 (según orden SSE) |
| `double& x` | Dirección del objeto | RDI (según orden INTEGER) |
| `double* x` | Dirección del objeto | RDI (según orden INTEGER) |

Para modificar un `double` por referencia, la función puede recibir su dirección en RDI y escribir el valor que está en XMM0:

```asm
# RDI = dirección del double
# XMM0 = valor double que se va a almacenar
movsd %xmm0, (%rdi)
```

Si ya no hay registros GP disponibles, el puntero o dirección de referencia se pasa por memoria igual que otros argumentos INTEGER.

---

# PARTE IV. CLASES, MÉTODOS Y SÍMBOLOS C++

# 15. Dónde se almacena el código de los métodos

Los métodos de una clase no se copian dentro de cada objeto. El código de un método se genera como código de función y se coloca en una sección ejecutable, normalmente `.text` o una subsección de `.text`.

Los objetos almacenan sus atributos y, en ciertas implementaciones, información auxiliar como un `vptr`; no guardan una copia de las instrucciones del método.

Ejemplo:

```cpp
class Persona {
public:
    int edad;

    void cumplirAnios() {
        edad++;
    }
};

Persona p1;
Persona p2;
p1.edad = 20;
p2.edad = 30;
p1.cumplirAnios();
p2.cumplirAnios();
```

Conceptualmente hay dos objetos con sus propios datos, pero una implementación compartida de `cumplirAnios`.

Crear 1000 objetos Persona no crea automáticamente 1000 copias del código de sus métodos.

# 16. `this`: parámetro implícito de un método no estático

Cuando se escribe:

```cpp
p1.cumplirAnios();
```

el método necesita saber qué objeto debe modificar. C++ proporciona el puntero implícito `this` en los métodos no estáticos.

Conceptualmente, el método puede entenderse como si recibiera un parámetro oculto que apunta a `p1`:

```cpp
void cumplirAnios() {
    edad++;
}
```

equivale conceptualmente a operar sobre `this->edad`.

En System V AMD64, para un método sencillo con retorno escalar y sin parámetros ocultos adicionales, `this` se pasa como primer argumento de clase INTEGER, normalmente en RDI.

Una traducción conceptual:

```asm
# RDI contiene la dirección del objeto Persona.
Persona_cumplirAnios:
    addl $1, (%rdi)     # edad está en offset 0 en este ejemplo
    ret
```

El llamador puede generar:

```asm
leaq -16(%rbp), %rdi    # dirección de p1
call simbolo_del_metodo

leaq -32(%rbp), %rdi    # dirección de p2
call simbolo_del_metodo
```

La función es la misma; cambia la dirección del objeto recibido.

Si una clase tiene otros atributos antes de `edad`, el desplazamiento no será cero. El compilador debe usar el offset real del atributo dentro del objeto.

# 17. Métodos con argumentos

```cpp
class Persona {
public:
    int edad;
    void cambiarEdad(int nuevaEdad) {
        edad = nuevaEdad;
    }
};
```

Al ejecutar:

```cpp
p1.cambiarEdad(25);
```

Para este ejemplo:

| Argumento | Registro |
|---|---|
| `this` | RDI |
| `nuevaEdad` | ESI |

El llamador:

```asm
leaq -16(%rbp), %rdi
movl $25, %esi
call simbolo_cambiarEdad
```

El método:

```asm
simbolo_cambiarEdad:
    movl %esi, (%rdi)
    ret
```

El primer parámetro es `this`; los siguientes se asignan según sus clases y la ABI. Si se agotan los registros, los parámetros restantes o los agregados afectados pueden ir por stack.

**Parámetro oculto de retorno:** si un método devuelve por valor un agregado que la ABI no puede devolver en registros y requiere un puntero de retorno (*sret*), puede aparecer un parámetro oculto adicional. En ese caso, la ubicación de `this` y de los argumentos explícitos puede desplazarse; no supongas que `this` siempre está en RDI para absolutamente todas las firmas. La asignación exacta debe seguir la clasificación y las reglas de retorno de la psABI.

# 18. Métodos `static`

```cpp
class Matematica {
public:
    static int duplicar(int x) {
        return x * 2;
    }
};
```

Se puede invocar sin crear un objeto:

```cpp
int resultado = Matematica::duplicar(10);
```

Un método `static` no tiene `this`, porque no opera sobre una instancia concreta. Su argumento `x` es el primer argumento ordinario y llega normalmente en EDI.

Una implementación posible:

```asm
Matematica_duplicar:
    leal (%rdi,%rdi), %eax
    ret
```

| Característica | Método no estático | Método `static` |
|---|---|---|
| Tiene `this` | Sí | No |
| Requiere instancia para una llamada normal | Sí | No |
| Acceso directo a miembros no estáticos | Sí | No |
| Código en sección ejecutable | Sí | Sí |

Los métodos `static` son funciones en cuanto a su convención de llamada, con el nombre asociado al ámbito de la clase.

## 18.1. Miembros de datos `static` de una clase

No confundas un método `static` con un atributo `static`. Ambos pertenecen al ámbito de la clase, pero el atributo sí es un objeto con almacenamiento.

```cpp
class Contador {
public:
    static int total;
    int valor;
};

int Contador::total = 0;
```

- `valor` forma parte de cada objeto `Contador`. Dos instancias tienen dos campos `valor` independientes.
- `total` es una única variable compartida por la clase; no se copia dentro de cada instancia.
- `sizeof(Contador)` no incluye el espacio de `total`. En este ejemplo, normalmente solo cuenta `valor` (más cualquier padding que corresponda).
- La definición de `total` tiene duración de almacenamiento estática y suele ubicarse en `.bss` si empieza en cero, o en `.data` si tiene un inicializador estático no nulo.

Una referencia conceptual desde ensamblador podría ser:

```asm
movl Contador_total(%rip), %eax   # leer el miembro estático
movl $5, Contador_total(%rip)    # escribirlo
```

`Contador_total` es un nombre ilustrativo. Con GCC/Itanium C++ ABI el símbolo real puede estar codificado, por ejemplo `_ZN8Contador5totalE`; compruébalo con `nm -C`. Desde C++17 también existe `inline static` para definir ciertos miembros estáticos directamente dentro de la clase sin una definición externa separada.

Un método `static`, en cambio, no es un dato: es código ejecutable en `.text` y no recibe `this`.

# 19. Métodos virtuales, `vptr` y `vtable`

# 19.1. Por qué existe el despacho virtual

```cpp
class Animal {
public:
    virtual void sonido() { }
};

class Perro : public Animal {
public:
    void sonido() override { }
};

Animal* animal = new Perro();
animal->sonido();
```

La llamada se hace a través de un puntero de tipo `Animal*`, pero el objeto real es `Perro`. El mecanismo virtual permite seleccionar la implementación correspondiente al tipo dinámico.

# 19.2. Organización habitual

En implementaciones GCC que siguen el Itanium C++ ABI, se generan tablas virtuales (*vtables*) asociadas a las clases polimórficas. Los objetos suelen contener un puntero interno (`vptr`) que permite llegar a la tabla apropiada.

Esquema simplificado:

```text
vtable de Animal:
    entrada para sonido -> Animal::sonido

vtable de Perro:
    entrada para sonido -> Perro::sonido

objeto Perro:
    vptr -> vtable de Perro
    atributos...
```

La tabla se comparte; no se copia completa dentro de cada objeto. El objeto suele contener el puntero que permite encontrarla.

La ubicación exacta del `vptr` no debe asumirse universalmente como offset cero: depende del layout y de la herencia. En herencia múltiple pueden existir más de un subobjeto base y ajustes de `this`.

# 19.3. Llamada virtual conceptual

En un caso sencillo de herencia simple donde el `vptr` se encuentra al principio del objeto:

```asm
# RDI = dirección del objeto
movq (%rdi), %rax       # cargar vptr
call *OFFSET(%rax)     # llamar indirectamente a la entrada elegida
```

Esto es solo un esquema. El layout real de las vtables incluye metadatos y el desplazamiento de la entrada depende de la ABI. En herencia múltiple pueden aparecer *thunks* que ajustan `this` antes de llegar al método final.

Las vtables de GCC suelen ubicarse en secciones de datos de solo lectura o de datos con relocalizaciones que pasan a ser de solo lectura después de la carga (por ejemplo, `.data.rel.ro`). No debe asumirse que toda vtable se emite literalmente en una sección `.rodata` sin relocalizaciones.

# 20. Nombres de métodos y `::`: name mangling

El operador `::` pertenece a la sintaxis de C++ para resolver ámbitos:

```cpp
Persona::cumplirAnios
```

No es una sintaxis especial para etiquetas de GAS. Al compilar, GCC codifica los nombres C++ mediante *name mangling* para producir símbolos compatibles con el enlazador.

Por ejemplo, un método como:

```cpp
class Persona {
public:
    void cumplirAnios();
};
```

puede tener un símbolo ABI como:

```asm
_ZN7Persona12cumplirAniosEv:
    # cuerpo del método
    ret
```

Lectura general del nombre:

- `_Z`: marca de nombre codificado.
- `N...E`: nombre anidado (clase y miembro).
- `7Persona`: nombre de clase de 7 caracteres.
- `12cumplirAnios`: nombre de método de 12 caracteres.
- `v`: lista de parámetros vacía.

Una llamada puede referenciar ese símbolo:

```asm
call _ZN7Persona12cumplirAniosEv
```

Para inspeccionar símbolos puedes usar:

```bash
nm programa.o
nm -C programa.o
c++filt _ZN7Persona12cumplirAniosEv
```

`-C` o `c++filt` demangla los símbolos para mostrarlos de manera legible.

**No todos los símbolos C++ se codifican igual:** las funciones y los métodos incluyen información de nombre y parámetros; ciertos nombres de variables globales de espacio de nombres global pueden permanecer sin mangling según el ABI, mientras que miembros estáticos y entidades dentro de namespaces pueden tener nombres codificados. No inventes el nombre del símbolo: compruébalo o implementa el ABI.

Para un compilador propio que controla todo el entorno puedes elegir nombres como `Persona_cumplirAnios`. Para interoperar con GCC/C++ y sus bibliotecas debes seguir el ABI correspondiente.

---

# PARTE V. VARIABLES GLOBALES Y DATOS DEL PROGRAMA

# 21. Dónde se almacenan las variables globales

Una variable global (o de duración de almacenamiento estática) no se reserva en el marco de una llamada ni mediante `subq %rsp`. Tiene almacenamiento durante la vida del programa.

El compilador coloca normalmente estos objetos en secciones de datos del archivo objeto y del ejecutable ELF.

## 21.1. Secciones habituales

| Sección | Uso habitual |
|---|---|
| `.text` | Instrucciones de funciones y métodos. |
| `.rodata` | Constantes y datos de solo lectura. |
| `.data` | Variables globales/estáticas con datos iniciales almacenados. |
| `.bss` | Variables globales/estáticas que comienzan en cero. |
| `.init_array` | Punteros a funciones de inicialización dinámica. |

La ubicación exacta de un símbolo puede depender del compilador, del enlazador, de atributos y de opciones de compilación.

# 21.2. Global con valor conocido

```cpp
int edad = 25;
double promedio = 17.5;
float descuento = 0.15f;
```

Una salida esquemática:

```asm
.section .data
.balign 4
.globl edad
edad:
    .long 25

.balign 8
.globl promedio
promedio:
    .double 17.5

.balign 4
.globl descuento
descuento:
    .float 0.15
```

Las etiquetas identifican direcciones. `.long`, `.double` y `.float` emiten los bytes iniciales.

No todos los nombres que ves en una salida GCC tienen que ser exactamente `edad` o `promedio`: depende del linkage y del ámbito. El ejemplo muestra la idea de almacenamiento.

# 21.3. Global sin inicializador explícito

```cpp
int contador;
double resultado;
```

Los objetos globales de duración estática que no tienen otro inicializador reciben inicialización a cero. Una representación habitual es `.bss`:

```asm
.section .bss
.balign 4
contador:
    .zero 4

.balign 8
resultado:
    .zero 8
```

`.zero N` solicita espacio de N bytes inicializado a cero. En ELF, `.bss` suele ser una sección `NOBITS`: el archivo no necesita contener todos los bytes cero, pero el cargador/runtime prepara esa región como cero.

# 21.4. Constantes globales y `.rodata`

Una constante que puede tratarse como dato de solo lectura puede aparecer en `.rodata`. Sin embargo, la sección exacta es decisión del compilador y puede verse afectada por relocalizaciones, linkage y opciones.

# 21.5. Alineación de variables globales sin conocer su dirección absoluta

No hace falta conocer la dirección final de la sección. El ensamblador registra la alineación y el enlazador coloca las secciones respetándola.

En GAS:

```asm
.section .data
.long 25
.balign 8
promedio:
    .double 17.5
```

`.balign 8` ajusta la posición actual para que la siguiente dirección dentro de la sección sea múltiplo de 8. El enlazador también debe respetar la alineación de la sección en el archivo ELF.

Usar `.balign` evita la ambigüedad entre variantes históricas de `.align`, cuyo significado puede depender de la plataforma.

# 21.6. Acceder a una global desde código

En un modelo de código habitual, una variable local al ejecutable puede accederse mediante direccionamiento relativo a RIP:

```asm
movl edad(%rip), %eax       # leer la variable
movl %eax, edad(%rip)       # escribir la variable
leaq edad(%rip), %rdi       # obtener su dirección
```

Diferencias:

- `edad(%rip)` como operando fuente/destino de `MOV` accede al contenido.
- `leaq edad(%rip), %rdi` calcula la dirección de la variable y la guarda en RDI.

En código PIE o bibliotecas compartidas, un símbolo externo/interponible puede necesitar acceso mediante GOT, como `symbol@GOTPCREL(%rip)`. No todas las referencias globales se emiten siempre con la forma directa.

---

# 22. Inicialización global: estática y dinámica

# 22.1. Inicialización estática

Las variables de duración estática reciben inicialización estática antes de la inicialización dinámica. Cuando el valor se puede determinar durante la compilación, el compilador puede emitir directamente los bytes iniciales.

```cpp
int edad = 25;
float precio = 1.5f;
```

Una representación habitual es `.data`.

Las variables que empiezan en cero pueden representarse en `.bss`.

No toda expresión que visualmente parece una llamada fuerza una llamada en tiempo de ejecución: si se puede evaluar como una expresión constante según las reglas de C++, puede realizarse inicialización constante.

# 22.2. Inicialización dinámica

Considera:

```cpp
int obtenerEdad() {
    return 25;
}

int edad = obtenerEdad();

int main() {
    return edad;
}
```

Si la inicialización no se puede resolver en compilación, se necesita ejecutar código antes de que el programa utilice normalmente esa variable.

Se requieren dos elementos:

1. Espacio de almacenamiento para `edad`.
2. Código que ejecute `obtenerEdad()` y escriba el resultado en ese espacio.

El almacenamiento puede estar inicialmente en `.bss` y la inicialización puede representarse conceptualmente:

```asm
.section .text
inicializar_globales:
    call simbolo_obtenerEdad
    movl %eax, edad(%rip)
    ret
```

Este fragmento ilustra el mecanismo; GCC normalmente genera funciones auxiliares con nombres como `__static_initialization_and_destruction_0` y `_GLOBAL__sub_I_...`.

# 22.3. Cómo se ejecuta antes de `main`

GCC y el runtime C++ registran funciones de inicialización mediante mecanismos ELF, normalmente entradas en `.init_array`:

```asm
.section .init_array,"aw"
.balign 8
.quad funcion_inicializadora
```

La entrada contiene la dirección de una función que debe ejecutarse durante el arranque. El código de inicio del runtime y el cargador/dynamic linker cooperan para procesar las inicializaciones antes de que `main` empiece a ejecutarse.

Secuencia conceptual:

1. El sistema inicia el proceso y prepara sus regiones de memoria.
2. Los datos con inicialización estática quedan establecidos; `.bss` comienza en cero.
3. El runtime ejecuta las funciones de inicialización registradas.
4. Se ejecuta `main`.

# 22.4. Variable global que es un objeto de clase

```cpp
class Persona {
public:
    int edad;
    Persona() { edad = 25; }
};

Persona p;
```

Se reserva primero el almacenamiento para `p`. Después, la inicialización dinámica debe ejecutar el constructor si no se ha podido realizar como inicialización estática.

Conceptualmente:

```asm
inicializar_persona:
    leaq p(%rip), %rdi     # this = dirección de p
    call simbolo_constructor
    ret
```

El constructor se genera como código ejecutable y recibe la dirección del objeto mediante `this`.

Los destructores de objetos globales también requieren mecanismos de finalización/registro durante el cierre del programa.

**Orden de inicialización:** dentro de una misma unidad de traducción hay reglas de orden, pero entre unidades de traducción diferentes no debes asumir un orden global simple. Esto puede producir problemas de orden de inicialización global (*static initialization order fiasco*).

# 22.5. Resumen de ubicaciones

| Elemento | Ubicación habitual |
|---|---|
| Global con inicialización estática no nula | `.data` |
| Global que empieza en cero | `.bss` |
| Constante compartida | `.rodata` o subsección de constantes |
| Código de inicialización dinámica | `.text` |
| Dirección de función inicializadora | `.init_array` |
| Atributos de objeto global | Dentro del almacenamiento global del objeto |
| Miembro de datos `static` de clase | Almacenamiento estático compartido, no dentro de cada instancia |
| Código de constructor/destructor | Código de función en `.text` |

---

# PARTE VI. CONEXIÓN ENTRE LOS TEMAS Y GENERACIÓN DE CÓDIGO

# 23. Ejemplo integrador: tipos, función, referencia y método

Considera:

```cpp
struct Medicion {
    int codigo;
    double valor;
};

class Sensor {
public:
    int id;
    void actualizar(double nuevoValor, Medicion& m) {
        m.valor = nuevoValor;
        id = m.codigo;
    }
};

float escalar(float x) {
    return x * 1.5f;
}
```

Primero se calculan los tipos:

- `int`: 4 bytes, alineación habitual 4.
- `double`: 8 bytes, alineación habitual 8.
- `Medicion`: `codigo` en offset 0; padding 4 bytes; `valor` en offset 8; tamaño 16 y alineación 8.
- `Sensor`: contiene su atributo `id`; en este ejemplo sin métodos virtuales ni bases, el objeto solo necesita almacenar ese atributo.

La función miembro `actualizar` tiene un `this` implícito. Para una versión simple con los argumentos indicados, la ABI puede asignar:

| Parámetro | Clase | Ubicación |
|---|---|---|
| `this` | INTEGER | RDI |
| `nuevoValor` | SSE | XMM0 |
| `m` (referencia) | INTEGER | RSI |

¿Por qué `m` no va en XMM1? Porque `Medicion&` se pasa como una dirección, que es un argumento INTEGER.

Dentro del método, para acceder a `m.valor`:

1. RSI contiene la dirección de la estructura `Medicion`.
2. `valor` está en offset 8 dentro de `Medicion`.
3. Se almacena el `double` de XMM0 en `8(%rsi)`.

Una secuencia conceptual:

```asm
# RDI = this (Sensor*)
# XMM0 = nuevoValor (double)
# RSI = &m (Medicion*)

movsd %xmm0, 8(%rsi)       # m.valor = nuevoValor
movl 0(%rsi), %eax         # leer m.codigo
movl %eax, (%rdi)          # this->id = m.codigo
ret
```

Es un ejemplo pedagógico. GCC puede elegir otros registros temporales, offsets, prólogos y símbolos codificados.

# 24. Otro ejemplo: constante flotante y función global

C++:

```cpp
float ajustar(float x) {
    return x + 2.5f;
}
```

Una salida conceptual con constante en memoria:

```asm
.section .text
.globl ajustar
ajustar:
    addss .LC0(%rip), %xmm0
    ret

.section .rodata
.balign 4
.LC0:
    .float 2.5
```

- El argumento `x` llega en XMM0.
- `.LC0` contiene los bits de 2.5f.
- `ADDSS` suma un único `float` escalar.
- El resultado vuelve en XMM0.
- La etiqueta no representa una variable local; representa una constante estática compartible.

# 25. Cómo inspeccionar lo que genera GCC

Crea un archivo `ejemplos.cpp` y compáralo con el ensamblador.

```bash
# Ensamblador legible, sin optimización y conservando frame pointer
 g++ -O0 -fno-omit-frame-pointer -S ejemplos.cpp -o ejemplos-O0.s

# Ensamblador optimizado
 g++ -O2 -S ejemplos.cpp -o ejemplos-O2.s

# Objeto relocatable
 g++ -O0 -c ejemplos.cpp -o ejemplos.o

# Tabla de símbolos, nombres demanglados
 nm -C ejemplos.o

# Secciones ELF
 readelf -S ejemplos.o

# Desensamblado y relocalizaciones
 objdump -drC ejemplos.o
```

Qué buscar:

- `movl`, `movq`, `movss`, `movsd`.
- `addss`, `addsd`, `addps`, `addpd`.
- `cvtsi2ss`, `cvtsi2sd`, `cvttss2si`, `cvttsd2si`.
- `.data`, `.bss`, `.rodata`, `.init_array`.
- `subq $N, %rsp` y offsets negativos de `%rbp`.
- Argumentos en RDI–R9 y XMM0–XMM7.
- Símbolos `_Z...` y nombres demanglados.
- `vtable` / `_ZTV...` cuando hay métodos virtuales.

`-O0` facilita observar variables y copias en memoria, pero no significa que GCC tenga que materializar cada variable exactamente como una ubicación separada. `-O2` puede eliminar variables, plegar constantes, mantener valores en registros, insertar métodos (*inlining*) y reorganizar el marco.

---

# PARTE VII. QUÉ DEBE HACER UN COMPILADOR PROPIO

# 26. Información que conviene conservar por tipo

Una representación interna de tipos debería permitir consultar al menos:

```text
TypeInfo:
    kind                 // integer, float, pointer, array, struct, class, reference...
    size                 // sizeof en bytes
    alignment            // alignof en bytes
    signedness           // signed / unsigned / no aplica
    element_type         // para punteros y arreglos
    element_count        // para arreglos de tamaño conocido
    fields               // para struct/class
    qualifiers           // const, volatile, etc.
```

Para `float` y `double`, el backend necesita distinguir formatos binary32 y binary64. Para referencias, debe conservar la semántica de C++ aunque el ABI las transmita como direcciones.

# 27. Layout de estructuras y clases

Para cada miembro:

1. Obtener `sizeof(miembro)`.
2. Obtener `alignof(miembro)`.
3. Redondear el offset actual al siguiente múltiplo de su alineación.
4. Registrar el offset del miembro.
5. Avanzar por su tamaño.
6. Al terminar, determinar la alineación del agregado (habitualmente el máximo de sus miembros, considerando `alignas` y reglas especiales).
7. Redondear el tamaño final a un múltiplo de la alineación del agregado.

Fórmula de redondeo:

\[
align\_up(x,a)=\left\lceil\frac{x}{a}\right\rceil a
\]

No implementes esta regla simplificada como algoritmo universal para clases con herencia compleja, bit-fields, `packed` u otras extensiones. Esos casos requieren reglas adicionales del ABI y del lenguaje.

# 28. Layout de variables locales

Un gestor de marco puede mantener por variable:

```text
LocalSlot:
    type
    size
    alignment
    offset_from_frame_base
    lifetime_start
    lifetime_end
```

Para cada objeto materializado:

- Elegir una dirección que cumpla su alineación.
- Evitar solapamientos entre objetos cuyos tiempos de vida requieren almacenamiento simultáneo.
- Respetar offsets internos de estructuras y arreglos.
- Reservar el marco total y cumplir la alineación de la pila en llamadas.
- Permitir slots reutilizados cuando los tiempos de vida no se superponen.

No asumas que todos los identificadores fuente requieren un slot real. Algunos valores pueden vivir solo en registros o desaparecer por optimización.

# 29. Clasificación de argumentos

Un generador para System V AMD64 debe distinguir:

- Clase INTEGER: enteros, punteros y direcciones de referencias.
- Clase SSE: `float`, `double` y ciertos vectores/agregados.
- Agregados: clasificar por *eightbytes* y seguir las reglas completas de la psABI.
- Argumentos MEMORY: ubicarlos en el área de argumentos del stack con la alineación requerida.

Además debe:

- Mantener contadores separados para registros GP y XMM.
- Considerar el `this` implícito en métodos no estáticos.
- Considerar parámetros ocultos cuando una devolución agregada requiere un puntero `sret`.
- Reservar el área de argumentos en el llamador.
- Colocar el padding necesario.
- Alinear RSP antes de `call`.
- Recuperar el área de argumentos después de la llamada.

No basta con asignar el argumento número siete al stack sin mirar su clase: los bancos INTEGER y SSE avanzan independientemente.

# 30. Constant pool

Un conjunto de constantes puede almacenar:

```text
ConstantEntry:
    type                 // float32, float64...
    bit_pattern          // representación IEEE exacta
    label                // .LC0, .LC1...
    alignment
```

El visitante de expresiones consulta el pool y recibe una etiqueta. Al final, el emisor escribe `.section .rodata`, aplica `.balign` y emite las entradas.

# 31. Métodos y clases

El backend necesita:

- Generar el código de métodos como funciones.
- Incluir `this` como argumento implícito en los métodos no estáticos.
- Omitir `this` para métodos `static`.
- Calcular offsets de atributos en el layout del objeto.
- Generar constructores y destructores.
- Generar nombres de símbolos compatibles con el ABI si se requiere interoperabilidad.
- Implementar despacho virtual, vtables, vptrs y ajustes de `this` si soporta polimorfismo.
- Distinguir los miembros de datos `static` (un objeto compartido fuera de cada instancia) de los atributos no estáticos.

# 32. Variables globales

El emisor debe separar:

- Datos inicializados estáticamente.
- Datos inicialmente cero.
- Constantes de solo lectura.
- Código de inicialización dinámica.
- Entradas de inicialización que el runtime debe ejecutar antes de `main`.

Además, cada símbolo debe tener linkage y visibilidad adecuados; no todos los símbolos C++ se nombran igual que el identificador fuente.

---

# 33. Resumen general

| Tema | Regla esencial |
|---|---|
| Tipo C++ | Define el significado y reglas del dato. |
| `sizeof` | Tamaño en bytes. |
| `alignof` | Alineación requerida en bytes. |
| Entero signed | Complemento a dos en x86-64. |
| Entero unsigned | Interpretación sin signo de los bits. |
| `MOVSX` | Extiende con signo. |
| `MOVZX` | Extiende con ceros. |
| `float` | Normalmente IEEE 754 binary32. |
| `double` | Normalmente IEEE 754 binary64. |
| XMM | Registro de 128 bits que puede contener datos escalares o empaquetados. |
| `addss` / `addsd` | Suma un `float` / `double` escalar. |
| `addps` / `addpd` | Suma cuatro `float` / dos `double` empaquetados. |
| `movd` | Copia 32 bits entre GPR/memoria y XMM. |
| `movq` GPR–XMM | Copia 64 bits entre GPR/memoria y XMM. |
| `.float` / `.double` | Directivas GAS que emiten constantes. |
| `.rodata` | Datos de solo lectura; puede alojar constantes flotantes. |
| Constant pool | Tabla para registrar/reutilizar constantes y sus etiquetas. |
| Padding de estructura | Bytes que satisfacen alineación de miembros y tamaño del agregado. |
| Alineación de estructura | Habitualmente la mayor alineación de sus miembros. |
| Tamaño de estructura | Debe ser múltiplo de su alineación. |
| Variables locales | Cada objeto materializado debe cumplir su alineación; el compilador decide slots. |
| ABI System V | Define cómo se pasan argumentos y resultados. |
| Puntero como argumento | Se pasa una dirección. |
| Referencia como argumento | Se transmite la dirección del objeto referido. |
| `this` | Dirección implícita del objeto en un método no estático. |
| Método `static` | No tiene `this`. |
| Método virtual | Suele usar `vptr`/`vtable` en ABI de GCC. |
| Variable global | Tiene almacenamiento estático, no un slot del marco local. |
| `.data` | Datos inicializados. |
| `.bss` | Datos que empiezan en cero. |
| `.init_array` | Registro de funciones de inicialización. |

---

# 34. Glosario

| Término | Significado |
|---|---|
| **ABI** | Contrato binario entre módulos compilados. |
| **LP64** | Modelo donde `long` y punteros tienen 64 bits. |
| **Complemento a dos** | Representación de enteros con signo. |
| **Extensión de signo** | Replicar el bit de signo al ampliar un entero. |
| **IEEE 754** | Estándar de formatos de punto flotante. |
| **binary32** | Formato habitual de `float` de 32 bits. |
| **binary64** | Formato habitual de `double` de 64 bits. |
| **XMM** | Registro multimedia/vectorial de 128 bits. |
| **Scalar** | Operación sobre un único elemento numérico. |
| **Packed** | Varios elementos independientes en un registro. |
| **SIMD** | Una instrucción procesa varios datos. |
| **Constant pool** | Conjunto de constantes reutilizables. |
| **Sección ELF** | Región lógica del archivo objeto/ejecutable, como `.text` o `.data`. |
| **Símbolo** | Nombre asociado a una dirección o entidad del archivo objeto. |
| **Alineación** | Restricción sobre las direcciones iniciales válidas de un objeto. |
| **Padding** | Bytes de relleno para cumplir alineación o tamaño. |
| **Offset** | Desplazamiento desde una dirección base. |
| **Eightbyte** | Unidad de 8 bytes usada por el algoritmo de clasificación de la ABI AMD64. |
| **`this`** | Puntero implícito al objeto de un método no estático. |
| **Name mangling** | Codificación de nombres C++ para símbolos binarios. |
| **vptr** | Puntero interno que suele apuntar a la tabla virtual del objeto. |
| **vtable** | Tabla usada para despacho de métodos virtuales en implementaciones habituales. |
| **GOT** | Global Offset Table, usada para resolver direcciones de símbolos en código relocatable/PIC. |
| **PLT** | Procedure Linkage Table, usada habitualmente para llamadas a símbolos dinámicos. |
| **`init_array`** | Sección ELF con direcciones de rutinas de inicialización. |
| **Sret** | Puntero oculto usado para devolver ciertos objetos agregados. |

---

# 35. Fuentes técnicas y alcance de las reglas

Esta guía adopta como entorno concreto Linux x86-64 con GCC/GAS y ABI System V AMD64. Para los detalles que dependen del compilador o de la ABI, consulta las especificaciones correspondientes:

1. [System V AMD64 ABI / x86-64 psABI](https://gitlab.com/x86-psABIs/x86-64-ABI) — clasificación de argumentos, registros, stack, alineación, retorno y convenciones de llamada.
2. [Itanium C++ ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html) — layout de objetos C++, nombres codificados, constructores, destructores, vtables y llamadas virtuales. GCC en Linux suele seguir este ABI C++ sobre la ABI de plataforma.
3. [GCC: x86 Options](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html) — tamaños/alineaciones de `long double`, alineación de stack y opciones que pueden alterar el ABI.
4. [GNU Assembler Manual](https://sourceware.org/binutils/docs/as/) — directivas de sección, alineación, emisión de datos y sintaxis GAS.
5. [Intel Software Developer's Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) — semántica de las instrucciones x86, SSE y registros.
6. [cppreference: `sizeof`](https://en.cppreference.com/w/cpp/language/sizeof) y [cppreference: `alignof`](https://en.cppreference.com/w/cpp/language/alignof) — comportamiento de C++ para tamaño y alineación.

**Límite importante:** los tamaños de tipos, layout exacto de clases, nombres de símbolos, ubicación de secciones y secuencias de instrucciones mostrados como ejemplos dependen del target, ABI, compilador y opciones. Los ejemplos indican las convenciones habituales del entorno indicado; cuando se muestra un fragmento «conceptual», no se afirma que GCC tenga que producir exactamente esas mismas instrucciones.


---

# 36. Precisiones y correcciones que evitan reglas falsas

Esta sección reúne distinciones que conviene conservar al estudiar o implementar un compilador. Algunas respuestas anteriores utilizaban ejemplos simplificados; estas son las reglas completas que debes recordar.

1. **Una referencia no es un puntero como objeto C++.** Un parámetro `T&` se transmite normalmente mediante la dirección del objeto, pero eso no significa que una referencia tenga un `sizeof` independiente de 8 bytes ni que pueda reasignarse como un puntero.
2. **No todos los argumentos ocupan una posición en el stack.** Los escalares INTEGER y SSE se pasan primero en bancos de registros independientes. Los parámetros que llegan en registros no se copian obligatoriamente al marco local; GCC puede copiarlos con `-O0`, pero es una decisión del compilador.
3. **No basta contar el número global de parámetro.** Para llamadas mixtas, INTEGER y SSE avanzan por separado. Los agregados se clasifican por *eightbytes*. Si no hay registros suficientes para todas las partes de un agregado, la ABI puede revertir esa asignación y pasarlo completo por memoria.
4. **El padding de una estructura no es el mismo que el padding de un marco.** El primero forma parte de `sizeof(estructura)`. Los huecos entre variables locales o para alinear el área de llamada no pertenecen a ningún objeto local individual.
5. **Un offset negativo no garantiza por sí solo alineación.** Debes conocer la alineación de la base (por ejemplo RBP) y verificar `(base + offset) % alignof(T) == 0`.
6. **El orden de variables locales no tiene que coincidir con el orden fuente.** El compilador puede reordenar slots, usar registros, eliminar variables o reutilizar almacenamiento cuando los tiempos de vida no se superponen. Los offsets mostrados en ejemplos son diseños posibles, no una promesa sobre GCC.
7. **La alineación de la pila no obliga a alinear cada variable a 16 bytes.** La ABI exige una frontera para el área de llamada; cada objeto conserva su alineación propia (`char`, `int`, `double`, `alignas`, etc.).
8. **`float` y `double` no tienen inmediatos flotantes SSE ordinarios.** Se cargan desde memoria, se construyen copiando sus bits mediante GPR/XMM o se eliminan/sintetizan mediante optimizaciones. Una etiqueta `.LC0` no representa una reserva dinámica.
9. **`movd` copia 32 bits; no convierte numéricamente.** `movd` mueve un patrón GPR/XMM. `cvtsi2ss`/`cvtsi2sd` realizan conversión numérica de entero a flotante.
10. **`long double` es una excepción.** En el entorno de referencia ocupa normalmente 16 bytes de almacenamiento, contiene un valor x87 extendido de 80 bits, sus argumentos se pasan en memoria y sus resultados se devuelven por `ST(0)` según la ABI System V AMD64.
11. **La alineación de una estructura no es múltiplo de su tamaño.** Para los tipos ordinarios del ejemplo, la alineación se deriva de los miembros (o de `alignas`); el tamaño se redondea a un múltiplo de esa alineación.
12. **No toda clase tiene un layout tan sencillo como una estructura plana.** Herencia, bases virtuales, miembros virtuales, bit-fields, `packed` y `alignas` pueden cambiar el layout. El `vptr` suele existir en implementaciones con virtuales, pero su posición no debe suponerse siempre offset cero.
13. **No todos los símbolos C++ se ven igual.** Los nombres de métodos y entidades en namespaces pueden estar codificados; los objetos de espacio de nombres global pueden conservar nombres simples según el ABI. Comprueba los símbolos con `nm -C`.
14. **Una llamada en un inicializador global no siempre implica inicialización dinámica.** Si el compilador puede realizar la inicialización como constante según las reglas de C++, puede emitir directamente los datos. Si hace falta ejecutar código, el runtime procesa las rutinas registradas antes de `main`.
15. **Los miembros de datos `static` no forman parte de cada objeto.** Tienen almacenamiento propio de duración estática y no contribuyen al `sizeof` de cada instancia.
16. **Las ubicaciones exactas de secciones y las instrucciones no son reglas de C++.** `.data`, `.bss`, `.rodata`, `.init_array`, los nombres `.LC0`, el uso de GOT/PLT y la secuencia de prólogo dependen de GCC, el enlazador, las opciones y el ABI.

Cuando una implementación propia tenga que interoperar con código GCC, la fuente de verdad para la llamada y el layout binario no es una salida de ejemplo: es la ABI System V AMD64 y el ABI C++ que adopte la plataforma.
