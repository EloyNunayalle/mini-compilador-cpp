# Memoria, funciones, stack y heap en x86-64 Linux

## Guía de estudio: desde las instrucciones del CPU hasta la memoria virtual y la asignación dinámica

Este documento conecta los temas que suelen estudiarse por separado al aprender ensamblador x86-64:

- Flujo de ejecución: etiquetas, saltos, `CALL` y `RET`.
- Stack, `%rsp`, `%rbp` y marcos de activación de funciones.
- Diferencia entre funciones y bloques `if` / `while`.
- Alcance léxico, tiempo de vida de variables y asignación de posiciones.
- Heap y memoria dinámica.
- `malloc`, `free`, `new` y `delete`.
- Proceso, kernel, memoria virtual, MMU, tablas de páginas, RAM y *page faults*.
- Cómo se conectan todas estas piezas en un programa Linux x86-64.

Los ejemplos de ensamblador utilizan sintaxis **GNU AT&T** y la convención de llamadas **System V AMD64**, habitual en Linux x86-64.

---

## Cómo está organizado este documento

El recorrido está pensado para leerse de principio a fin, sin necesitar conceptos de capítulos posteriores:

1. Primero se explica cómo el CPU ejecuta instrucciones y cómo funciona el stack (`%rsp`, `push` y `pop`).
2. Después se introduce la llamada a funciones (`CALL` y `RET`), que utiliza ese stack, y se construye el marco mediante `%rbp`.
3. Luego se explica por qué las funciones necesitan conservar su estado y por qué los bloques `if` y `while` normalmente reutilizan el marco de la función.
4. Antes de introducir el heap, se presentan el programa, el proceso, el kernel y la memoria virtual. Así, cuando aparezcan las regiones stack y heap, ya sabrás dónde existen y quién las administra.
5. Luego se explica el asignador, cómo obtiene memoria del kernel y cómo `malloc`, `free`, `new` y `delete` se conectan con el ensamblador.
6. Al final se integran todos los niveles y se incluyen comparaciones y preguntas de repaso.

Los ejemplos son esquemáticos y usan GNU AT&T en Linux x86-64. Las direcciones hexadecimales que aparezcan son ilustrativas, no direcciones fijas.

# 1. Dos problemas diferentes durante la ejecución

Antes de separar esos dos problemas, fijemos los términos básicos que aparecerán en todo el documento:

- **CPU o procesador:** hardware que obtiene, decodifica y ejecuta instrucciones máquina.
- **Instrucción:** operación codificada que el CPU ejecuta, por ejemplo `mov`, `add`, `cmp` o `jmp`.
- **Registro:** almacenamiento pequeño dentro del CPU. `RAX`, `RSP` y `RBP` son registros de propósito general con usos convencionales.
- **`RIP`:** registro que identifica la dirección de la siguiente instrucción que debe ejecutarse. Los saltos y llamadas cambian el flujo que sigue el CPU.
- **Memoria:** almacenamiento al que el CPU accede mediante direcciones. Un operando como `(%rax)` indica memoria en la dirección contenida en `RAX`; no es lo mismo que `%rax`, que indica el registro.

### Convenciones mínimas de GNU AT&T

Los ejemplos usan sintaxis AT&T. Antes de leerlos, identifica estas convenciones:

| Escritura | Significado |
|---|---|
| `%rax` | El registro RAX. |
| `$10` | El valor inmediato 10; no es una dirección. |
| `(%rax)` | La memoria cuya dirección está contenida en RAX. |
| `-8(%rbp)` | La memoria en la dirección `RBP - 8`. |
| `movl` | Copiar 32 bits (*long* en la nomenclatura de GAS). |
| `movq` | Copiar 64 bits (*quadword*). |
| `movw` / `movb` | Copiar 16 / 8 bits. |

En AT&T, el orden habitual de los operandos es **fuente, destino**. Por ejemplo, `movl $10, %eax` copia el valor 10 a EAX; `movl (%rax), %edx` lee 32 bits de la memoria apuntada por RAX y los copia a EDX. El sufijo indica el tamaño de la operación, no necesariamente el tamaño de todos los campos que se codifican en la instrucción.

Cuando un programa se ejecuta, hay dos problemas que debemos resolver:

1. **Control del flujo:** determinar qué instrucción se ejecuta después.
2. **Almacenamiento del estado:** determinar dónde se guardan los datos que necesita cada ejecución.

El primero se resuelve principalmente mediante instrucciones, etiquetas y saltos. El segundo se resuelve mediante registros y memoria.

Por ejemplo, en C++:

```cpp
int main() {
    int x = 10;

    if (x > 5) {
        x = 20;
    }

    return x;
}
```

En ensamblador no existe una instrucción especial que represente todo el `if`. El compilador lo transforma en instrucciones que comparan valores y cambian el flujo.

```asm
    movl $10, %eax
    cmpl $5, %eax
    jle .Lendif

    movl $20, %eax

.Lendif:
    # Continuación del programa
```

El procesador ejecuta las instrucciones en orden, salvo cuando una instrucción cambia el flujo. La etiqueta `.Lendif` identifica una dirección del código; no reserva memoria ni crea un objeto de ejecución.

## 1.1. Flags necesarios para entender las condiciones

Los **flags** son bits del registro `RFLAGS` que algunas instrucciones actualizan para describir propiedades del resultado. Para seguir los ejemplos de este documento, los cuatro más importantes son:

| Flag | Nombre | Qué indica |
|---|---|---|
| `ZF` | Zero Flag | Se activa cuando el resultado es cero. |
| `SF` | Sign Flag | Copia el bit más significativo del resultado; participa en condiciones con signo. |
| `OF` | Overflow Flag | Indica desbordamiento al interpretar el resultado con signo. |
| `CF` | Carry Flag | Indica acarreo en suma o préstamo en resta; participa en condiciones sin signo. |

No son variables independientes: son bits de `RFLAGS`. Las instrucciones determinan cuáles actualizan y cuáles conservan.

## 1.2. Qué hace `CMP` y qué hace un salto condicional

```asm
cmpl $5, %eax
jle .Lendif
```

`cmp` actualiza los flags como si calculara `EAX - 5`, pero no almacena el resultado. En AT&T, el orden es fuente y después destino: `cmpl $5, %eax` compara el destino EAX con la fuente 5. `jle` consulta los flags y salta si el resultado representa una relación de menor o igual **con signo**.

La misma comparación de bits puede interpretarse con signo o sin signo según el salto elegido. Por ejemplo:

- `jle`: menor o igual con signo (`ZF = 1` o `SF != OF`).
- `jge`: mayor o igual con signo (`SF = OF`).
- `jbe`: menor o igual sin signo (`CF = 1` o `ZF = 1`).
- `jae`: mayor o igual sin signo (`CF = 0`).

`CMP` no decide por sí solo si los valores son signed o unsigned; el salto condicional determina cómo se interpretan los flags.

También aparecerá `TEST`, que realiza un AND bit a bit entre sus operandos, actualiza flags y descarta el resultado. Es útil para comprobar si un valor es cero:

```asm
testq %rax, %rax
je .Lcero
```

Como `RAX AND RAX` solo es cero cuando RAX es cero, `ZF` se activa en ese caso y `je` salta. `TEST` no cambia RAX.

El patrón general para un `if` es:

1. Evaluar la condición.
2. Comparar los valores o probar el resultado.
3. Saltar a una etiqueta cuando corresponda.
4. Ejecutar el bloque que proceda.
5. Continuar en la etiqueta posterior.

## 1.3. Cómo se representa un `while`

Considera:

```cpp
int x = 0;
while (x < 3) {
    x++;
}
```

Una traducción ilustrativa es:

```asm
    movl $0, %eax

.Lwhile:
    cmpl $3, %eax
    jge .Lendwhile

    incl %eax
    jmp .Lwhile

.Lendwhile:
```

El bucle vuelve a la etiqueta `.Lwhile` para evaluar otra vez la condición. No se está llamando a una función: se cambia el valor del contador de programa para ejecutar instrucciones anteriores.

