# x86-64 en Linux — referencia de ensamblador GNU AS

**Entorno de referencia:** arquitectura x86-64, Linux, formato ELF, GNU Assembler (GAS), sintaxis AT&T y ABI System V AMD64.

**Alcance:** registros generales, transferencia de datos, direccionamiento, aritmética, lógica, desplazamientos, rotaciones, flags, pila, control de flujo y directivas habituales. No cubre en detalle x87, SSE, AVX, AVX-512 ni instrucciones privilegiadas.

**Documentación de referencia:** el manual de Intel define las formas de las instrucciones, sus campos de codificación y el comportamiento del procesador. El manual de GNU `as` define la sintaxis del ensamblador. Cuando se indica el resultado exacto de aceptar o rechazar una expresión fuente, se especifica el comportamiento observado en GNU Binutils 2.44 con `as --64`; no se atribuye esa decisión de sintaxis a la arquitectura del procesador.

---

# 1. Sintaxis y convenciones

## 1.1. Estructura de una instrucción

En sintaxis AT&T, el orden habitual es **fuente, destino**:

```asm
instruccion sufijo fuente, destino

movq $25, %rax       # Copia 25 en RAX
addq %rbx, %rax      # RAX = RAX + RBX
```

En sintaxis Intel, el orden habitual es **destino, fuente**:

```asm
mov rax, 25
add rax, rbx
```

GAS admite ambas sintaxis. Esta chuleta utiliza AT&T.

## 1.2. Símbolos de AT&T

| Símbolo | Significado                                                          | Ejemplo        |
| ------- | -------------------------------------------------------------------- | -------------- |
| `$`     | Operando inmediato: un valor constante codificado en la instrucción. | `$10`          |
| `%`     | Registro.                                                            | `%rax`         |
| `()`    | Operando de memoria mediante una dirección.                          | `(%rax)`       |
| `,`     | Separa operandos o componentes de direccionamiento.                  | `%rax, %rbx`   |
| `*`     | Indica un salto o llamada indirecta.                                 | `call *%rax`   |
| `#`     | Inicia un comentario.                                                | `# comentario` |
| `:`     | Termina la definición de una etiqueta.                               | `inicio:`      |

La diferencia entre `$numero` y `(%registro)` es fundamental:

```asm
movq $10, %rax       # El valor inmediato 10
movq (%rax), %rbx    # El contenido de memoria cuya dirección está en RAX
```

## 1.3. Sufijos de tamaño

| Sufijo | Nombre habitual   | Bits | Bytes |
| ------ | ----------------- | ----:| -----:|
| `b`    | Byte              | 8    | 1     |
| `w`    | Word              | 16   | 2     |
| `l`    | Long / doubleword | 32   | 4     |
| `q`    | Quadword          | 64   | 8     |

Ejemplos:

```asm
movb $10, %al
movw $10, %ax
movl $10, %eax
movq $10, %rax
```

El sufijo indica el tamaño del operando de la instrucción, no necesariamente la cantidad de bits que ocupa un inmediato en el código máquina. Por ejemplo, `addq` opera sobre 64 bits, pero su inmediato aritmético no ocupa 64 bits: la codificación normal utiliza 32 bits extendidos con signo.

En ciertas instrucciones GAS puede inferir el tamaño y permitir que se omita el sufijo. En esta referencia se escribe explícitamente para evitar ambigüedades.

## 1.4. Notación de operandos

Los manuales de arquitectura describen las formas de las instrucciones con abreviaturas. Se utilizarán estas:

| Notación                          | Significado                                                                      |
| --------------------------------- | -------------------------------------------------------------------------------- |
| `r8`, `r16`, `r32`, `r64`         | Registro de 8, 16, 32 o 64 bits.                                                 |
| `m8`, `m16`, `m32`, `m64`         | Operando de memoria de ese tamaño.                                               |
| `r/m8`, `r/m16`, `r/m32`, `r/m64` | Registro **o** memoria del tamaño indicado.                                      |
| `imm8`, `imm16`, `imm32`, `imm64` | Campo inmediato de 8, 16, 32 o 64 bits dentro de la instrucción.                 |
| `disp8`, `disp32`                 | Campo de desplazamiento de dirección de 8 o 32 bits.                             |
| `rel8`, `rel32`                   | Desplazamiento relativo de salto o llamada de 8 o 32 bits.                       |
| `CL`                              | Registro de 8 bits que contiene el contador variable de desplazamiento/rotación. |

`immN` indica el tamaño del **campo inmediato que queda escrito en el código máquina**. GAS selecciona la forma de codificación y coloca en ese campo los N bits correspondientes; no escribe ahí la versión extendida a 16, 32 o 64 bits.

Cuando una instrucción requiere extensión, **la CPU la realiza al ejecutar la instrucción**, después de decodificar el campo. La extensión puede ser con signo o con ceros, según la definición de esa instrucción. Si el campo ya tiene el mismo ancho que el operando, la CPU normalmente utiliza directamente esos bits. Por eso deben distinguirse el valor escrito, el campo emitido por GAS y el uso que hace la CPU del campo.

**El número de bits de un campo no determina por sí solo si el valor se trata como signed o unsigned.** La descripción de cada instrucción establece cómo interpreta la CPU ese campo. En los apartados siguientes también se indica qué expresiones acepta GAS 2.44 en los casos relevantes.

## 1.5. Literales numéricos

GAS acepta las bases habituales:

| Escritura | Base        | Valor matemático |
| --------- | ----------- | ----------------:|
| `$25`     | Decimal     | 25               |
| `$-25`    | Decimal     | −25              |
| `$0xFF`   | Hexadecimal | 255              |
| `$0b1010` | Binario     | 10               |
| `$077`    | Octal       | 63               |

La base no establece signedness. `$-0x1` también expresa −1. En cambio, `$0xFF` expresa el valor positivo 255.

Para entender qué ocurre después, hay que distinguir:

1. **El valor escrito:** por ejemplo, −1 o 255.
2. **El campo codificado:** por ejemplo, los ocho bits `11111111`.
3. **El uso del campo:** la instrucción puede usarlo como dato directo, extenderlo con signo o interpretarlo como contador.

Un patrón de 8 bits `11111111` puede representar −1 si se interpreta como entero con signo, o 255 si se interpreta como entero sin signo. La instrucción determina su uso.

Para un campo de `N` bits, el rango matemático de un entero sin signo es `0` a `2^N − 1`; el rango de un entero con signo en complemento a dos es `−2^(N−1)` a `2^(N−1) − 1`. Estos rangos describen interpretaciones de un patrón de bits; no sustituyen las restricciones particulares de cada instrucción.

Tres operaciones que aparecerán en las instrucciones: **extensión con signo** copia el bit más significativo hacia los bits nuevos (`imm8 = FF` pasa a `imm32 = FFFFFFFF`); **extensión con ceros** rellena los bits nuevos con `0`; **truncamiento** descarta los bits superiores y conserva solo los N bits inferiores.

---

# 2. Registros

## 2.1. Registros de propósito general

| 64 bits | 32 bits | 16 bits | 8 bits  |
| ------- | ------- | ------- | ------- |
| `%rax`  | `%eax`  | `%ax`   | `%al`   |
| `%rbx`  | `%ebx`  | `%bx`   | `%bl`   |
| `%rcx`  | `%ecx`  | `%cx`   | `%cl`   |
| `%rdx`  | `%edx`  | `%dx`   | `%dl`   |
| `%rsi`  | `%esi`  | `%si`   | `%sil`  |
| `%rdi`  | `%edi`  | `%di`   | `%dil`  |
| `%rbp`  | `%ebp`  | `%bp`   | `%bpl`  |
| `%rsp`  | `%esp`  | `%sp`   | `%spl`  |
| `%r8`   | `%r8d`  | `%r8w`  | `%r8b`  |
| `%r9`   | `%r9d`  | `%r9w`  | `%r9b`  |
| `%r10`  | `%r10d` | `%r10w` | `%r10b` |
| `%r11`  | `%r11d` | `%r11w` | `%r11b` |
| `%r12`  | `%r12d` | `%r12w` | `%r12b` |
| `%r13`  | `%r13d` | `%r13w` | `%r13b` |
| `%r14`  | `%r14d` | `%r14w` | `%r14b` |
| `%r15`  | `%r15d` | `%r15w` | `%r15b` |

Las columnas muestran diferentes vistas de los mismos registros físicos; no son registros independientes.

Reglas importantes(se aplica para todo registro general, no solo el registro acumulador)

- Escribir en `%eax` pone en cero los 32 bits superiores de `%rax`.
- Escribir en `%ax` o `%al` modifica solo esa parte y conserva los bits superiores.
- `%ah`, `%bh`, `%ch` y `%dh` son los bytes altos de los cuatro registros históricos. No pueden usarse en una instrucción que requiera un prefijo REX.

## 2.2. Registros especiales

| Registro       | Función                                                                      |
| -------------- | ---------------------------------------------------------------------------- |
| `%rip`         | Puntero a la siguiente instrucción que ejecutará el procesador.              |
| `%rsp`         | Puntero de pila.                                                             |
| `%rbp`         | Registro que puede utilizarse como base del marco de pila.                   |
| `%rflags`      | Registro que contiene flags de estado y control.                             |
| `%rax`         | Acumulador; también aparece implícitamente en multiplicaciones y divisiones. |
| `%rdx`         | Mitad alta de ciertos resultados y dividendos.                               |
| `%rcx` / `%cl` | `%cl` contiene el contador variable de desplazamientos y rotaciones.         |

---

# 3. RFLAGS y flags de estado

`RFLAGS` contiene bits de estado y control. Las instrucciones aritméticas y lógicas actualizan algunos de estos bits para que el programa pueda consultar el resultado y tomar decisiones.

| Flag | Nombre          | Significado                                                                           |
| ---- | --------------- | ------------------------------------------------------------------------------------- |
| `CF` | Carry Flag      | Acarreo; en restas se usa para indicar borrow. Fundamental en comparaciones unsigned. |
| `OF` | Overflow Flag   | Desbordamiento cuando el resultado se interpreta con signo.                           |
| `ZF` | Zero Flag       | Se activa si el resultado es cero.                                                    |
| `SF` | Sign Flag       | Copia el bit más significativo del resultado.                                         |
| `PF` | Parity Flag     | Se activa si los 8 bits inferiores tienen cantidad par de unos.                       |
| `AF` | Auxiliary Carry | Acarreo entre los bits 3 y 4; se utiliza principalmente en operaciones BCD.           |

### Posición de los flags en RFLAGS

| Bit(s) | Flag/campo | Función |
| ------:| ---------- | ------- |
| 0 | `CF` | Carry |
| 1 | Reservado | Se lee como 1 |
| 2 | `PF` | Parity |
| 3 | Reservado | — |
| 4 | `AF` | Auxiliary Carry |
| 5 | Reservado | — |
| 6 | `ZF` | Zero |
| 7 | `SF` | Sign |
| 8 | `TF` | Trap |
| 9 | `IF` | Interrupt enable |
| 10 | `DF` | Direction |
| 11 | `OF` | Overflow |
| 12–13 | `IOPL` | Nivel de privilegio de E/S |
| 14 | `NT` | Nested task |
| 15 | Reservado | — |
| 16 | `RF` | Resume |
| 17 | `VM` | Virtual-8086 mode |
| 18 | `AC` | Alignment check |
| 19 | `VIF` | Virtual interrupt |
| 20 | `VIP` | Virtual interrupt pending |
| 21 | `ID` | Identificación de CPUID |
| 22–63 | Reservados | — |

Los flags no son variables independientes: son bits del registro `RFLAGS`. Cada instrucción especifica cuáles actualiza, conserva, limpia o deja indefinidos. Algunos bits de control solo pueden modificarse bajo determinadas condiciones de privilegio. La tabla de modificación de los flags aritméticos aparece en la sección de comparación y saltos, después de estudiar las operaciones que los producen.

---

# 4. Direccionamiento de memoria

## 4.1. Forma general en AT&T

```asm
desplazamiento(base, indice, escala)
```

La dirección efectiva se calcula como:

`EA = desplazamiento + base + índice × escala`

| Componente     | Significado                                            |
| -------------- | ------------------------------------------------------ |
| `EA`           | Dirección efectiva (Effective Address).                |
| Desplazamiento | Constante o expresión simbólica sumada a la dirección. |
| Base           | Contenido de un registro base.                         |
| Índice         | Contenido de un registro índice.                       |
| Escala         | Factor 1, 2, 4 u 8 aplicado al índice.                 |

Los componentes base e índice pueden omitirse. Si no se indica escala, se utiliza 1.

## 4.2. Ejemplos de direccionamiento

| Expresión        | Dirección calculada                                                        |
| ---------------- | -------------------------------------------------------------------------- |
| `(%rax)`         | `RAX`                                                                      |
| `8(%rax)`        | `RAX + 8`                                                                  |
| `-8(%rax)`       | `RAX − 8`                                                                  |
| `(%rax,%rcx)`    | `RAX + RCX`                                                                |
| `(%rax,%rcx,4)`  | `RAX + RCX × 4`                                                            |
| `8(%rax,%rcx,4)` | `RAX + RCX × 4 + 8`                                                        |
| `etiqueta(%rip)` | `RIP_siguiente + desplazamiento_relativo_a_rip_de_etiqueta`(caso especial) |

Ejemplo:

```asm
movq 8(%rax,%rcx,4), %rdx
```

Si `RAX = 1000` y `RCX = 3`:

`EA = 1000 + (3 × 4) + 8 = 1020`

`movq` lee ocho bytes desde la dirección 1020.

## 4.3. Restricción del desplazamiento de memoria

En el direccionamiento general de x86-64, el desplazamiento codificado es `disp8` o `disp32`:

- `disp8` es un desplazamiento **con signo** de −128 a 127. Permite una codificación más corta.
- `disp32` es un desplazamiento **con signo** de −2³¹ a 2³¹−1.
- El desplazamiento se suma a la base/índice; no es el tamaño del dato que se lee o escribe.

GAS selecciona `disp8` cuando el desplazamiento cabe en ese campo; si no, utiliza `disp32`. Un desplazamiento que no pueda representarse en el formato disponible no puede codificarse como una referencia ordinaria base+índice.

## 4.4. Dirección frente a contenido

Supongamos que `RAX = 1000` y que en la dirección 1000 se almacena el valor 40:

```asm
leaq (%rax), %rbx    # RBX = 1000: calcula la dirección
movq (%rax), %rbx    # RBX = 40: lee el contenido de memoria
```

`LEA` calcula una dirección efectiva y la guarda en un registro. No lee la memoria situada en esa dirección.

## 4.5. Etiquetas y direccionamiento relativo a RIP

Una etiqueta identifica la posición actual dentro de una sección:

```asm
.section .data
numero:
    .quad 40

.section .text
inicio:
    movq $10, %rax
```

Una referencia habitual en x86-64 es relativa a `RIP`:

```asm
leaq numero(%rip), %rax   # Guarda la dirección de numero
movq numero(%rip), %rax   # Lee el contenido de numero
```

El procesador calcula la dirección relativa usando el `RIP` de la instrucción siguiente y un `disp32` con signo:

`EA = RIP_siguiente + sign_extend(disp32)`

Por tanto, el alcance del desplazamiento RIP-relative es −2³¹ a 2³¹−1 bytes respecto de la instrucción siguiente. Esta forma es común en ejecutables PIE y bibliotecas compartidas.

---

# 5. Transferencia de datos

## 5.1. MOV — copiar datos

`MOV` copia la fuente en el destino. No modifica los flags.

Formas habituales:

| Forma  | Fuente                                            | Destino       |
| ------ | ------------------------------------------------- | ------------- |
| `movb` | `r8`, `m8` o `imm8`                               | `r8` o `m8`   |
| `movw` | `r16`, `m16` o `imm16`                            | `r16` o `m16` |
| `movl` | `r32`, `m32` o `imm32`                            | `r32` o `m32` |
| `movq` | `r64`, `m64` o inmediato admitido para el destino | `r64` o `m64` |

Restricciones generales:

- No admite memoria como fuente y memoria como destino en una misma instrucción.
- El destino no puede ser un inmediato.
- Los operandos de registro/memoria deben tener el tamaño compatible con el sufijo.

### Inmediatos de MOV a registros

**Campo e interpretación de la CPU:**
- Para destinos de 8, 16 y 32 bits, `imm8`, `imm16` e `imm32` ocupan el mismo ancho que el destino; la CPU copia esos bits directamente, sin extensión.
- Para un destino de 64 bits, la codificación puede contener `imm64` directo o `imm32` extendido con signo. La CPU realiza esta extensión al ejecutar la forma `imm32`.
- `MOV` no interpreta por sí mismo el patrón como una cantidad signed o unsigned: copia bits. La diferencia aparece en la forma de codificación y en cómo se utilicen después esos bits.

**Aceptación por GAS 2.44:** para destinos de 8, 16 y 32 bits, GAS acepta literales negativos y escribe el patrón de complemento a dos del ancho del campo; los valores que exceden el campo pueden truncarse y generar advertencias. Para un destino de 64 bits en registro, un literal negativo que cabe en signed `imm32` puede usar esa forma corta y la CPU lo extiende con signo; un valor positivo como `4294967295` necesita `imm64`. `movabsq` fuerza la forma de 64 bits. Para memoria de 64 bits solo existe `imm32` extendido con signo; se precisa más abajo.

```asm
movb $255, %al
movw $50000, %ax
movl $4000000000, %eax
movq $4294967295, %rax
movabsq $0x123456789ABCDEF0, %rax
```

- Para registros de 8, 16 y 32 bits se codifica un inmediato del mismo ancho: `imm8`, `imm16` o `imm32`. En estos campos se copia el patrón de bits al registro.
- Para un registro de 64 bits existe una forma con `imm64`, que copia los 64 bits completos. GAS puede seleccionarla para `movq` cuando el valor no cabe en un inmediato de 32 bits extendido con signo.
- `movabsq` solicita explícitamente la forma con inmediato completo de 64 bits.
- Otra forma de `MOV` a un registro de 64 bits usa `imm32` extendido con signo. Por ejemplo, `movq $-1, %rax` puede codificarse con 32 bits `FFFFFFFF` y extenderse a 64 bits.

**Ejemplo que distingue las formas:**

```asm
movq $-1, %rax          # Puede usar imm32 FFFFFFFF y extensión de signo
movq $4294967295, %rax  # Valor positivo: requiere imm64
```

En el segundo caso GAS utiliza una codificación de inmediato de 64 bits. No debe confundirse con `movq` hacia memoria.

### Inmediato de MOV a memoria

```asm
movq $-1, (%rax)        # Válido: imm32 FFFFFFFF extendido con signo
movq $4294967295, (%rax) # Error: no cabe como imm32 con signo extendido
```

Para `movq` a memoria, la codificación disponible lleva un `imm32` que la CPU extiende con signo a 64 bits. **No existe una forma ordinaria `MOV r/m64, imm64`.** Si se necesita almacenar un valor arbitrario de 64 bits en memoria, primero se carga en un registro con `movabsq` y luego se almacena:

```asm
movabsq $0x123456789ABCDEF0, %rbx
movq %rbx, (%rax)
```

## 5.2. MOVSX / MOVSXD — extensión con signo

Copia un valor más pequeño en un registro mayor y extiende el signo. La extensión replica el bit más significativo del valor original en los bits superiores nuevos.

| Mnemónico AT&T | Fuente | Destino |
| -------------- | ------:| -------:|
| `movsbw`       | 8      | 16      |
| `movsbl`       | 8      | 32      |
| `movsbq`       | 8      | 64      |
| `movswl`       | 16     | 32      |
| `movswq`       | 16     | 64      |
| `movslq`       | 32     | 64      |

Fuente: registro o memoria. Destino: registro. No admite inmediato. No modifica flags.

Ejemplo: `11111101` como entero signed de 8 bits vale −3. Al extenderlo a 64 bits, los bits superiores se rellenan con unos y el resultado sigue representando −3.