**Idea importante:** un salto (`jmp`, `je`, `jge`, etc.) cambia el flujo dentro del código. No guarda por sí mismo una dirección de retorno ni crea un nuevo marco de función.

---


# 2. El stack: qué es y cómo funciona

El **stack** o pila es una región de memoria utilizada para organizar información temporal de las llamadas a funciones. Entre otros datos, puede contener direcciones de retorno, valores de registros preservados, variables locales y temporales.

En x86-64, el stack convencional crece hacia direcciones de memoria más bajas. Su disciplina habitual es **LIFO** (*Last In, First Out*: lo último que entra es lo primero que sale). El registro `%rsp` indica la posición actual del tope de la pila.

En la sintaxis GNU AT&T, una referencia como `-8(%rbp)` significa la posición de memoria cuya dirección se calcula como `%rbp - 8`. En cambio, `%rbp` por sí solo nombra el registro. El signo `$` indica un valor inmediato: por ejemplo, `$32` es el número 32, no una dirección de memoria.

## 2.1. `RSP`: Stack Pointer

`%rsp` es el puntero de pila. No representa toda la pila: contiene una dirección que permite identificar su tope actual.

Supongamos que inicialmente:

```text
RSP = 0x1000
```

Al ejecutar:

```asm
pushq %rax
```

el procesador realiza conceptualmente:

```text
RSP = RSP - 8
Memoria[RSP] = RAX
```

El nuevo valor es `0x0ff8`. Se restan 8 bytes porque `pushq` introduce un operando de 64 bits.

Para recuperar el dato:

```asm
popq %rbx
```

se realiza conceptualmente:

```text
RBX = Memoria[RSP]
RSP = RSP + 8
```

`pop` no tiene que borrar físicamente los bytes antiguos. Simplemente mueve el tope de la pila y esos bytes dejan de formar parte del stack activo.

## 2.2. Reservar espacio con `SUB` y recuperarlo con `ADD`

Una función puede reservar espacio para sus variables locales mediante:

```asm
subq $32, %rsp
```

Si `%rsp` valía `0x1000`, ahora valdrá `0x0fe0`. El rango comprendido entre las nuevas posiciones del stack y la posición anterior queda disponible para el uso de la función.

Esto no inicializa los bytes. Si se necesita que una variable tenga un valor, hay que escribirlo:

```asm
movq $10, -8(%rbp)
```

Para recuperar los 32 bytes reservados, si no hay otros datos de pila que deban conservarse por encima de ese espacio:

```asm
addq $32, %rsp
```

**Diferencia:** `push` guarda un valor y actualiza `%rsp`; `subq $N, %rsp` desplaza el puntero para reservar espacio, pero no guarda automáticamente un dato.

---


# 3. Funciones: `CALL`, `RET` y dirección de retorno

Una función es código que puede invocarse desde distintos puntos. Cuando termina, debe continuar en la instrucción que seguía a la llamada.

```cpp
int suma(int a, int b) {
    return a + b;
}

int main() {
    int r = suma(10, 20);
    return r;
}
```

Para entender cómo se pasan esos datos, necesitamos la **ABI** (*Application Binary Interface*). Una ABI es el conjunto de reglas que permite que funciones compiladas por separado puedan llamarse correctamente: define cómo se pasan argumentos, dónde se devuelven resultados, qué registros deben preservarse y cómo se alinea la pila.

En Linux x86-64 se utiliza normalmente la ABI **System V AMD64**. Sus primeros argumentos enteros o punteros se pasan en:

| Argumento | Registro |
|---|---|
| 1 | `%rdi` |
| 2 | `%rsi` |
| 3 | `%rdx` |
| 4 | `%rcx` |
| 5 | `%r8` |
| 6 | `%r9` |

Los valores enteros de retorno se entregan normalmente en `%rax` (para un `int`, se utiliza la parte baja `%eax`). Los argumentos que no caben en los registros previstos se pasan por la pila siguiendo las reglas de la ABI.

Un ejemplo esquemático con dos funciones:

```asm
main:
    movl $10, %edi
    movl $20, %esi
    call suma

    # El resultado de suma está en EAX
    ret

suma:
    leal (%rdi,%rsi), %eax
    ret
```

El `ret` de `main` es importante en este fragmento: evita que la ejecución continúe accidentalmente en el código de `suma`. El ejemplo solo muestra el paso de argumentos y el retorno; un ejecutable completo también necesita sus directivas y el proceso normal de enlace.

## 3.1. Qué hace `CALL`

En modo de 64 bits, una llamada cercana ordinaria como `call suma` hace conceptualmente lo siguiente:

1. Calcula la dirección de la instrucción que sigue al `call`.
2. Decrementa `%rsp` en 8 bytes.
3. Escribe en la pila la dirección de retorno.
4. Cambia el flujo para ejecutar la función llamada.

Por tanto, `CALL` guarda automáticamente la dirección de retorno en el stack.

## 3.2. Qué hace `RET`

`ret` hace conceptualmente lo siguiente:

1. Lee de la dirección apuntada por `%rsp` la dirección de retorno.
2. La carga en `%rip`.
3. Incrementa `%rsp` en 8 bytes para retirar esa dirección de la pila.

Así, la ejecución continúa justo después del `call` que originó esa llamada.

## 3.3. Comparación con RISC-V

| Concepto | RISC-V | x86-64 |
|---|---|---|
| Llamada | `jal ra, funcion` | `call funcion` |
| Dirección de retorno | Se escribe en `ra` (`x1`) | `CALL` la guarda en el stack |
| Retorno | `jalr x0, 0(ra)`; normalmente se escribe `ret` como pseudoinstrucción | `ret` |

En RISC-V la dirección de retorno queda inicialmente en un registro. En x86-64, una llamada normal la coloca automáticamente en la pila. En ambos casos, el software debe preservar la dirección de retorno si una llamada posterior pudiera sobrescribir el lugar donde se conserva.

---


# 4. `RBP` y el marco de activación de una función

`%rbp` puede utilizarse como **frame pointer** (puntero de marco): una referencia estable desde la que se accede a las posiciones del marco de la función.

¿Por qué puede ser útil? Porque `%rsp` puede cambiar durante la ejecución por llamadas, `push`, `pop` o reservas temporales. Si las variables se referencian mediante desplazamientos desde `%rsp`, esos desplazamientos pueden necesitar actualizarse. Usar `%rbp` como ancla permite mantener offsets estables.

## 4.1. Prólogo clásico

```asm
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
```

Cada instrucción tiene una finalidad:

1. `pushq %rbp`: guarda el valor de `%rbp` que pertenecía al marco anterior.
2. `movq %rsp, %rbp`: establece la base del nuevo marco.
3. `subq $32, %rsp`: reserva 32 bytes para variables locales y/o temporales.

A partir de ese momento se pueden usar posiciones como:

```asm
movq $10, -8(%rbp)
movq $20, -16(%rbp)
movq $30, -24(%rbp)
```

Son desplazamientos respecto de la base del marco actual.

## 4.2. Organización conceptual del marco

Después del prólogo clásico, una disposición ilustrativa es:

```text
Direcciones más altas

RBP + 16    Argumentos que hayan sido pasados por stack (si existen)
RBP + 8     Dirección de retorno guardada por CALL
RBP         RBP anterior guardado por PUSH RBP
RBP - 8     Variable local / temporal
RBP - 16    Variable local / temporal
RBP - 24    Variable local / temporal
RBP - 32    Límite del espacio local reservado

Direcciones más bajas
```

No es una distribución universal de todas las funciones. Los argumentos pueden estar en registros, los valores pueden quedarse en registros en vez de ocupar memoria, y el compilador puede elegir otros offsets.

## 4.3. Epílogo: `LEAVE` y `RET`

El epílogo clásico es:

```asm
leave
ret
```

`leave` equivale conceptualmente a:

```asm
movq %rbp, %rsp
popq %rbp
```