## 5.3. MOVZX — extensión con ceros

Copia un valor más pequeño a un registro mayor y rellena con ceros los bits superiores.

| Mnemónico AT&T | Fuente | Destino |
| -------------- | ------:| -------:|
| `movzbw`       | 8      | 16      |
| `movzbl`       | 8      | 32      |
| `movzbq`       | 8      | 64      |
| `movzwl`       | 16     | 32      |
| `movzwq`       | 16     | 64      |

Fuente: registro o memoria. Destino: registro. No admite inmediato. No modifica flags.

Para obtener una extensión a 64 bits desde un valor de 32 bits sin signo, basta con escribir en el registro de 32 bits: escribir `%eax` limpia automáticamente la mitad alta de `%rax`.

## 5.4. LEA — calcular dirección efectiva

`LEA` calcula la expresión de direccionamiento indicada y almacena el resultado en un registro. No accede al contenido de memoria.

```asm
leaq numero(%rip), %rax
leaq 8(%rax), %rbx
leaq (%rax,%rcx,4), %rdx
```

El destino es un registro; la fuente es una expresión de direccionamiento. No admite un inmediato aislado ni modifica flags.

## 5.5. XCHG — intercambiar operandos

Intercambia el contenido de dos operandos del mismo tamaño:

```asm
xchgq %rax, %rbx
xchgq %rax, (%rbx)
```

Admite registro↔registro y registro↔memoria. No admite inmediato ni memoria↔memoria. No modifica flags. **Cuando uno de los operandos es memoria, el intercambio es atómico por definición**, incluso sin escribir el prefijo `LOCK`.

## 5.6. CMPXCHG — comparar e intercambiar

Compara el acumulador con el destino. Si son iguales, copia la fuente al destino; si son distintos, copia el destino al acumulador.

```asm
cmpxchgq %rbx, (%rdi)  # Compara RAX con [RDI]
```

Destino: `r/m8`, `r/m16`, `r/m32` o `r/m64`; fuente: registro del mismo tamaño. El acumulador implícito es `AL`, `AX`, `EAX` o `RAX`, según el tamaño. No admite inmediato.

Los flags se actualizan como en una comparación (`CMP`). `ZF=1` indica que el acumulador coincidía con el destino. Puede utilizarse con `LOCK` cuando el destino es memoria para hacer atómica la operación.

## 5.7. XADD — intercambiar y sumar

Suma la fuente al destino y, además, deja en la fuente el valor original del destino:

```asm
xaddq %rax, (%rdi)
```

Destino: `r/m8`, `r/m16`, `r/m32` o `r/m64`; fuente: registro del mismo tamaño. No admite inmediato. Modifica `CF`, `OF`, `ZF`, `SF`, `PF` y `AF` como `ADD`. Puede utilizarse con `LOCK` cuando el destino es memoria.

---

# 6. Operaciones aritméticas

## 6.1. ADD, ADC, SUB, SBB y CMP

Estas instrucciones comparten las formas básicas de operandos y las reglas de codificación de inmediatos.

| Instrucción | Operación conceptual                                                 |
| ----------- | -------------------------------------------------------------------- |
| `ADD`       | `destino = destino + fuente`                                         |
| `ADC`       | `destino = destino + fuente + CF`                                    |
| `SUB`       | `destino = destino − fuente`                                         |
| `SBB`       | `destino = destino − fuente − CF`                                    |
| `CMP`       | Calcula `destino − fuente`, actualiza flags y descarta el resultado. |

Formas generales:

- Registro/memoria con registro.
- Registro con registro/memoria.
- Inmediato con registro/memoria.
- No admiten dos operandos de memoria.

### Restricciones de los inmediatos

**Campo e interpretación de la CPU:**
- Operación de 8 bits: `imm8` directo; la CPU utiliza sus ocho bits.
- Operación de 16 bits: `imm16` directo o `imm8` extendido con signo a 16 bits.
- Operación de 32 bits: `imm32` directo o `imm8` extendido con signo a 32 bits.
- Operación de 64 bits: `imm32` o `imm8`, ambos extendidos con signo a 64 bits.

**Aceptación por GAS 2.44:** para destinos de 64 bits, el valor matemático debe poder representarse mediante el inmediato signed de 32 bits (o signed de 8 bits si se usa la forma corta). Para destinos de 16 y 32 bits, GAS puede elegir `imm8` cuando su extensión con signo produce el mismo patrón de bits que requiere el destino; por eso `addw $65535, %ax` puede utilizar `imm8 = FF`, que se extiende a `FFFF`. Cuando el campo tiene el mismo ancho que el destino, GAS escribe ese patrón de bits y puede truncar valores que exceden el campo, a veces con una advertencia. No uses ese truncamiento como conversión numérica deliberada.


La CPU **no** interpreta `ADD` como signed o unsigned. La suma de bits es la misma; la distinción aparece al interpretar los datos y los flags después.

Ejemplos representativos:

```asm
addb $255, %al         # imm8 directo: FF
addw $300, %ax         # imm16: 012C
addw $-1, %ax          # GAS puede usar imm8 FF extendido a 16 bits
addl $100000, %eax     # imm32 o forma corta si la constante cabe
addq $-1, %rax         # imm8 FF extendido con signo a 64 bits
addq $-129, %rax       # no cabe en imm8 signed; utiliza imm32
addq $0xffffffff, %rax # error en GAS: +4294967295 no cabe en imm32 signed
```

**Por qué `addq $0xffffffff` se rechaza:** el campo de 32 bits de esta forma se extiende con signo. Si se codificara `FFFFFFFF`, la CPU lo ampliaría como −1, no como +4 294 967 295. Como la suma de 64 bits debe recibir el valor pretendido, esa constante positiva no cabe en la forma inmediata de `ADDQ`. Cárgala en un registro con `movabsq` y suma el registro.

### Qué significa elegir `imm8` extendido con signo

La CPU extiende el bit 7 del campo `imm8` hasta el tamaño del destino. Por eso `imm8 = FF` se convierte en `FFFF` para una operación de 16 bits o en `FFFFFFFFFFFFFFFF` para una operación de 64 bits.

GAS puede elegir la forma corta cuando produce el mismo patrón de operando para el tamaño de destino. Por ejemplo:

```asm
addw $65535, %ax
```

Puede codificarse con `imm8 = FF`, porque extender `FF` con signo da `FFFF`, que es el mismo patrón de 16 bits que 65535. En cambio, `addw $255, %ax` no puede usar esa forma corta: extender `FF` produciría `FFFF`, no `00FF`; necesita el inmediato de 16 bits.

Para las operaciones de 8 bits, el inmediato ocupa el mismo ancho que el destino y se usan sus ocho bits directamente. Los resultados aritméticos quedan limitados al ancho del destino.

**Comportamiento concreto de GAS 2.44:** no apliques a todos los campos la regla simplificada «el inmediato debe caber en el rango signed». Cuando el campo tiene el mismo ancho que el destino, la operación utiliza los bits del campo y el resultado queda limitado a ese ancho. Por eso GAS puede aceptar valores negativos fuera del rango signed convencional y conservar sus bits bajos. Ejemplos comprobados:

```asm
addb $255, %al          # Aceptado: campo FF
addb $-129, %al         # Aceptado: campo 7F (−129 mod 256)
addb $256, %al          # Aceptado con warning: campo 00
addw $65535, %ax        # Aceptado; GAS puede elegir imm8 FF extendido a FFFF
addw $-32769, %ax       # Aceptado: imm16 7FFF
```

En las formas cortas, GAS selecciona `imm8` solo cuando la extensión con signo produce el mismo patrón del ancho de destino. En una operación de 64 bits el inmediato disponible es `imm32` extendido con signo: el valor debe caber en signed 32 bits para que la CPU reciba ese mismo valor de 64 bits. Por eso `addq $0xffffffff, %rax` se rechaza, pero `addq $-1, %rax` se acepta. No uses el truncamiento como conversión deliberada: escribe el valor o patrón que realmente quieres operar.

`ADD`, `ADC`, `SUB`, `SBB` y `CMP` modifican `CF`, `OF`, `ZF`, `SF`, `PF` y `AF`.

### Control explícito del Carry Flag

Estas instrucciones permiten preparar o invertir `CF`, que luego puede ser consumido por `ADC` y `SBB`:

| Instrucción | Efecto |
| ----------- | ------ |
| `CLC` | Limpia `CF` (`CF = 0`). |
| `STC` | Establece `CF` (`CF = 1`). |
| `CMC` | Invierte el valor actual de `CF`. |

No reciben operandos ni inmediatos. Los demás flags no se modifican.

## 6.2. INC y DEC

`INC` suma uno y `DEC` resta uno al operando:

```asm
incq %rax
incl (%rbx)
decw %cx
decb (%rdi)
```

Admiten un único operando de registro o memoria. No admiten inmediato explícito. No tienen variantes signed/unsigned. Modifican `OF`, `ZF`, `SF`, `PF` y `AF`; conservan `CF`.

## 6.3. NEG

Calcula el opuesto en complemento a dos:

`destino = 0 − destino`

```asm
negq %rax
negl (%rbx)
```

Admite un operando de registro o memoria. No admite inmediato. Modifica `CF`, `OF`, `ZF`, `SF`, `PF` y `AF`.

- `CF = 0` si el operando original era cero; en otro caso, `CF = 1`.
- `OF = 1` si el operando original era el mínimo valor signed representable, porque su opuesto no cabe en el mismo ancho.

---

# 7. Multiplicación

## 7.1. MUL — multiplicación sin signo

`MUL` multiplica el acumulador por un operando explícito. El resultado ocupa el doble de bits y se guarda en registros implícitos. La notación `DX:AX`, `EDX:EAX` o `RDX:RAX` significa concatenar dos registros: el primero contiene la mitad alta y el segundo la mitad baja del resultado.

| Instrucción | Primer factor implícito | Segundo factor | Resultado |
| ----------- | ----------------------- | -------------- | --------- |
| `mulb`      | `AL`                    | `r/m8`         | `AX`      |
| `mulw`      | `AX`                    | `r/m16`        | `DX:AX`   |
| `mull`      | `EAX`                   | `r/m32`        | `EDX:EAX` |
| `mulq`      | `RAX`                   | `r/m64`        | `RDX:RAX` |

Ejemplo:

```asm
movq $10, %rax
movq $5, %rbx
mulq %rbx             # RDX:RAX = 50
```

Solo admite un operando explícito de registro o memoria. No admite inmediato ni destino explícito.

Para la forma de 64 bits, `RDX:RAX` representa el resultado completo de 128 bits: `RDX` contiene la mitad alta y `RAX` la mitad baja.

Flags: `CF` y `OF` valen 1 si la mitad alta del producto no es cero; en caso contrario valen 0. Los demás flags aritméticos quedan indefinidos.

## 7.2. IMUL — multiplicación con signo

`IMUL` interpreta los factores como enteros con signo. Tiene tres formas: la de un operando, la de dos operandos y la de tres operandos. Las formas de dos y tres operandos están disponibles para tamaños de 16, 32 y 64 bits; la forma de un operando también dispone de la variante de 8 bits. **Solo la forma de tres operandos admite un inmediato**; las formas de uno y dos operandos no lo admiten.

### Forma de un operando

```asm
imulq %rbx            # RDX:RAX = RAX × RBX
```

Utiliza el acumulador y el registro de la mitad alta igual que `MUL`, pero la multiplicación es signed. No admite inmediato.

### Forma de dos operandos

```asm
imulq %rbx, %rax      # RAX = RAX × RBX (se conserva la mitad baja)
```

Fuente: registro o memoria. Destino: registro. El resultado se trunca al ancho del destino.

### Forma de tres operandos

```asm
imulq $5, %rbx, %rax  # RAX = RBX × 5
```

Fuente: registro o memoria; destino: registro; primer operando en AT&T: inmediato.

### Restricciones del inmediato de IMUL

**Campo e interpretación de la CPU:** el inmediato es un factor signed. Para operaciones de 16 bits se codifica como `imm16` directo o `imm8` extendido con signo; para 32 bits, como `imm32` directo o `imm8` extendido con signo; para 64 bits, como `imm32` o `imm8`, ambos extendidos con signo a 64 bits. La CPU realiza la extensión durante la ejecución.

**Aceptación por GAS 2.44:** para `imulq`, el valor debe caber en signed 32 bits; la forma corta se utiliza cuando cabe en signed 8 bits. Para las formas de 16 y 32 bits, el campo completo ocupa el ancho del operando y se interpreta como signed en la multiplicación; GAS puede codificar el patrón de ese ancho. Los inmediatos negativos son válidos cuando su patrón cabe en el campo elegido.

En la forma de tres operandos, la codificación ofrece:

- Operación de 16 bits: `imm16` o `imm8` extendido con signo.
- Operación de 32 bits: `imm32` o `imm8` extendido con signo.
- Operación de 64 bits: `imm32` o `imm8`, ambos extendidos con signo a 64 bits.

Para `imulq`, el valor matemático del inmediato debe poder representarse con signo en 32 bits (o en 8 bits para la codificación corta). No existe una forma `IMUL r64, r/m64, imm64`.

```asm
imulq $-1, %rax, %rbx          # válido
imulq $2147483647, %rax, %rbx  # válido: imm32 signed
imulq $2147483648, %rax, %rbx  # error: no cabe en signed imm32
```

En la forma corta, un `imm8` se extiende con signo. Por ejemplo, `FF` significa −1 para la multiplicación, no +255.

Flags: en las formas de dos y tres operandos, `CF` y `OF` indican si el producto completo no cabe en el tamaño del destino con signo. En la forma de un operando indican si la mitad alta es una extensión del signo de la mitad baja. Los demás flags quedan indefinidos.

---

# 8. División

## 8.1. DIV — división sin signo

El divisor es un registro o una posición de memoria. No admite inmediato. El dividendo se encuentra en un registro doble implícito.

| Instrucción | Dividendo | Divisor | Cociente | Residuo |
| ----------- | --------- | ------- | -------- | ------- |
| `divb`      | `AX`      | `r/m8`  | `AL`     | `AH`    |
| `divw`      | `DX:AX`   | `r/m16` | `AX`     | `DX`    |
| `divl`      | `EDX:EAX` | `r/m32` | `EAX`    | `EDX`   |
| `divq`      | `RDX:RAX` | `r/m64` | `RAX`    | `RDX`   |

Ejemplo de división de 64 bits de 100 entre 5:

```asm
movq $100, %rax
xorq %rdx, %rdx      # Dividendo = 0:RAX
movq $5, %rbx
divq %rbx            # RAX = 20; RDX = 0
```

Para `divq`, el dividendo completo es `RDX × 2⁶⁴ + RAX`. Si el dividendo original cabe en `RAX` como valor sin signo, se pone `RDX` en cero.

La división genera una excepción si el divisor es cero o si el cociente no cabe en el registro de resultado. Los flags aritméticos quedan indefinidos.

## 8.2. IDIV — división con signo

Tiene las mismas posiciones de operandos y resultados que `DIV`, pero interpreta dividendo y divisor como signed. No admite inmediato.

| Instrucción | Dividendo | Divisor | Cociente | Residuo |
| ----------- | --------- | ------- | -------- | ------- |
| `idivb`     | `AX`      | `r/m8`  | `AL`     | `AH`    |
| `idivw`     | `DX:AX`   | `r/m16` | `AX`     | `DX`    |
| `idivl`     | `EDX:EAX` | `r/m32` | `EAX`    | `EDX`   |
| `idivq`     | `RDX:RAX` | `r/m64` | `RAX`    | `RDX`   |

Antes de `IDIV`, se debe extender el signo del dividendo al registro doble:

| Instrucción | Entrada | Resultado |
| ----------- | ------- | --------- |
| `cbtw`      | `AL`    | `AX`      |
| `cwtl`      | `AX`    | `EAX`     |
| `cltq`      | `EAX`   | `RAX`     |
| `cwtd`      | `AX`    | `DX:AX`   |
| `cltd`      | `EAX`   | `EDX:EAX` |
| `cqto`      | `RAX`   | `RDX:RAX` |

Ejemplo:

```asm
movq $-100, %rax
cqto                  # Extiende el signo a RDX:RAX
movq $5, %rbx
idivq %rbx            # RAX = -20; RDX = 0
```

El cociente se trunca hacia cero y el residuo conserva el signo del dividendo. La división genera una excepción por divisor cero o cociente fuera del rango representable. Los flags aritméticos quedan indefinidos.

---

# 9. Operaciones lógicas y de bits

## 9.1. AND, OR y XOR

Estas instrucciones realizan una operación bit a bit y guardan el resultado en el destino:

| Instrucción | Operación                                         |
| ----------- | ------------------------------------------------- |
| `AND`       | El bit resultante es 1 si ambos bits son 1.       |
| `OR`        | El bit resultante es 1 si al menos uno es 1.      |
| `XOR`       | El bit resultante es 1 si los bits son distintos. |

Ejemplos:

```asm
andq $15, %rax
orq $1, %rax
xorq %rbx, %rax
xorq %rax, %rax       # Pone RAX en cero
```

Admiten registro/memoria con registro, registro con registro/memoria e inmediato con registro/memoria. No admiten memoria↔memoria.

### Restricciones de inmediatos

**Campo e interpretación de la CPU:** los campos disponibles son `imm8` para 8 bits; `imm16` o `imm8` extendido con signo para 16 bits; `imm32` o `imm8` extendido con signo para 32 bits; y `imm32` o `imm8` extendidos con signo para 64 bits. En las formas extendidas, la CPU realiza la extensión durante la ejecución. La operación lógica no tiene signedness; el patrón extendido determina qué bits recibe.

**Aceptación por GAS 2.44:** los negativos son válidos cuando el valor puede expresarse en el campo y la extensión requerida conserva el patrón pretendido. Para operaciones de 64 bits, el inmediato debe poder representarse con signo en 32 bits. En campos del mismo ancho que el destino, GAS puede conservar solo los bits que caben y advertir sobre truncamientos.

Sus tamaños y extensión son los mismos que los de las operaciones aritméticas binarias:

- 8 bits: `imm8` directo.
- 16 bits: `imm16` o `imm8` extendido con signo.
- 32 bits: `imm32` o `imm8` extendido con signo.
- 64 bits: `imm32` o `imm8` extendidos con signo a 64 bits.

Ejemplo:

```asm
andq $-1, %rax        # válido: el inmediato se extiende con signo
andq $0xffffffff, %rax # error: +4294967295 no se representa con signed imm32
```

En las operaciones lógicas no hay una versión signed y otra unsigned. Se operan bits. La extensión del inmediato sí importa porque determina qué patrón de 64 bits recibe la CPU.

Flags: `CF=0` y `OF=0`; `ZF`, `SF` y `PF` reflejan el resultado. `AF` queda indefinido.

## 9.2. TEST

`TEST` calcula un AND lógico, descarta el resultado y actualiza flags. Se utiliza para comprobar si determinados bits están activos o si un valor es cero.

**Inmediato:** para un destino de 8, 16 o 32 bits se codifica `imm8`, `imm16` o `imm32` directo; para un destino de 64 bits se codifica `imm32` y la CPU lo extiende con signo a 64 bits. No hay forma corta `imm8` para destinos de 16, 32 o 64 bits. La operación es lógica, no signed/unsigned; en `TESTQ`, el inmediato debe poder representarse mediante signed `imm32` para que su extensión produzca el patrón deseado. GAS 2.44 acepta negativos que caben en esa codificación y rechaza, por ejemplo, el positivo `0xffffffff` para el destino de 64 bits.

```asm
testq %rax, %rax       # Comprueba si RAX es cero
testb $1, %al          # Comprueba el bit menos significativo
```

Admite registro/memoria con registro y formas con inmediato; no admite dos operandos de memoria.