Primero restaura `%rsp` al punto inmediatamente posterior al `pushq %rbp`, abandonando de una vez el espacio local. Luego recupera el `%rbp` anterior. Finalmente `ret` retira la dirección de retorno y vuelve al llamador.

## 4.4. ¿Todas las funciones necesitan este marco?

No necesariamente. Es el esquema clásico y didáctico, pero `%rbp` no está obligado a ser frame pointer. Un compilador puede omitirlo y utilizar `%rsp` u otros registros para localizar datos. También puede mantener variables en registros o eliminar almacenamiento que no sea necesario.

Lo esencial es que cada invocación debe conservar el estado que necesite para ejecutarse correctamente. No todas las invocaciones requieren un marco físico con el prólogo exacto mostrado arriba.

---


# 5. Por qué una función necesita contexto propio y un `if` o `while` normalmente no

La diferencia no es que una función pueda tener variables y un bloque no. Ambos pueden declarar variables. La diferencia es que una función crea una **nueva invocación de código**, mientras que un `if` o un `while` organizan el flujo dentro de una invocación que ya existe.

## 5.1. Las llamadas pueden estar activas simultáneamente

Considera la recursividad:

```cpp
int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}
```

Cuando se ejecuta `factorial(5)`, esta llamada invoca `factorial(4)` antes de terminar. La llamada de `factorial(5)` sigue activa y necesita conservar su propio `n` y el trabajo pendiente. Lo mismo ocurre con las siguientes llamadas.

```text
factorial(5)   n = 5, espera el resultado de factorial(4)
  factorial(4) n = 4, espera el resultado de factorial(3)
    factorial(3) n = 3, espera el resultado de factorial(2)
      factorial(2) n = 2, espera el resultado de factorial(1)
        factorial(1) n = 1, ejecución actual
```

Cada invocación tiene un estado distinto. Es necesario conservarlo hasta que esa llamada termine.

## 5.2. Un `while` repite instrucciones dentro de la misma invocación

```cpp
int main() {
    int x = 0;

    while (x < 3) {
        int y = x + 1;
        x++;
    }
}
```

El bucle no crea tres llamadas independientes a `main`. Es una sola invocación que vuelve a ejecutar determinadas instrucciones.

El compilador podría asignar:

```text
x -> -8(%rbp)
y -> -16(%rbp)
```

Y generar una estructura como:

```asm
    # Espacio local reservado una vez en el prólogo

.Lwhile:
    cmpl $3, -8(%rbp)
    jge .Lend

    movl -8(%rbp), %eax
    addl $1, %eax
    movl %eax, -16(%rbp)  # y = x + 1

    addl $1, -8(%rbp)     # x++
    jmp .Lwhile

.Lend:
```

La posición de `y` puede reutilizarse en cada vuelta. Se vuelve a escribir su valor cuando se ejecuta su inicialización, pero no hace falta reservar un marco nuevo en cada iteración.

## 5.3. Alcance léxico no equivale a marco de pila

El **alcance léxico** responde:

> ¿Desde qué partes del código se permite referenciar este identificador?

El **marco de activación** responde:

> ¿Qué estado de ejecución necesita conservar esta invocación de función?

Ejemplo:

```cpp
void funcion() {
    int x;

    if (x > 0) {
        int y;
        y = 5;
    }

    // y no es visible aquí
}
```

`y` solo es visible dentro del bloque. El compilador puede reservar su posición dentro del marco de `funcion` desde el prólogo y mantener ese espacio reservado hasta que la función termine. Al salir del bloque, `y` deja de ser visible y termina su tiempo de vida como objeto local; eso no obliga a modificar `%rsp`.

En C++, si la variable representa un objeto con destructor, salir del bloque también puede ejecutar ese destructor. Eso es una acción del lenguaje que el compilador debe generar; no implica que tenga que crear un marco de pila independiente ni que necesariamente deba mover `%rsp`.

Hay que separar tres conceptos:

- **Visibilidad:** dónde el código puede nombrar una variable.
- **Tiempo de vida:** durante qué intervalo existe el objeto según las reglas del lenguaje.
- **Almacenamiento físico:** qué registro o posición de memoria utiliza el compilador para representarlo.

No son lo mismo. Una variable puede dejar de estar en alcance y finalizar su tiempo de vida aunque los bytes que había ocupado permanezcan dentro del espacio reservado del marco.

## 5.4. ¿Un bloque puede cambiar `%rsp`?

Sí, en casos especiales. Por ejemplo, un arreglo de tamaño variable en C o una operación como `alloca` puede requerir reservar memoria en tiempo de ejecución y modificar `%rsp` dentro de un bloque.

Pero eso no convierte al bloque en una nueva invocación de función ni le da automáticamente un marco de activación independiente. El compilador debe restaurar correctamente el stack cuando corresponda.

## 5.5. `break`, `continue` y `return`

Estas instrucciones normalmente se traducen a saltos hacia etiquetas apropiadas:

- `break`: salta a la etiqueta de salida del bucle.
- `continue`: salta al punto donde se comprueba la condición o se ejecuta la siguiente iteración.
- `return`: transfiere el control al epílogo de la función, que restaura el estado necesario y retorna al llamador.

No necesitan un marco separado por ser bloques de control.

---


# 6. Variables locales, ámbitos y asignación de offsets en un compilador

Esta distinción es especialmente importante al implementar un compilador.

Supongamos:

```cpp
int main() {
    int x;

    if (x > 10) {
        int y;
        y = 5;
    }

    while (x < 20) {
        int z;
    }

    return 0;
}
```

Una asignación posible de posiciones es:

```text
x -> -8(%rbp)
y -> -16(%rbp)
z -> -24(%rbp)
```

El compilador podría reservar espacio suficiente para ellas en el prólogo de `main`, redondeando el tamaño según la alineación y otros requisitos.

## 6.1. Tabla de símbolos y niveles de alcance

Durante el análisis semántico, el compilador puede mantener niveles de alcance:

```text
Nivel de main:
    x

    Nivel de if:
        y

    Nivel de while:
        z
```

En un compilador, estas estructuras existen durante el análisis y la generación de código. No son regiones especiales de memoria que el CPU vaya a reconocer durante la ejecución.

Operaciones como:

```text
position.add_level()
types.add_level()
```

pueden representar la apertura de un ámbito. Y operaciones como:

```text
position.remove_level()
types.remove_level()
```

pueden representar el cierre de ese ámbito.

Eso controla qué nombres están visibles y qué tipos se asocian a ellos. No significa, por sí solo, ejecutar `push`, `pop` o ajustar `%rsp`.

## 6.2. Asignación de posiciones y espacio de la función

El compilador también debe decidir dónde se almacena cada variable. Una estructura de datos de posiciones puede asignar offsets como los del ejemplo anterior.

Si el generador utiliza algo como:

```asm
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
```

el último paso reserva un área fija para las variables locales y otros datos que el generador haya decidido ubicar allí.

El tamaño final puede incluir redondeo por alineación, almacenamiento temporal y requisitos de llamadas. Por eso no siempre coincide exactamente con la suma aritmética de los tamaños de las variables.

## 6.3. Espacio fijo del marco y temporales de evaluación

No confundas:

- **Espacio fijo del marco:** se reserva normalmente al entrar a la función mediante `subq $N, %rsp`.
- **Temporales apilados:** valores intermedios que el generador guarda con `push` y recupera con `pop` durante el cálculo de expresiones.

Por ejemplo, para calcular `a + b`, un generador sencillo puede hacer:

```asm
    movl -8(%rbp), %eax
    pushq %rax              # Guardar temporalmente a

    movl -16(%rbp), %eax    # Calcular/obtener b
    movl %eax, %edx

    popq %rax               # Recuperar a
    addl %edx, %eax         # a + b
```

Un contador interno llamado `pila` podría ayudar al generador a registrar cuántos valores temporales están apilados y comprobar que los `push` y `pop` estén equilibrados.

Ese contador no es lo mismo que el cálculo del tamaño fijo del marco.

**Importante:** si el generador conserva temporales en la pila y ejecuta un `call`, debe garantizar la alineación exigida por la ABI en el punto de llamada.

---


# 7. Conceptos necesarios: programa, proceso, kernel y espacio de direcciones

Para entender de dónde aparece la memoria del heap debemos conocer varios conceptos del sistema operativo.

## 7.1. Programa y proceso

Un **programa** es un conjunto de instrucciones y datos almacenados, por ejemplo, en un archivo ejecutable.

Un **proceso** es una instancia de un programa en ejecución. Tiene un **espacio de direcciones virtuales**, es decir, el conjunto de direcciones virtuales que puede utilizar. También tiene estado propio, como:

- Un contador de programa que indica qué instrucción se ejecuta.
- Registros de CPU guardados o activos.
- Recursos asociados, como archivos abiertos.
- Uno o más **hilos** (*threads*) de ejecución. Un hilo es una secuencia de instrucciones que se ejecuta dentro del proceso. Los hilos de un mismo proceso comparten normalmente su espacio de direcciones, pero cada hilo tiene su propio estado de CPU y su propia pila.

Dos ejecuciones del mismo programa son procesos distintos y, normalmente, tienen espacios de direcciones virtuales separados. Por ello, una dirección virtual del proceso A no tiene por qué referirse a la misma memoria que esa misma dirección virtual en el proceso B.

## 7.2. Kernel

El **kernel** o núcleo es la parte central del sistema operativo que administra recursos de la máquina y ofrece servicios controlados a los programas.

Los programas ordinarios se ejecutan normalmente en **modo usuario**, con permisos restringidos. El kernel se ejecuta en **modo kernel** o privilegiado, que le permite realizar operaciones protegidas, por ejemplo configurar memoria virtual y controlar dispositivos. Esta separación ayuda a aislar procesos y proteger el sistema.

Entre sus responsabilidades están:

- Planificar qué hilo puede utilizar el CPU.
- Administrar procesos y memoria virtual.
- Gestionar dispositivos y operaciones de entrada/salida.
- Controlar permisos y aislamiento entre procesos.
- Atender llamadas al sistema y excepciones del procesador.

## 7.3. Llamada a función frente a llamada al sistema

No son lo mismo:

- `call malloc`: es una llamada a una función de biblioteca dentro del proceso.
- Una llamada al sistema (*system call*): es una transición controlada desde modo usuario al kernel para pedir un servicio del sistema operativo.

`malloc` no es una instrucción del CPU ni necesariamente una llamada al sistema por cada reserva. Puede resolver una petición usando bloques que ya administra. Cuando necesita más memoria virtual, la biblioteca puede solicitarla al kernel mediante mecanismos como `brk` o `mmap`, que se explican más adelante.

---


# 8. Memoria virtual, direcciones virtuales y direcciones físicas

Esta es una de las piezas más importantes para comprender por qué stack y heap son parte de la misma memoria del proceso.

## 8.1. Dirección virtual

Cuando el programa ejecuta:

```asm
movl $25, (%rax)
```

el valor de `%rax` se interpreta como una dirección de memoria que utiliza el proceso. En un sistema moderno con memoria virtual, normalmente es una **dirección virtual** (más precisamente, una dirección lineal en el contexto x86-64).

El programa trabaja con su espacio de direcciones virtuales. No necesita conocer directamente en qué posición de la RAM física se encuentran los datos.

## 8.2. Dirección física

La **dirección física** identifica una ubicación en la memoria física que el hardware puede direccionar. El proceso no suele manejarla directamente.

El sistema operativo establece correspondencias entre páginas virtuales y páginas físicas. En Linux, las regiones virtuales con atributos relacionados se describen mediante estructuras de gestión conocidas como **VMA** (*Virtual Memory Areas*). Una VMA representa un intervalo de direcciones virtuales y sus propiedades, por ejemplo sus permisos o el tipo de mapeo. Gracias a la memoria virtual, el sistema puede:

- Aislar los espacios de memoria de diferentes procesos.
- Dar a cada proceso un espacio de direcciones propio.
- Aplicar permisos de lectura, escritura y ejecución.
- Mover páginas físicas sin que el programa tenga que cambiar sus punteros virtuales.
- Compartir ciertas páginas entre procesos cuando corresponde.

## 8.3. Páginas y tablas de páginas

La memoria virtual se administra en unidades llamadas **páginas**. La memoria física se administra en unidades correspondientes llamadas **marcos de página** (*page frames*).

El sistema operativo mantiene estructuras llamadas **tablas de páginas** (*page tables*) que permiten traducir páginas virtuales a marcos físicos y registrar permisos. La traducción se organiza por páginas, no con una entrada individual para cada byte. En x86-64, las tablas pueden tener varios niveles.

## 8.4. MMU y TLB

La **MMU** (*Memory Management Unit*) es el componente de hardware que participa en la traducción de direcciones virtuales a físicas y en la comprobación de permisos.

Para no recorrer las tablas de páginas en cada acceso, el procesador dispone de una caché de traducciones llamada **TLB** (*Translation Lookaside Buffer*). Si la traducción está en la TLB, puede reutilizarse rápidamente. Si no está, el hardware debe obtener la traducción siguiendo las estructuras de páginas, según la arquitectura.

## 8.5. Cachés del procesador: L1, L2 y L3

La **RAM** es mucho más lenta que los registros del CPU. Para reducir el tiempo de acceso, los procesadores incorporan memorias pequeñas y rápidas llamadas **cachés**. Normalmente se organizan en varios niveles:

- **L1:** muy pequeña y muy rápida; suele estar cerca de cada núcleo y puede estar separada para instrucciones y datos.
- **L2:** generalmente mayor que L1 y algo más lenta.
- **L3:** normalmente más grande y compartida entre varios núcleos del procesador.

Las cachés guardan copias de partes de la memoria, organizadas en bloques llamados **líneas de caché**. Si el dato solicitado ya está en una caché, el CPU puede obtenerlo sin tener que traerlo desde la RAM. Si no está, el hardware busca en niveles inferiores y, si es necesario, en la memoria principal.

Esto no crea una región nueva del proceso ni cambia la diferencia entre stack y heap. Las cachés no son «la memoria del stack» ni «la memoria del heap»: almacenan temporalmente copias de datos e instrucciones de distintas regiones. El acceso a stack y heap pasa por la jerarquía de memoria del mismo procesador. La traducción mediante TLB/MMU y el acceso a caché están coordinados por el hardware; el dibujo siguiente es una explicación conceptual, no una secuencia rígida de etapas físicas.

## 8.6. Qué ocurre al acceder a distintas regiones de memoria

El CPU no necesita una instrucción `LOAD_STACK`, `LOAD_DATA` ni una instrucción especial para cada región.

Para el procesador, todos son accesos a direcciones virtuales. Por ejemplo:

```asm
movl $25, -4(%rbp)   # Acceso a una dirección calculada desde RBP
movl $25, (%rax)     # Acceso a la dirección guardada en RAX
```

El mecanismo de traducción de direcciones es el mismo. El uso que el programa da a cada rango y los permisos asociados son los que cambian.

---


# 9. Cómo se organizan las regiones de memoria de un proceso

El stack y la región tradicional del heap no son dos tipos de RAM ni dos módulos físicos diferentes. Ahora que ya definimos proceso, espacio de direcciones virtuales, páginas y kernel, podemos ubicarlos: ambos son rangos de direcciones virtuales del mismo proceso, con propósitos y reglas de administración diferentes. El **heap** es el nombre habitual de la memoria usada para reservas dinámicas; su gestión detallada se explica en el capítulo siguiente.

Un dibujo conceptual típico puede mostrar:

```text
Direcciones virtuales altas
+----------------------------------+
| Stack del hilo                   |  Crece normalmente hacia abajo
+----------------------------------+
|                                  |
| Regiones mapeadas / espacio libre |
|                                  |
+----------------------------------+
| Heap tradicional                 |  Puede crecer hacia arriba
+----------------------------------+
| Datos globales / BSS              |
+----------------------------------+
| Código del programa               |
+----------------------------------+
Direcciones virtuales bajas
```