A diferencia de `AND`, `OR` y `XOR`, para un destino de 16, 32 o 64 bits no existe la forma general corta `imm8` extendida con signo. La forma inmediata utiliza el ancho completo del operando: `imm8`, `imm16`, `imm32`; para 64 bits se codifica `imm32` extendido con signo. Hay codificaciones especiales para el acumulador, pero no amplían este límite a un inmediato arbitrario de 64 bits.

Flags: `CF=0`, `OF=0`; `ZF`, `SF` y `PF` se calculan a partir del AND temporal; `AF` queda indefinido.

## 9.3. NOT

Invierte todos los bits del operando:

```asm
notq %rax
notl (%rbx)
```

Admite un registro o memoria. No admite inmediato y no modifica flags.

## 9.4. BT, BTS, BTR y BTC — operaciones sobre un bit

Estas instrucciones seleccionan un bit de un registro o de una secuencia de memoria. El bit seleccionado se copia a `CF`.

| Instrucción | Efecto adicional sobre el bit seleccionado |
| ----------- | ------------------------------------------ |
| `BT` | No modifica el operando. |
| `BTS` | Lo establece en 1 (*set*). |
| `BTR` | Lo pone en 0 (*reset*). |
| `BTC` | Lo invierte (*complement*). |

```asm
btq  $3, %rax       # Copia el bit 3 de RAX a CF
btsq $3, %rax       # Además establece ese bit en 1
btrq %rcx, %rax     # Usa RCX como índice y pone el bit en 0
btcq $7, (%rdi)    # Invierte un bit de memoria
```

**Operandos:** el destino es un registro o memoria de 16, 32 o 64 bits; el índice es un registro del mismo ancho o un inmediato `imm8`. No hay forma de 8 bits.

**Inmediato:** el campo es `imm8`, que codifica un índice de bit. GAS 2.44 acepta literales de 0 a 255 y rechaza literales negativos. Si el destino es un registro, la CPU reduce el índice módulo 16, 32 o 64, según el ancho del destino; por eso `btq $255, %rax` examina el bit 63. Si el destino es memoria, el índice puede señalar bits de elementos contiguos de la secuencia y afectar a la dirección efectiva; no debe asumirse que siempre se reduce módulo el tamaño del operando.

```asm
btq $255, %rax     # Aceptado; en RAX selecciona el bit 63
# btq $-1, %rax    # Rechazado por GAS 2.44
```

`BT` solo consulta. `BTS`, `BTR` y `BTC` modifican el destino. Estas tres últimas permiten el prefijo `LOCK` cuando el destino es memoria, para realizar la modificación atómicamente; `BT` no admite `LOCK`.

Flags: `CF` recibe el valor original del bit seleccionado. `ZF` no cambia; `OF`, `SF`, `AF` y `PF` quedan indefinidos.

## 9.5. BSF y BSR — búsqueda de un bit activo

Buscan un bit igual a 1 en el operando fuente y guardan su índice en el destino.

```asm
bsfq %rax, %rcx     # RCX = índice del bit 1 menos significativo
bsrq %rax, %rcx     # RCX = índice del bit 1 más significativo
```

Fuente: registro o memoria de 16, 32 o 64 bits. Destino: registro del mismo tamaño. No admiten inmediato.

- `BSF` busca desde el bit menos significativo.
- `BSR` busca desde el bit más significativo.
- Si la fuente es cero, `ZF=1` y el destino queda indefinido.
- Si la fuente no es cero, `ZF=0` y el destino contiene el índice encontrado.

Los demás flags (`CF`, `OF`, `SF`, `AF`, `PF`) quedan indefinidos.

## 9.6. BSWAP — invertir bytes

Invierte el orden de los bytes de un registro de 32 o 64 bits:

```asm
bswapl %eax
bswapq %rax
```

Solo admite un registro de 32 o 64 bits; no admite 8 ni 16 bits, memoria ni inmediato. No modifica flags. Es útil para cambiar entre representaciones little-endian y big-endian.

---

# 10. Desplazamientos y rotaciones

## 10.1. SHL / SAL, SHR y SAR

| Instrucción   | Comportamiento                                                                                    |
| ------------- | ------------------------------------------------------------------------------------------------- |
| `SHL` / `SAL` | Desplaza a la izquierda e introduce ceros por la derecha. Son alias: realizan la misma operación. |
| `SHR`         | Desplaza a la derecha e introduce ceros por la izquierda.                                         |
| `SAR`         | Desplaza a la derecha y replica el bit más significativo (desplazamiento aritmético).             |

Ejemplos:

```asm
shlq $1, %rax
shrq $2, %rax
sarq %cl, %rax
```

El destino puede ser un registro o memoria de 8, 16, 32 o 64 bits. El contador puede ser:

1. El valor constante 1, que tiene una codificación especial.
2. Un inmediato de 8 bits (`imm8`).
3. El registro `%cl` para un contador variable.

No se puede utilizar `%al`, `%cx` ni otro registro como contador variable: la forma variable usa `%cl`.

**Inmediato:** el campo es `imm8` y la CPU lo trata como contador unsigned; no lo extiende con signo. GAS 2.44 acepta literales de 0 a 255 y rechaza negativos. La CPU aplica después la máscara de 5 bits para destinos de 8/16/32 bits o de 6 bits para destinos de 64 bits. Por ello, el rango del campo (0–255) no es el rango del contador efectivo.

### Qué significa el inmediato `imm8` y cómo lo trata la CPU

El campo codificado ocupa **8 bits y es un contador unsigned**. La CPU no interpreta el byte como un número signed. Después aplica una máscara:

- Destino de 8, 16 o 32 bits: conserva los 5 bits inferiores (`contador AND 31`).
- Destino de 64 bits: conserva los 6 bits inferiores (`contador AND 63`).

Por tanto, el contador efectivo es 0–31 o 0–63, pero el campo codificado puede contener cualquier byte de 0 a 255. La CPU no rechaza 32, 64 ni 255: primero los enmascara. Después, si el contador resultante supera el tamaño del operando de 8 o 16 bits, el resultado de la instrucción queda indefinido según Intel. Para destinos de 32 y 64 bits, los máximos efectivos (31 y 63) no superan el tamaño del operando.

**Comportamiento de GNU `as` en Binutils 2.44:**

```asm
shlq $63, %rax       # Aceptado; contador efectivo 63
shlq $64, %rax       # Aceptado; 64 AND 63 = 0, no desplaza
shlq $255, %rax      # Aceptado; FF AND 3F = 3F = 63
shlq $256, %rax      # Rechazado: no cabe en imm8
shlq $-1, %rax       # Rechazado: el contador inmediato se valida como unsigned
```

Ejemplo de la diferencia entre máscara y tamaño del destino: `shlb $9, %al` tiene contador efectivo 9; la CPU no rechaza el byte inmediato, pero como 9 supera el ancho de 8 bits, el resultado queda indefinido. En cambio, `shlq $64, %rax` tiene contador efectivo 0 y no modifica ni el operando ni los flags.

Este es el caso que suele causar confusión: `shlq $255, %rax` es válido porque 255 cabe en el **campo unsigned de 8 bits**. Que la CPU reduzca después el contador a 6 bits es una regla de ejecución distinta. El ensamblador no convierte −1 en 255: `-1` y `255` son expresiones matemáticas diferentes, aunque compartan los ocho bits bajos.

Con `%cl`, la CPU aplica la misma máscara a los bits de `CL`; no existe validación del valor en tiempo de ensamblado porque el contador se conoce durante la ejecución.

### Flags de los desplazamientos

- `CF` recibe el último bit desplazado fuera cuando está definido.
- `OF` está definido para un desplazamiento de una posición; para otras cantidades queda indefinido.
- `SF`, `ZF` y `PF` se actualizan según el resultado cuando el contador efectivo es distinto de cero y no supera el tamaño del operando.
- `AF` queda indefinido cuando se realiza el desplazamiento.
- Si el contador efectivo es cero, el operando y los flags no cambian.
- Si el contador efectivo supera el tamaño del operando, el resultado y los flags quedan indefinidos.

`SHR` introduce ceros aunque el bit alto original fuera 1. `SAR` conserva el bit alto, por lo que replica el signo del patrón.

## 10.2. ROL y ROR

`ROL` rota a la izquierda y `ROR` rota a la derecha. Los bits que salen por un extremo vuelven a entrar por el otro.

```asm
rolq $1, %rax
rorq %cl, %rax
```

Admiten un destino de registro o memoria y un contador igual a 1, `imm8` o `%cl`.

**Inmediato:** el campo es `imm8`. La CPU lo procesa como contador y aplica la máscara de 5 bits (destinos de hasta 32 bits) o 6 bits (destino de 64 bits). Para destinos de 8 y 16 bits, el contador enmascarado se reduce además módulo 8 o 16. GAS 2.44 acepta valores de 0 a 255 y también literales negativos entre −128 y −1, codificándolos en complemento a dos; por eso `-1` y `255` producen el byte `FF`, que la CPU procesa como contador, no como entero negativo.

Ejemplos comprobados con GAS 2.44:

```asm
rolq $-1, %rax       # Aceptado; codifica FF; la CPU usa 63
rolq $255, %rax      # Aceptado; codifica FF; la CPU usa 63
rolq $256, %rax      # Rechazado: no cabe en imm8
```

`-1` y `255` son expresiones matemáticas distintas, aunque GAS codifique el mismo byte `FF` para ambas. La CPU procesa ese byte como contador, no como entero negativo.

Flags: `CF` se actualiza con el bit que sale de la rotación. `OF` está definido para una rotación efectiva de una posición; para cantidades mayores queda indefinido. Los demás flags se conservan.

## 10.3. RCL y RCR

`RCL` rota a la izquierda a través de `CF`; `RCR` rota a la derecha a través de `CF`. El carry forma parte del anillo de bits de la rotación.

```asm
rclq $1, %rax
rcrq %cl, %rax
```

Aceptan contador 1, `imm8` o `%cl`.

**Inmediato:** el campo es `imm8`. La CPU aplica la máscara de 5 bits (hasta 32 bits) o 6 bits (64 bits); en destinos de 8 y 16 bits, el recuento se reduce después módulo 9 o 17, respectivamente, porque la rotación incluye también `CF`. GAS 2.44 acepta literales de 0 a 255 y rechaza negativos.