Este dibujo es una simplificación. La disposición exacta depende del sistema, del ejecutable, de las bibliotecas, de ASLR (*Address Space Layout Randomization*, una técnica que varía algunas ubicaciones virtuales para dificultar ataques) y de las regiones mapeadas. Además, no todas las reservas de `malloc` pertenecen necesariamente a la región tradicional denominada `[heap]`: algunas pueden proceder de mapeos independientes. Cada hilo también suele tener su propia pila dentro del espacio del proceso. Las páginas virtuales de estas regiones no tienen por qué corresponder a posiciones contiguas de la RAM física.

---


# 10. El heap: qué es la memoria dinámica

El **heap de memoria** es el término habitual para referirse al conjunto de bloques que un asignador administra para reservas dinámicas. Permite reservar bloques durante la ejecución y liberarlos cuando ya no se necesitan, por lo que la duración de esos objetos no tiene que coincidir con la entrada y salida de una función.

Conviene distinguir dos usos de la palabra: el **heap tradicional de Linux**, que es una región virtual asociada al segmento de datos y delimitada por el *program break* (el límite de esa región que puede modificarse mediante la interfaz `brk`), y el **heap en sentido de asignación dinámica**, que es el conjunto de bloques gestionados por un asignador. Este segundo sentido es más amplio: parte de las reservas puede provenir de mapeos independientes creados mediante `mmap` (una llamada al sistema que crea una región dentro del espacio de direcciones virtuales), no necesariamente de la región `[heap]`.

No debe confundirse con el **Binary Heap** de estructuras de datos. El Binary Heap es una estructura de datos usada, por ejemplo, para implementar una cola de prioridad. El heap de memoria es un mecanismo de asignación dinámica.

## 10.1. Antes: qué es un puntero

Un **puntero** es una variable cuyo valor representa una dirección de memoria. Por ejemplo:

```cpp
int x = 20;
int* p = &x;
```

- `x` contiene el entero `20`.
- `&x` obtiene la dirección donde está almacenado `x`.
- `p` almacena esa dirección.
- `*p` (desreferenciar el puntero) accede al objeto que está en esa dirección.

En x86-64, si `%rax` contiene la dirección de `x`, `(%rax)` se refiere a la memoria ubicada allí. Un puntero no es el objeto apuntado: solo guarda la dirección que permite localizarlo.

En un proceso de 64 bits, un puntero ordinario ocupa normalmente 8 bytes. El tamaño de los datos apuntados es independiente: un `int` ocupa normalmente 4 bytes en Linux x86-64.

## 10.2. ¿Por qué necesitamos memoria dinámica?

Considera:

```cpp
void procesar() {
    int notas[5];
}
```

El tamaño del arreglo está fijado en el código. ¿Qué pasa si la cantidad de estudiantes se conoce solo durante la ejecución?

```cpp
int n;
std::cin >> n;

int* notas = new int[n];
```

`new int[n]` solicita espacio para `n` enteros y devuelve la dirección del primer elemento.

Si `n = 4`, una representación ilustrativa sería:

```text
STACK                                  HEAP
+----------------------+               +---------+---------+---------+---------+
| notas = 0x5000       | ------------> | int[0]  | int[1]  | int[2]  | int[3]  |
+----------------------+               +---------+---------+---------+---------+
                                               región dinámica
```

La variable `notas` es un puntero. El arreglo de cuatro enteros es el bloque dinámico al que apunta.

En una implementación típica, la variable local puntero puede almacenarse en el stack, mientras que el bloque solicitado se obtiene del asignador de memoria dinámica.

## 10.3. Tiempo de vida: stack frente a heap

Una variable local automática tiene un tiempo de vida asociado al bloque o función donde se declara. Un objeto creado dinámicamente tiene un tiempo de vida que se administra por separado.

Ejemplo incorrecto:

```cpp
int* crearNumero() {
    int x = 20;
    return &x; // Incorrecto: x deja de existir al salir de la función
}
```

La dirección devuelta apunta a un objeto cuyo tiempo de vida terminó. Utilizarla produce comportamiento indefinido.

Ejemplo con asignación dinámica:

```cpp
int* crearNumero() {
    int* p = new int(20);
    return p;
}
```

Al terminar la función, el puntero local `p` deja de existir, pero el entero reservado dinámicamente continúa existiendo. Quien recibe el puntero debe hacerse responsable de su tiempo de vida y liberarlo correctamente.

La idea central es:

- La posición local usada para guardar `p` pertenece al contexto de la función.
- El objeto apuntado por `p` tiene una reserva dinámica independiente.

---


# 11. Quién administra el heap: el asignador

Cuando se escribe:

```cpp
int* p = new int(20);
```

el programa no escoge manualmente una dirección física de RAM. Solicita una asignación al mecanismo de memoria dinámica del entorno de ejecución.

El **asignador** (*allocator*) es el software que administra bloques de memoria disponibles. Mantiene **metadatos**: información que describe los bloques, por ejemplo sus tamaños, alineación y disponibilidad. Esos metadatos también se almacenan en memoria. De forma simplificada:

1. Recibe una solicitud de cierto tamaño y alineación.
2. Busca un bloque libre que pueda satisfacerla.
3. Si encuentra uno, lo asigna al solicitante.
4. Si no tiene espacio adecuado, puede solicitar más memoria al sistema operativo.
5. Devuelve una dirección que el programa puede almacenar en un puntero.

Cuando se libera un bloque, el asignador puede volver a utilizarlo en futuras solicitudes.

## 11.1. Por qué `%rsp` no administra el heap

El stack convencional sigue una disciplina LIFO: *Last In, First Out*. Lo último que se introduce es lo primero que se retira.

Ejemplo:

```text
Reservar A -> reservar B -> reservar C
```

Si se quiere recuperar espacio simplemente moviendo `%rsp`, lo natural es retirar primero C, luego B y finalmente A.

Pero las reservas dinámicas pueden liberarse en otro orden:

```text
Reservar A -> reservar B -> reservar C
Liberar B
Mantener A y C
```

El asignador necesita recordar que B está libre aunque A y C sigan ocupados. `%rsp` por sí solo no puede representar ese estado: solo indica el tope actual de la pila.

Un asignador puede usar estructuras internas como:

- Metadatos que registran el tamaño y estado de los bloques.
- Listas de bloques libres.
- Agrupaciones de tamaños (*bins*): conjuntos o listas que agrupan bloques libres según su tamaño.
- Estrategias de búsqueda y selección de bloques.
- Unión de bloques libres contiguos (*coalescing*): combinar huecos adyacentes para formar un bloque libre mayor.
- Arenas: áreas o estructuras de administración separadas que algunos asignadores usan para permitir asignaciones concurrentes; pueden reducir la contención, es decir, la espera de varios hilos por acceder al mismo recurso compartido.

No existe un único algoritmo obligatorio para todos los heaps. Es una decisión de implementación del asignador.

## 11.2. Fragmentación

La **fragmentación externa** aparece cuando hay espacio libre total suficiente, pero está repartido en huecos separados que no satisfacen una solicitud grande contigua.

La **fragmentación interna** aparece cuando el bloque entregado es algo mayor que la cantidad útil solicitada, por ejemplo por redondeo de tamaños, alineación o clases de tamaño.

Los asignadores utilizan diferentes estrategias para reducir estos problemas.

---


# 12. Cómo el asignador obtiene espacio del kernel: `brk` y `mmap`

El asignador necesita disponer de regiones de direcciones virtuales para dividirlas en bloques. En Linux, dos mecanismos importantes son `brk` y `mmap`.

## 12.1. `brk` y el heap tradicional

El **program break** es un límite asociado al final del segmento de datos/heap tradicional del proceso. Modificarlo puede ampliar o reducir el rango virtual que queda disponible para esa región.

De forma esquemática:

```text
Antes:
Inicio del heap                         program break
     |-----------------------------------|

Después de ampliarlo:
Inicio del heap                                             program break
     |-------------------------------------------------------|
```

`brk` es una llamada al sistema Linux para cambiar ese límite. `sbrk` es una interfaz histórica de biblioteca relacionada con ese mecanismo.

No significa que `malloc` tenga que invocar `brk` en cada llamada. Muchas solicitudes pueden satisfacerse con bloques que el asignador ya tiene disponibles.

## 12.2. `mmap`

`mmap` permite crear un mapeo en el espacio virtual del proceso. Puede mapear un archivo o crear una región anónima, que no está respaldada por un archivo ordinario.

Ejemplo conceptual en C:

```c
void* p = mmap(
    NULL,
    4096,
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS,
    -1,
    0
);
```

Los argumentos son: dirección sugerida (`NULL` deja que el kernel elija), longitud, permisos (`PROT_READ | PROT_WRITE`), opciones del mapeo (`MAP_PRIVATE | MAP_ANONYMOUS`), descriptor de archivo y desplazamiento dentro del archivo. `MAP_ANONYMOUS` indica que no se está mapeando un archivo ordinario; por eso se usa `-1` como descriptor. `MAP_PRIVATE` solicita un mapeo privado: las modificaciones no se escriben como cambios compartidos en un archivo y se gestionan con semántica de copia bajo escritura cuando corresponde.

Esto solicita una región virtual de 4096 bytes con permisos de lectura y escritura. La dirección concreta la elige el kernel cuando se usa `NULL` como dirección sugerida. Si falla, `mmap` devuelve `MAP_FAILED`, no `NULL`; el programa debe comprobar ese valor. Un descriptor de archivo es un identificador entero que el proceso utiliza para referirse a un archivo abierto.

La región no tiene que estar pegada al heap tradicional. Puede ser otro mapeo dentro del espacio virtual del proceso.

## 12.3. Cómo se relacionan con `malloc`

En muchos sistemas Linux se utiliza `glibc` (la biblioteca C del proyecto GNU). Su asignador puede utilizar bloques que ya tiene administrados, pedir más espacio mediante mecanismos como `brk`, o crear mapeos independientes con `mmap` para determinadas solicitudes.

Los umbrales y las estrategias dependen de la implementación y su configuración. No se debe asumir que cada `malloc` genera un `mmap` ni que todo bloque de `malloc` aparece dentro de la región `[heap]`.

---


# 13. Paginación bajo demanda: cuándo llega a intervenir la RAM

Solicitar una región virtual no implica necesariamente que toda su extensión ya tenga una página física de RAM asignada de forma inmediata.

En sistemas con **demand paging** (paginación bajo demanda), algunas páginas físicas se preparan cuando el programa accede por primera vez a una página virtual que todavía no está materializada de la forma requerida.

Ejemplo:

```c
char* p = malloc(4096);
p[0] = 'A';
```

Una secuencia simplificada puede ser:

1. `malloc` consigue una dirección utilizable para el programa.
2. El programa escribe en `p[0]`.
3. El CPU intenta traducir y realizar el acceso.
4. Si la página no tiene todavía una traducción física válida para ese acceso, se produce una excepción de página (*page fault*).
5. El kernel analiza la causa. Si el acceso es válido y la página puede prepararse, establece la correspondencia necesaria y la ejecución puede continuar.

Un **page fault** (*fallo de página*) es una excepción que se produce cuando el acceso no puede completarse con la traducción y los permisos disponibles en ese momento. No significa automáticamente que el programa haya cometido un error: puede ser parte normal de la asignación bajo demanda. El kernel analiza la causa. Si el acceso es válido, puede preparar la página y permitir que la instrucción se reintente; si el acceso viola los permisos o apunta a una región inválida, puede enviar `SIGSEGV` al proceso. `SIGSEGV` es una señal que normalmente termina el proceso por un acceso de memoria inválido.

La memoria física puede incluir RAM que contiene páginas de muchos procesos, cachés de archivos y otras estructuras. La memoria virtual es la abstracción de direcciones que utiliza cada proceso; no es sinónimo de RAM.

---


# 14. Reservar memoria dinámica desde ensamblador x86-64

Ahora conectemos el mecanismo con las instrucciones que escribe el programador.

## 14.1. `malloc`: solicitar bytes

En C:

```c
int* p = malloc(sizeof(int));
```

En Linux x86-64, si `int` ocupa 4 bytes, se puede hacer una llamada equivalente de esta forma:

En `malloc@PLT`, el sufijo `@PLT` indica que la llamada utiliza una entrada de la **Procedure Linkage Table (PLT)**. La PLT participa en el enlace dinámico para transferir el control a funciones que pueden residir en bibliotecas compartidas, como `malloc` en la biblioteca de C. No es una instrucción especial de reserva de memoria: sigue siendo una llamada a una función.

```asm
movl $4, %edi
call malloc@PLT
```

Según System V AMD64:

- `%rdi` contiene el primer argumento, la cantidad de bytes solicitados.
- `%rax` contiene el puntero devuelto.

`movl $4, %edi` escribe 4 en los 32 bits bajos de RDI y, en modo de 64 bits, la escritura en un registro de 32 bits pone a cero los 32 bits altos. Por tanto, el argumento completo en RDI vale 4. El argumento de tamaño es de tipo `size_t` (el tipo entero sin signo que C utiliza para representar tamaños), que en Linux x86-64 tiene 64 bits; esta instrucción es válida precisamente por el efecto de puesta a cero de los bits altos.

`malloc` devuelve un puntero nulo (`NULL`) cuando no puede satisfacer la solicitud. Por eso se debe comprobar el resultado.

```asm
movl $4, %edi
call malloc@PLT

testq %rax, %rax
je .Lsin_memoria
```

`testq %rax, %rax` no modifica RAX: actualiza los flags según el AND del registro consigo mismo. Si `malloc` devolvió `NULL` (un puntero nulo, representado por cero), `ZF` queda activo y `je` salta a la ruta de error. Esto utiliza las reglas de `TEST` y los flags explicados en el capítulo 1.

La dirección concreta que devuelve `malloc` es variable. Si para explicar imaginamos `RAX = 0x5000`, significa que el bloque empieza en esa dirección virtual ilustrativa.

`malloc` devuelve una dirección, no un entero inicializado. El contenido de la memoria reservada no está inicializado.

## 14.2. Guardar el puntero en una variable local

```asm
movq %rax, -8(%rbp)
```

Se copia el puntero de RAX a la posición local `-8(%rbp)`.

```text
STACK                                      HEAP
+-------------------------+                +----------------------+
| -8(%rbp): 0x5000        | --------------> | bloque desde 0x5000  |
| variable local p        |                | contenido sin inicializar
+-------------------------+                +----------------------+
```

La posición `-8(%rbp)` almacena la dirección. No es el propio objeto reservado.

## 14.3. Escribir en el bloque

En C:

```c
*p = 25;
```

En ensamblador:

```asm
movq -8(%rbp), %rax
movl $25, (%rax)
```

Primero se recupera el puntero desde el stack. Después `(%rax)` significa la memoria cuya dirección es el contenido de RAX.

La diferencia de notación es fundamental:

- `$25`: valor inmediato 25.
- `%rax`: contenido del registro RAX.
- `(%rax)`: memoria ubicada en la dirección contenida en RAX.

La escritura es de 32 bits (`movl`) porque el entero C `int` normalmente ocupa 4 bytes en Linux x86-64.

## 14.4. Leer del bloque

En C:

```c
int x = *p;
```

En ensamblador:

```asm
movq -8(%rbp), %rax
movl (%rax), %edx
movl %edx, -12(%rbp)
```

Se recupera el puntero, se leen los cuatro bytes de la dirección apuntada y se almacena el entero leído en una variable local ilustrativa.

## 14.5. Liberar el bloque con `free`

En C:

```c
free(p);
```

En ensamblador:

```asm
movq -8(%rbp), %rdi
call free@PLT
```

`free` recibe el puntero en RDI. El bloque vuelve a estar disponible para el asignador. No se debe acceder al objeto después de liberarlo.

`free` no garantiza que los bytes se pongan a cero ni que el asignador devuelva inmediatamente la región al kernel. Puede conservar el bloque para reutilizarlo en futuras reservas.

---


# 15. Programa completo de ejemplo con `malloc` y `free`

Este ejemplo reserva un entero, comprueba si la reserva tuvo éxito, escribe el valor 25, lo lee, lo libera y retorna.

Código C equivalente:

```c
#include <stdlib.h>

int main(void) {
    int* p = malloc(sizeof(int));

    if (p == NULL)
        return 1;

    *p = 25;
    int x = *p;

    free(p);
    return 0;
}
```

Ensamblador GNU AT&T para Linux x86-64:

```asm
.section .text
.globl main

main:
    # Prólogo clásico
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp

    # int* p = malloc(4);
    movl $4, %edi
    call malloc@PLT

    # Comprobar si malloc devolvió NULL
    testq %rax, %rax
    je .Lerror

    # Guardar p en una variable local del stack
    movq %rax, -8(%rbp)

    # *p = 25;
    movq -8(%rbp), %rax
    movl $25, (%rax)

    # int x = *p;
    movl (%rax), %edx
    movl %edx, -12(%rbp)

    # free(p);
    movq -8(%rbp), %rdi
    call free@PLT

    # return 0;
    movl $0, %eax
    leave
    ret

.Lerror:
    movl $1, %eax
    leave
    ret

.section .note.GNU-stack,"",@progbits
```

## 15.1. Por qué se reserva 16 bytes

Al entrar a `main`, bajo la convención System V AMD64, el stack tiene una alineación determinada por la llamada. El prólogo guarda `%rbp` y deja la pila alineada; reservar 16 bytes mantiene esa alineación antes de llamar a `malloc` y `free`.

En el marco se usan:

- `-8(%rbp)`: 8 bytes para el puntero `p`.
- `-12(%rbp)`: 4 bytes para el entero local `x`.

Hay espacio adicional disponible dentro de los 16 bytes reservados. El ejemplo es didáctico; un compilador real podría optimizar `x` y no almacenarlo en memoria.

El **ensamblador** traduce el archivo `.s` a código objeto. El **enlazador** combina los objetos y resuelve símbolos entre ellos. El **cargador dinámico** prepara el proceso y carga las bibliotecas compartidas necesarias; el mecanismo de enlace dinámico permite resolver referencias externas como `malloc`.

Se puede ensamblar y enlazar, por ejemplo, con:

```bash
gcc programa.s -o programa
./programa
```

El ensamblador no implementa `malloc`: genera una llamada a una función de biblioteca. El enlazador y el cargador ayudan a resolver cómo se localiza esa función durante el enlace y la ejecución.

---


# 16. `new` / `delete` frente a `malloc` / `free`

En C++:

```cpp
int* p = new int(25);
delete p;
```

En C, en cambio, `malloc` devuelve `void*`, que puede convertirse implícitamente a un puntero a objeto. En C++ no existe esa conversión implícita de `void*` a `int*`, por lo que el ejemplo de `malloc` usado anteriormente está escrito como C o requeriría una conversión explícita en C++.

De forma conceptual:

1. `new` obtiene almacenamiento mediante el mecanismo de asignación de C++, normalmente `operator new`.
2. Construye o inicializa el objeto en ese almacenamiento.
3. `delete` ejecuta la destrucción correspondiente y libera el almacenamiento mediante `operator delete`.

En cambio, `malloc` solicita bytes y `free` los devuelve al asignador de C. `malloc` no ejecuta constructores de C++ y `free` no ejecuta destructores de C++.

| Reserva | Liberación correspondiente |
|---|---|
| `malloc` | `free` |
| `calloc` | `free` |
| `realloc` | `free` cuando corresponda |
| `new T` | `delete` |
| `new T[n]` | `delete[]` |

No mezcles las parejas. Para un arreglo creado con `new[]` se utiliza `delete[]`.

---


# 17. Errores comunes al utilizar memoria dinámica

## 17.1. Memory leak (fuga de memoria)

```cpp
void funcion() {
    int* p = new int(100);
} // p deja de existir, pero el entero dinámico sigue reservado
```

El puntero local desaparece al salir de la función, pero no se libera el objeto dinámico. Si se pierde la única dirección que permitía alcanzarlo, el programa ya no puede liberarlo normalmente: es una fuga de memoria.

## 17.2. Dangling pointer (puntero colgante)

```cpp
int* p = new int(100);
delete p;

// p todavía contiene una dirección, pero el objeto ya no existe
```

La dirección permanece en el valor del puntero, pero ya no se puede acceder al objeto liberado. Hacerlo produce comportamiento indefinido.

Una práctica defensiva es poner el puntero a `nullptr` después de liberarlo cuando siga siendo necesario conservar la variable:

```cpp
delete p;
p = nullptr;
```

Esto no corrige copias antiguas del mismo puntero, pero evita algunos usos accidentales de esa variable.

## 17.3. Liberación doble

```cpp
int* p = new int(100);
delete p;
delete p; // Incorrecto
```

Liberar dos veces el mismo objeto mediante el mismo bloque de almacenamiento produce comportamiento indefinido.

## 17.4. Acceder fuera del bloque

```cpp
int* a = new int[4];
a[4] = 10; // Fuera de los índices válidos: 0, 1, 2 y 3
```

La dirección calculada queda fuera de la región del arreglo. El acceso es inválido y puede corromper otros datos o provocar un fallo.

---


# 18. ¿Se puede implementar un heap propio?

Sí. No hace falta un registro especial del CPU llamado `HEAP`.

Un programa o una biblioteca puede obtener una región de memoria virtual —por ejemplo, usando `mmap`— y construir su propio asignador.

Una versión muy sencilla podría:

1. Conservar la dirección inicial de la región y su tamaño.
2. Dividir la región en bloques.
3. Guardar metadatos sobre el tamaño y el estado de cada bloque.
4. Al solicitar memoria, encontrar un bloque libre suficientemente grande.
5. Marcarlo como ocupado y devolver la dirección de la zona de datos.
6. Al liberar, marcarlo como libre.
7. Unir bloques contiguos libres cuando sea posible.

Las estructuras del asignador también se almacenan en memoria. Pueden estar dentro de la propia región administrada o en otros bloques que controle el asignador.

Por tanto, un heap de memoria es una política y un conjunto de estructuras de software para administrar bloques. El CPU solo ejecuta las instrucciones que implementan esa política y accede a las direcciones indicadas.

---


# 19. Comparación global: stack y heap

| Característica | Stack | Heap de memoria |
|---|---|---|
| Finalidad típica | Estado de llamadas y almacenamiento automático | Reservas dinámicas |
| Administración habitual | Prólogo/epílogo, `%rsp`, convenciones de llamada | Asignador de memoria (`malloc`, `new`, etc.) |
| Organización | Disciplina LIFO para el stack convencional | Bloques con tamaños y tiempos de liberación variados |
| Reserva | Cambios de `%rsp` y/o gestión del marco | Solicitudes al asignador |
| Liberación | Retirar el marco o restaurar `%rsp` | Devolver el bloque al asignador |
| Tiempo de vida | Normalmente ligado a la ejecución o al ámbito del objeto automático | Independiente de la función que realizó la reserva, hasta su liberación |
| Acceso | Direcciones referenciadas desde `%rsp`, `%rbp` u otros registros | Direcciones obtenidas y transportadas mediante punteros |
| Riesgo característico | Desbordar el stack | Fugas, fragmentación, uso después de liberar, liberación doble |

**Precisión:** stack y heap no son tipos de memoria física. Ambos utilizan memoria virtual del proceso y, cuando sus páginas están respaldadas por memoria física residente, utilizan la misma RAM del sistema.