Ejemplos comprobados con GAS 2.44:

```asm
rclq $255, %rax      # Aceptado; campo FF; la CPU aplica la máscara
rclq $-1, %rax       # Rechazado por GAS
```

La diferencia con `ROL`/`ROR` está en la aceptación de la expresión fuente por GAS; la CPU aplica la regla de contador definida para cada instrucción.

Flags: `CF` se actualiza; `OF` está definido para una rotación efectiva de una posición y queda indefinido en los demás casos. Los otros flags se conservan.

## 10.4. SHLD y SHRD — desplazamientos dobles

Desplazan el destino e incorporan por el extremo opuesto bits procedentes de un segundo registro.

```asm
shldq $4, %rbx, %rax  # Desplaza RAX a la izquierda e incorpora bits de RBX
shrdq %cl, %rbx, %rax # Desplaza RAX a la derecha e incorpora bits de RBX
```

El destino es registro o memoria de 16, 32 o 64 bits; la fuente es un registro del mismo tamaño. El contador se codifica como `imm8` o se toma de `%cl`; para un desplazamiento de una posición se escribe `$1`.

**Inmediato:** `imm8` unsigned. La CPU aplica una máscara de 5 bits para destinos de 16/32 bits y de 6 bits para destinos de 64 bits. GAS 2.44 acepta valores de 0 a 255 y rechaza negativos. Si el contador resultante es mayor que el ancho del destino (puede ocurrir con destinos de 16 bits), el resultado y los flags quedan indefinidos. Un contador efectivo de cero no modifica el destino ni los flags.

```asm
shldq $255, %rbx, %rax  # Aceptado; contador efectivo 63
# shldq $-1, %rbx, %rax # Rechazado por GAS 2.44
```

Flags: para un desplazamiento no nulo válido, `CF` recibe el último bit desplazado fuera; `SF`, `ZF` y `PF` se actualizan según el resultado; `OF` está definido solo para un contador de una posición y queda indefinido para cantidades mayores; `AF` queda indefinido.

---

# 11. Comparación y control condicional

## 11.1. Resumen de modificación de flags

Leyenda: `M` = modificado; `0` = limpiado; `—` = conservado; `U` = indefinido.

| Instrucción                       | CF  | OF  | ZF  | SF  | PF  | AF  |
| --------------------------------- | --- | --- | --- | --- | --- | --- |
| `ADD`, `ADC`, `SUB`, `SBB`, `CMP` | M   | M   | M   | M   | M   | M   |
| `INC`, `DEC`                      | —   | M   | M   | M   | M   | M   |
| `NEG`                             | M   | M   | M   | M   | M   | M   |
| `MUL`, `IMUL`                     | M*  | M*  | U   | U   | U   | U   |
| `DIV`, `IDIV`                     | U   | U   | U   | U   | U   | U   |
| `AND`, `OR`, `XOR`, `TEST`        | 0   | 0   | M   | M   | M   | U   |
| `NOT`, `MOV`, `LEA`, `XCHG`       | —   | —   | —   | —   | —   | —   |

`*` En `MUL`, `CF` y `OF` indican si la mitad alta del producto sin signo no es cero. En `IMUL`, indican si el producto completo no cabe en el ancho del destino con signo, según la forma utilizada.

Para `SHL`, `SHR`, `SAR` y rotaciones, consultar las reglas específicas de sus apartados; el estado de algunos flags depende del contador efectivo.

## 11.2. CMP y saltos condicionales

`CMP` resta conceptualmente la fuente al destino, actualiza flags y descarta el resultado:

```asm
cmpq $10, %rax       # Flags a partir de RAX - 10
```

`CMP` no tiene una variante signed y otra unsigned. La interpretación se elige al consultar los flags con el salto correspondiente.

### Igualdad

| Salto         | Condición |
| ------------- | --------- |
| `JE` / `JZ`   | `ZF = 1`  |
| `JNE` / `JNZ` | `ZF = 0`  |

### Comparaciones signed

| Salto | Condición        | Lectura                  |
| ----- | ---------------- | ------------------------ |
| `JG`  | `ZF=0` y `SF=OF` | Mayor que, con signo     |
| `JGE` | `SF=OF`          | Mayor o igual, con signo |
| `JL`  | `SF≠OF`          | Menor que, con signo     |
| `JLE` | `ZF=1` o `SF≠OF` | Menor o igual, con signo |

### Comparaciones unsigned

| Salto | Condición       | Lectura                                  |
| ----- | --------------- | ---------------------------------------- |
| `JA`  | `CF=0` y `ZF=0` | Above: mayor, sin signo                  |
| `JAE` | `CF=0`          | Above or equal: mayor o igual, sin signo |
| `JB`  | `CF=1`          | Below: menor, sin signo                  |
| `JBE` | `CF=1` o `ZF=1` | Below or equal: menor o igual, sin signo |

### Otros saltos

| Salto        | Condición       |
| ------------ | --------------- |
| `JO` / `JNO` | `OF=1` / `OF=0` |
| `JS` / `JNS` | `SF=1` / `SF=0` |
| `JC` / `JNC` | `CF=1` / `CF=0` |
| `JP` / `JNP` | `PF=1` / `PF=0` |
| `JMP`        | Siempre salta   |

## 11.3. SETcc

Escribe 1 byte con valor 1 si la condición se cumple y 0 si no se cumple:

```asm
sete %al
setne %bl
setg %cl
setb (%rdi)
```

Destino: registro de 8 bits o memoria de 8 bits. No admite inmediato. No modifica flags.

El sufijo `cc` utiliza las mismas condiciones y aliases que los saltos de la sección 11.2: por ejemplo, `sete/setz`, `setne/setnz`, `setg`, `setl`, `seta`, `setb`, `setae`, `setbe`, `setc` y `setnc`.

## 11.4. CMOVcc

Copia la fuente al destino únicamente si se cumple la condición:

```asm
cmove %rbx, %rax
cmovl %rcx, %rdx
```

Fuente: registro o memoria. Destino: registro de 16, 32 o 64 bits. No admite inmediato como fuente ni modifica flags.

---

# 12. Control de flujo y pila

## 12.1. JMP — salto incondicional

```asm
jmp etiqueta       # Salto directo
jmp *%rax          # Salto indirecto mediante registro
jmp *(%rax)        # Salto indirecto mediante memoria
```

En AT&T, `*` identifica el operando indirecto.

### Codificación de saltos directos

Un salto directo utiliza un desplazamiento relativo a la dirección de la instrucción siguiente:

- `rel8`: desplazamiento signed de −128 a 127 bytes.
- `rel32`: desplazamiento signed de −2³¹ a 2³¹−1 bytes.

GAS normalmente elige una codificación corta cuando el destino cabe en `rel8`; si no, utiliza la forma cercana `rel32`. El salto no lleva una dirección absoluta de 64 bits como inmediato.

## 12.2. Jcc — salto condicional

Los saltos condicionales (`JE`, `JNE`, `JG`, `JL`, `JA`, `JB`, etc.) utilizan las condiciones de flags descritas anteriormente.

```asm
cmpq %rbx, %rax
jl menor
ja mayor_unsigned
```

Los saltos condicionales directos tienen desplazamientos relativos `rel8` o `rel32`, ambos con signo. GAS selecciona la forma que permite alcanzar la etiqueta.

## 12.3. CALL — llamada

Una llamada directa guarda la dirección de retorno en la pila y transfiere la ejecución:

```asm
call funcion
```

La forma directa codifica un `rel32` signed relativo a la instrucción siguiente. No existe una forma `CALL rel64` con un destino inmediato absoluto de 64 bits.

También existe llamada indirecta:

```asm
call *%rax
call *(%rax)
```

En la forma indirecta, el destino se obtiene de un registro o de memoria.

## 12.4. RET — retorno

```asm
ret
ret $16
```

`RET` extrae la dirección de retorno de la pila y continúa la ejecución allí. En la ABI System V AMD64 habitual se utiliza `ret` sin ajuste adicional.

**Inmediato de `RET`:** la forma `ret $n` codifica un campo `imm16` que la CPU interpreta como cantidad unsigned de bytes adicionales que retirar de la pila después de extraer la dirección de retorno. No hay extensión de signo. GAS 2.44 escribe el patrón de 16 bits; por ejemplo, `ret $-1` produce `imm16 = FFFF`, que la CPU utiliza como 65535.

## 12.5. LOOP, LOOPE, LOOPNE y JRCXZ

Estas instrucciones permiten controlar ciclos con el contador implícito:

| Instrucción | Comportamiento |
|---|---|
| `LOOP etiqueta` | Decrementa `RCX` y salta si no queda en cero. |
| `LOOPE` / `LOOPZ` | Decrementa `RCX` y salta si `RCX ≠ 0` y `ZF = 1`. |
| `LOOPNE` / `LOOPNZ` | Decrementa `RCX` y salta si `RCX ≠ 0` y `ZF = 0`. |
| `JRCXZ etiqueta` | Salta si `RCX = 0`; no decrementa el contador. |

En modo de 64 bits, `LOOP`/`LOOPE`/`LOOPNE` usan `RCX`; con prefijo de tamaño de dirección usan `ECX`. `JRCXZ` comprueba `RCX`; `JECXZ` es la forma con tamaño de dirección de 32 bits. No modifican flags.

**Desplazamiento:** estas instrucciones usan únicamente `rel8`, un desplazamiento signed de −128 a +127 bytes relativo a la instrucción siguiente. No reciben un inmediato numérico `$n`; GAS calcula el desplazamiento a partir de la etiqueta y rechaza un destino fuera de alcance.

## 12.6. NOP — no operación

```asm
nop
```

No modifica registros ni flags y permite ocupar espacio en el flujo de instrucciones. GAS y el procesador también admiten codificaciones de `NOP` de varios bytes, utilizadas para relleno y alineamiento. No recibe un inmediato numérico.

## 12.7. PUSH