---


# 20. Mapa completo: desde una variable local hasta un objeto dinámico

Cuando el programa ejecuta:

```cpp
int* p = new int(25);
```

o su equivalente conceptual con `malloc`, intervienen varios niveles:

```text
CÓDIGO DEL PROGRAMA
    |
    | llama a malloc / operator new
    v
BIBLIOTECA DE EJECUCIÓN Y ASIGNADOR
    |
    | busca un bloque disponible
    | reutiliza uno o pide más espacio si lo necesita
    v
KERNEL DEL SISTEMA OPERATIVO (si se requiere más espacio virtual)
    |
    | administra regiones del espacio virtual mediante mecanismos como brk/mmap
    v
ESPACIO DE DIRECCIONES VIRTUALES DEL PROCESO
    |
    | el programa recibe y utiliza una dirección virtual
    v
CPU + MMU + TABLAS DE PÁGINAS
    |
    | traducen y verifican el acceso
    v
MEMORIA FÍSICA (RAM), cuando la página está residente
```

La función `malloc` puede no llegar al kernel en una solicitud concreta: quizá ya tenga bloques disponibles. El acceso a una página puede provocar un *page fault* si necesita materialización o atención adicional.

---


# 21. Resumen de instrucciones y operaciones importantes

| Objetivo | Ejemplo | Qué hace |
|---|---|---|
| Guardar registro en stack | `pushq %rax` | Reduce RSP y guarda 8 bytes |
| Recuperar del stack | `popq %rax` | Lee 8 bytes y aumenta RSP |
| Reservar espacio local | `subq $32, %rsp` | Desplaza RSP para reservar 32 bytes |
| Recuperar espacio local | `addq $32, %rsp` | Devuelve RSP a la posición anterior, si corresponde |
| Crear referencia de marco | `movq %rsp, %rbp` | Copia el stack pointer en RBP |
| Desmontar marco clásico | `leave` | Restaura RSP y recupera el RBP anterior |
| Llamar a función | `call funcion` | Guarda dirección de retorno y transfiere el control |
| Retornar | `ret` | Recupera dirección de retorno desde el stack |
| Solicitar bytes | `call malloc@PLT` | Llama a la función de asignación |
| Pasar tamaño a malloc | `movq $N, %rdi` | Coloca el primer argumento |
| Recibir puntero | `%rax` | Registro de retorno |
| Guardar el puntero | `movq %rax, -8(%rbp)` | Almacena la dirección en una variable local |
| Escribir en objeto dinámico | `movl $25, (%rax)` | Escribe 4 bytes en la dirección apuntada |
| Leer objeto dinámico | `movl (%rax), %edx` | Lee 4 bytes desde la dirección apuntada |
| Liberar bloque de malloc | `call free@PLT` | Devuelve el bloque al asignador |

---


# 22. Ideas que debes recordar

1. **El CPU ejecuta instrucciones y accede a direcciones.** No tiene una instrucción especial para acceder al heap o al stack.
2. **`CALL` y `RET` gestionan la dirección de retorno.** En x86-64, `CALL` la coloca automáticamente en el stack.
3. **`%rsp` señala el tope del stack.** `push`, `pop`, `sub` y `add` pueden modificarlo.
4. **`%rbp` es opcional como frame pointer.** Cuando se utiliza, ofrece una referencia estable para acceder a posiciones del marco.
5. **Una función crea una nueva invocación.** Cada llamada activa debe conservar el estado que necesite, especialmente cuando hay llamadas anidadas o recursividad.
6. **Un `if` o un `while` normalmente no crea una invocación nueva.** Utiliza el contexto de la función actual, aunque tenga un ámbito léxico propio.
7. **Alcance, tiempo de vida y almacenamiento no son sinónimos.** Una variable puede dejar de ser visible y terminar su tiempo de vida aunque su espacio reservado permanezca hasta que termine la función.
8. **El heap de memoria es administrado por software.** Un asignador controla bloques libres y ocupados; `%rsp` no realiza esa tarea.
9. **`malloc` y `free` son funciones de biblioteca.** No son instrucciones x86-64 ni necesariamente hacen una llamada al kernel por cada operación.
10. **Stack y heap forman parte del espacio virtual del proceso.** No son memorias físicas separadas.
11. **El kernel administra memoria virtual.** La MMU y las tablas de páginas participan en la traducción de direcciones virtuales a físicas.
12. **La RAM es memoria física.** Una reserva virtual no implica que todas sus páginas estén materializadas inmediatamente en RAM.

La distinción global es esta: el stack es una región que se administra habitualmente mediante una disciplina LIFO y el puntero `%rsp`; el heap es memoria utilizada para reservas dinámicas que administra un asignador mediante metadatos y algoritmos. Ambos terminan accediéndose con las mismas instrucciones de memoria del procesador.

---


# 23. Preguntas para comprobar la comprensión

1. ¿Qué guarda `CALL` en x86-64 y qué hace después `RET` con ese dato?
2. ¿Por qué un bucle no necesita crear un marco de función nuevo en cada iteración?
3. ¿Qué diferencia hay entre alcance léxico y tiempo de vida de una variable?
4. Si `y` solo existe dentro de un `if`, ¿por qué su posición de memoria podría estar reservada desde el prólogo de la función?
5. ¿Qué diferencia hay entre `subq $32, %rsp` y `pushq %rax`?
6. ¿Por qué una función recursiva necesita conservar un estado independiente por cada llamada activa?
7. En `int* p = new int(25)`, ¿qué se guarda en `p` y dónde vive el objeto apuntado?
8. ¿Por qué `%rsp` no basta para administrar bloques que se liberan en un orden arbitrario?
9. ¿Qué trabajo realiza el asignador y qué trabajo realiza el kernel?
10. ¿Qué diferencia hay entre una dirección virtual y una dirección física?
11. ¿Qué papel cumplen la MMU y las tablas de páginas?
12. ¿Por qué una llamada a `malloc` no tiene que generar necesariamente una llamada al sistema?
13. ¿Por qué un *page fault* no siempre representa un error del programa?
14. ¿Cuál es la diferencia entre `malloc/free` y `new/delete`?
15. ¿Por qué no se debe acceder a un objeto después de liberarlo?


---

# Glosario breve de términos

| Término | Significado en este documento |
|---|---|
| ABI | Reglas de interoperabilidad entre funciones compiladas: argumentos, retornos, registros y alineación. |
| Asignador (*allocator*) | Software que administra bloques de memoria dinámica. |
| Dirección virtual | Dirección que utiliza el proceso dentro de su propio espacio virtual. |
| Dirección física | Dirección de una ubicación de memoria física. |
| Frame pointer | Registro usado como referencia estable de un marco; en el ejemplo es `%rbp`. |
| Heap tradicional (`[heap]`) | Región virtual asociada al segmento de datos y al *program break* en Linux. |
| Kernel | Núcleo del sistema operativo, con privilegios para gestionar recursos y memoria. |
| LIFO | Regla “último en entrar, primero en salir”, usada por el stack convencional. |
| MMU | Hardware que participa en la traducción de direcciones virtuales y la comprobación de permisos. |
| Page fault | Excepción que aparece cuando un acceso a memoria requiere atención de gestión de páginas. |
| PLT | Estructura usada por el enlace dinámico para transferir llamadas a funciones externas. |
| Proceso | Instancia de un programa en ejecución con un espacio de direcciones y recursos propios. |
| Stack | Región y disciplina de memoria usada para el estado de llamadas y almacenamiento automático. |
| TLB | Caché de traducciones de direcciones virtuales a físicas. |
| Caché L1/L2/L3 | Memoria rápida del procesador que conserva copias de líneas de memoria para acelerar accesos. |
| Línea de caché | Bloque de datos que se transfiere y almacena como unidad en una caché. |
| VMA | Descripción de un intervalo de direcciones virtuales y sus propiedades en Linux. |

Este glosario es un repaso final; las definiciones principales aparecen antes de que cada término se necesite en la explicación.