Introduce un valor en la pila. En modo de 64 bits se admiten operandos de 16 y 64 bits; no existen `PUSH` de 8 ni de 32 bits.

| Forma | Operando permitido | Efecto sobre `RSP` |
|---|---|---:|
| `pushw` | Registro o memoria de 16 bits | `RSP = RSP − 2` |
| `pushq` | Registro o memoria de 64 bits | `RSP = RSP − 8` |

```asm
pushw %ax
pushw (%rbx)
pushq %rax
pushq (%rbx)
```

`PUSHW` utiliza el prefijo de tamaño de operando `66h` en modo de 64 bits; `PUSHQ` utiliza el tamaño de pila predeterminado de 64 bits.

### Inmediatos de PUSHW

- **Campos:** `imm16` directo o `imm8` extendido con signo a 16 bits.
- **CPU:** el `imm16` aporta los 16 bits que se guardan; si se usa `imm8`, la CPU lo extiende con signo antes de almacenarlo.
- **GAS 2.44:** elige `imm8` cuando su extensión con signo produce el patrón de 16 bits requerido; en otro caso utiliza `imm16`. Por ejemplo, `pushw $-1` y `pushw $65535` pueden utilizar `imm8 = FF`, mientras que `pushw $50000` necesita `imm16 = C350`. Para `imm16`, GAS puede escribir los 16 bits inferiores de valores fuera del rango signed convencional; no debe usarse ese truncamiento como conversión.

```asm
pushw $-1       # imm8 FF → CPU extiende a FFFF
pushw $50000    # imm16 C350 → guarda esos 16 bits
```

### Inmediatos de PUSHQ

- **Campos:** `imm8` o `imm32`; no existe `imm64`.
- **CPU:** ambos campos se extienden con signo a 64 bits durante la ejecución.
- **GAS 2.44:** el valor debe poder representarse con signo en 8 o 32 bits, según la forma elegida. Por eso `pushq $-1` es válido, pero `pushq $4294967295` se rechaza: `imm32 = FFFFFFFF` se extendería como −1, no como +4 294 967 295.

```asm
pushq $-1
pushq $2147483647
# pushq $4294967295  # Rechazado: no cabe en signed imm32
```

No existe `pushl`, `pushb` ni una forma ordinaria `PUSH imm64` en modo de 64 bits.

## 12.8. POP

Extrae un valor de la pila y lo coloca en el destino:

```asm
popq %rbx
popq (%rax)
```

Destino: registro o memoria de 16 o 64 bits. No admite inmediato ni destino de memoria a memoria. `POPW` incrementa `RSP` en 2; `POPQ`, en 8.

`PUSH` y `POP` no modifican los flags aritméticos.

### PUSHFQ y POPFQ — guardar y restaurar RFLAGS

```asm
pushfq              # Guarda RFLAGS en la pila (8 bytes)
popfq               # Recupera 8 bytes de la pila hacia RFLAGS
```

No reciben operandos ni inmediatos. `PUSHFQ` decrementa `RSP` en 8; `POPFQ` incrementa `RSP` en 8 después de extraer el valor. La restauración está sujeta a las reglas de privilegio: en código de usuario no todos los bits de control de `RFLAGS` pueden modificarse libremente.

## 12.9. LEAVE

Restablece el marco de pila clásico:

```asm
leave
```

Su efecto conceptual es:

```asm
movq %rbp, %rsp
popq %rbp
```

Se utiliza cuando la función empleó `%rbp` como puntero de marco.

---

# 13. Convención de llamadas System V AMD64

La ABI (*Application Binary Interface*) establece cómo se pasan argumentos, cómo se devuelven resultados y qué registros deben preservarse entre funciones. Es una convención de software, no una instrucción de la CPU.

## 13.1. Argumentos enteros y punteros

Los primeros seis argumentos enteros o punteros se pasan normalmente en:

| Argumento | Registro |
| ---------:| -------- |
| 1         | `%rdi`   |
| 2         | `%rsi`   |
| 3         | `%rdx`   |
| 4         | `%rcx`   |
| 5         | `%r8`    |
| 6         | `%r9`    |

Los argumentos enteros posteriores se pasan en la pila, respetando las reglas de alineación y clasificación de la ABI.

## 13.2. Argumentos de punto flotante

Los argumentos de punto flotante escalares se pasan normalmente en registros XMM (`%xmm0` a `%xmm7`). Las clases de argumentos enteros y SSE se asignan de forma independiente.

## 13.3. Valores de retorno

| Tipo habitual                     | Ubicación   |
| --------------------------------- | ----------- |
| Entero o puntero de hasta 64 bits | `%rax`      |
| Entero de 128 bits                | `%rdx:%rax` |
| Punto flotante escalar            | `%xmm0`     |

Las estructuras y tipos compuestos siguen reglas adicionales de clasificación; algunos se devuelven mediante un puntero oculto suministrado por el llamador.

## 13.4. Preservación de registros

| Registro                                             | Regla                                                                                 |
| ---------------------------------------------------- | ------------------------------------------------------------------------------------- |
| `%rbx`, `%rbp`, `%r12`–`%r15`                        | Callee-saved: la función llamada debe restaurarlos si los modifica.                   |
| `%rsp`                                               | Debe quedar restaurado antes de retornar.                                             |
| `%rax`, `%rcx`, `%rdx`, `%rsi`, `%rdi`, `%r8`–`%r11` | Caller-saved: el llamador debe guardar el valor si lo necesita después de la llamada. |

## 13.5. Alineación de la pila

En una llamada ordinaria, `RSP` debe estar alineado a 16 bytes justo antes de ejecutar `CALL`. Como `CALL` introduce la dirección de retorno (8 bytes), al entrar a la función se cumple normalmente:

`(RSP + 8) mod 16 = 0`

## 13.6. Funciones variádicas

En funciones variádicas como `printf`, `%al` indica cuántos registros vectoriales se utilizaron para pasar argumentos. Si no se pasó ningún argumento vectorial, se pone en cero:

```asm
leaq formato(%rip), %rdi
movq %rax, %rsi
xorl %eax, %eax       # AL = 0 registros vectoriales
call printf@PLT
```

## 13.7. SYSCALL — entrada al kernel Linux

`SYSCALL` transfiere la ejecución desde el programa de usuario al kernel para solicitar un servicio del sistema operativo. No es una llamada normal a una función: utiliza una convención de registros propia de Linux x86-64. El ejemplo presupone que `mensaje` está definido como una cadena o un bloque de bytes.

```asm
movq $1, %rax             # Número de syscall: write
movq $1, %rdi             # arg1: descriptor
leaq mensaje(%rip), %rsi  # arg2: dirección del buffer
movq $5, %rdx             # arg3: cantidad de bytes
syscall
```

Convención de Linux x86-64:

| Uso | Registro |
|---|---|
| Número de syscall | `RAX` |
| Argumentos 1–6 | `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9` |
| Resultado | `RAX` |

`SYSCALL` no recibe operandos explícitos ni inmediato. La instrucción guarda la dirección de retorno en `RCX` y el estado de flags en `R11`; por eso ambos registros quedan destruidos para el llamador. El resultado vuelve en `RAX`. Los errores del kernel se devuelven como valores negativos entre −1 y −4095; las funciones de biblioteca suelen convertirlos a `-1` y establecer `errno`.

---

# 14. Operaciones de cadenas y sincronización

## 14.1. Instrucciones de cadenas

Las instrucciones de cadenas trabajan con posiciones de memoria y registros implícitos. En modo de 64 bits, normalmente utilizan `RSI` como origen, `RDI` como destino, `RCX` como contador de repetición y `RAX` (o su parte de menor tamaño) como acumulador.

| Familia | Instrucciones | Operación |
|---|---|---|
| Mover | `MOVSB`, `MOVSW`, `MOVSL`, `MOVSQ` | Copian desde `[RSI]` hacia `[RDI]`. |
| Almacenar | `STOSB`, `STOSW`, `STOSL`, `STOSQ` | Copian `AL/AX/EAX/RAX` hacia `[RDI]`. |
| Cargar | `LODSB`, `LODSW`, `LODSL`, `LODSQ` | Cargan `[RSI]` en `AL/AX/EAX/RAX`. |
| Comparar memoria | `CMPSB`, `CMPSW`, `CMPSL`, `CMPSQ` | Actualizan flags como `[RSI] − [RDI]`. |
| Comparar con acumulador | `SCASB`, `SCASW`, `SCASL`, `SCASQ` | Actualizan flags como `AL/AX/EAX/RAX − [RDI]`. |

Ejemplos:

```asm
rep movsb           # Copia RCX bytes de [RSI] a [RDI]
rep stosq           # Escribe RAX en RCX posiciones de 8 bytes
repe cmpsb          # Compara mientras RCX != 0 y ZF = 1
repne scasb         # Busca mientras RCX != 0 y ZF = 0
```

Estas instrucciones no reciben inmediatos. Los sufijos indican el tamaño del elemento; las direcciones y el contador son implícitos.

### Prefijos de repetición y dirección

- `REP` repite mientras el contador (`RCX` en modo de 64 bits) no sea cero. Se utiliza habitualmente con `MOVS` y `STOS`; también puede repetirse `LODS`.
- `REPE` / `REPZ` repite mientras el contador no sea cero y `ZF=1`; es útil con `CMPS` y `SCAS`.
- `REPNE` / `REPNZ` repite mientras el contador no sea cero y `ZF=0`; también se usa con `CMPS` y `SCAS`.
- `CLD` limpia `DF` (`DF=0`): `RSI` y/o `RDI` avanzan después de cada elemento.
- `STD` establece `DF` (`DF=1`): `RSI` y/o `RDI` retroceden después de cada elemento.

`CLD` y `STD` no reciben operandos ni inmediatos. Las instrucciones de cadenas modifican los registros de índice y, cuando corresponde, los flags de comparación. La ABI System V AMD64 exige que `DF` esté limpio al entrar y salir de una función; si se usa `STD`, debe ejecutarse `CLD` antes de llamar a otra función o retornar.

## 14.2. LOCK — prefijo de atomicidad

`LOCK` es un prefijo de codificación, no una instrucción independiente. Se utiliza con determinadas operaciones de lectura-modificación-escritura cuyo destino es memoria para hacer la operación atómica respecto de otros procesadores/hilos:

```asm
lock addq %rax, (%rdi)
lock cmpxchgq %rbx, (%rdi)
lock xaddq %rax, (%rdi)
```

Solo puede anteponerse a las formas que la arquitectura permite bloquear y cuando el destino es memoria. Entre las instrucciones cubiertas aquí están `ADD`, `ADC`, `AND`, `BTC`, `BTR`, `BTS`, `CMPXCHG`, `DEC`, `INC`, `NEG`, `NOT`, `OR`, `SBB`, `SUB`, `XADD` y `XOR`. No se coloca delante de una forma con destino en registro. `XCHG` con un operando de memoria ya es atómico de forma implícita y no necesita `LOCK`.

---

# 15. Directivas de GNU Assembler

Las directivas son órdenes para el ensamblador. Organizan secciones, definen datos y símbolos o reservan espacio; no son instrucciones ejecutadas por la CPU.

## 15.1. Secciones

| Directiva         | Función                                                                             |
| ----------------- | ----------------------------------------------------------------------------------- |
| `.text`           | Selecciona la sección de código.                                                    |
| `.data`           | Selecciona la sección de datos inicializados.                                       |
| `.bss`            | Selecciona la sección de datos reservados sin contenido inicializado en el archivo. |
| `.section nombre` | Selecciona una sección por nombre.                                                  |

Secciones ELF habituales:

| Sección   | Contenido y uso habitual                                         |
| --------- | ---------------------------------------------------------------- |
| `.text`   | Código ejecutable.                                               |
| `.data`   | Variables inicializadas y modificables.                          |
| `.rodata` | Constantes y cadenas de solo lectura.                            |
| `.bss`    | Variables reservadas que comienzan en cero durante la ejecución. |

Ejemplo:

```asm
.section .data
numero:
    .quad 40

.section .rodata
mensaje:
    .asciz "Hola"

.section .text
main:
    movq numero(%rip), %rax
```

La forma general ELF es:

```asm
.section nombre, "flags", @tipo
```

Flags habituales: `a` = asignable a memoria; `w` = escritura; `x` = ejecución. Tipos habituales: `@progbits` = sección con contenido en el archivo; `@nobits` = reserva espacio sin almacenar esos bytes en el archivo.

## 15.2. Declaración de datos

| Directiva          | Tamaño  | Bytes |
| ------------------ | -------:| -----:|
| `.byte`            | 8 bits  | 1     |
| `.word` / `.short` | 16 bits | 2     |
| `.long` / `.int`   | 32 bits | 4     |
| `.quad`            | 64 bits | 8     |
| `.float`           | 32 bits | 4     |
| `.double`          | 64 bits | 8     |

Ejemplo:

```asm
.data
a: .byte 10
b: .word 1000
c: .long 50000
d: .quad 123456789
```

Estas directivas emiten datos; no son inmediatos de instrucciones. Si un valor no cabe en el tamaño solicitado, GAS puede advertir y conservar solo los bits que caben. No utilices el truncamiento como sustituto de una conversión deliberada.

En x86-64 los bytes se almacenan en formato *little-endian*: el byte menos significativo aparece primero en memoria.

## 15.3. Cadenas

| Directiva         | Comportamiento                                         |
| ----------------- | ------------------------------------------------------ |
| `.ascii "texto"`  | Almacena los caracteres sin agregar terminador nulo.   |
| `.asciz "texto"`  | Almacena los caracteres y agrega `\0`.                 |
| `.string "texto"` | En GAS para x86, almacena la cadena terminada en cero. |

Ejemplo:

```asm
.section .rodata
mensaje:
    .asciz "Hola"
```

Bytes almacenados: `48 6F 6C 61 00`.

## 15.4. Reserva de espacio

| Directiva         | Función                                                                 |
| ----------------- | ----------------------------------------------------------------------- |
| `.zero n`         | Emite `n` bytes con valor cero.                                         |
| `.space n`        | Reserva/emite `n` bytes, con un valor de relleno opcional.              |
| `.comm nombre,n`  | Declara almacenamiento común de `n` bytes.                              |
| `.lcomm nombre,n` | Declara almacenamiento común local, cuando el formato/target lo admite. |

Ejemplo:

```asm
.bss
buffer:
    .zero 64
```

## 15.5. Alineamiento

| Directiva    | Función                                                  |
| ------------ | -------------------------------------------------------- |
| `.balign n`  | Alinea la ubicación actual a un múltiplo de `n` bytes.   |
| `.p2align k` | Alinea la ubicación actual a un múltiplo de `2^k` bytes. |

Ejemplo:

```asm
.balign 8
numero:
    .quad 100

.p2align 4
tabla:
    .quad 1, 2, 3, 4
```

En el segundo ejemplo, la tabla queda alineada a 16 bytes.

## 15.6. Símbolos y visibilidad

| Directiva                          | Función                                    |
| ---------------------------------- | ------------------------------------------ |
| `.globl nombre` / `.global nombre` | Hace visible el símbolo para el enlazador. |
| `.type nombre, @function`          | Marca el símbolo como función ELF.         |
| `.type nombre, @object`            | Marca el símbolo como objeto ELF.          |
| `.size nombre, expresión`          | Define el tamaño del símbolo.              |
| `.equ nombre, valor`               | Define un símbolo constante.               |
| `.set nombre, valor`               | Asigna un valor a un símbolo.              |

Ejemplo:

```asm
.globl main
.type main, @function
main:
    movl $0, %eax
    ret
.size main, .-main
```

La expresión `.-main` es la diferencia entre la posición actual (`.`) y el inicio de `main`; por tanto, calcula el tamaño de la función.

---

# 16. Programa integrado

El siguiente ejemplo combina secciones, etiquetas, acceso a datos, operaciones, convención de llamadas y retorno desde `main`:

```asm
.section .rodata
formato:
    .asciz "Resultado: %ld\n"

.section .data
numero:
    .quad 40

.section .text
.globl main
.type main, @function
main:
    pushq %rbp
    movq %rsp, %rbp

    # Leer el contenido de numero
    movq numero(%rip), %rax

    # Sumar 2
    addq $2, %rax

    # Primer argumento: dirección de la cadena de formato
    leaq formato(%rip), %rdi

    # Segundo argumento: número que se imprimirá
    movq %rax, %rsi

    # printf es variádica: cero argumentos vectoriales
    xorl %eax, %eax
    call printf@PLT

    # Valor de retorno de main
    movl $0, %eax
    leave
    ret
.size main, .-main
```

| Código                     | Función                                                                     |
| -------------------------- | --------------------------------------------------------------------------- |
| `.section .rodata`         | Selecciona la sección de constantes.                                        |
| `.asciz`                   | Define una cadena terminada en cero.                                        |
| `.section .data`           | Selecciona datos inicializados.                                             |
| `.quad 40`                 | Almacena 8 bytes con el valor 40.                                           |
| `.section .text`           | Selecciona la sección de código.                                            |
| `.globl main`              | Exporta el símbolo `main`.                                                  |
| `movq numero(%rip), %rax`  | Lee el dato de `numero`.                                                    |
| `addq $2, %rax`            | Suma 2 a `RAX`; el inmediato se codifica en el tamaño permitido por `ADDQ`. |
| `leaq formato(%rip), %rdi` | Obtiene la dirección de la cadena.                                          |
| `movq %rax, %rsi`          | Coloca el segundo argumento de `printf`.                                    |
| `xorl %eax, %eax`          | Indica cero argumentos vectoriales.                                         |
| `call printf@PLT`          | Llama a `printf` mediante la PLT.                                           |
| `leave`                    | Restaura el marco de pila.                                                  |
| `ret`                      | Retorna al llamador.                                                        |

---

# 17. Referencias técnicas

1. **Intel, Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 2 — Instruction Set Reference.** Define operandos, formas de codificación, immediates, flags y comportamiento de cada instrucción: <https://cdrdv2-public.intel.com/789581/325383-sdm-vol-2abcd.pdf>
2. **Intel, Software Developer’s Manual, Volume 1 — Basic Architecture.** Explica registros, operación básica, flags y representación de datos: <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html>
3. **GNU Binutils, GNU Assembler — i386 Dependent Features.** Define las convenciones de GAS para x86, sintaxis AT&T/Intel, registros, sufijos, memoria y saltos: <https://sourceware.org/binutils/docs/as/i386_002dDependent.html>
4. **System V AMD64 ABI.** Define la convención de llamadas de funciones de usuario para programas Linux x86-64: <https://gitlab.com/x86-psABIs/x86-64-ABI>
5. **Intel SDM, Volume 1 — Basic Architecture.** Define la organización de `RFLAGS` y los bits reservados y de control: <https://cdrdv2-public.intel.com/843827/253665-sdm-vol-1-dec-24.pdf>
6. **Intel SDM, Volume 2 — Instruction Set Reference.** Incluye las formas y semántica de `PUSH`, operaciones de bits, desplazamientos dobles, flags, strings y `SYSCALL`: <https://cdrdv2-public.intel.com/835757/325383-sdm-vol-2abcd.pdf>
7. **Linux man-pages, `syscall(2)`.** Resume los registros usados por las llamadas al sistema en x86-64: <https://man7.org/linux/man-pages/man2/syscall.2.html>
8. **Linux kernel, implementación `nolibc` x86-64.** Documenta el número de syscall, los seis registros de argumentos, el resultado en `RAX` y los registros destruidos `RCX`/`R11`: <https://code.googlesource.com/linux/torvalds/linux/+/75f2c0b3690702c90863c2e138cb5520670845ea/tools/include/nolibc/arch-x86.h>

**Cómo resolver una duda concreta:** primero identifica la forma de operandos de la instrucción en el manual Intel; después verifica el ancho y el tratamiento del campo inmediato; finalmente comprueba la sintaxis y la aceptación de la expresión en el manual de GAS o con `as --64`. No deduzcas signedness únicamente del número de bits del campo.
