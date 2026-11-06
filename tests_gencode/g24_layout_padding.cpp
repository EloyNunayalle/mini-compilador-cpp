// Mixto: c@0 i@4 d@8 x@16 b@24, size 32, align 8
// ASM-CONTAINS: addq $4, %rax
// ASM-CONTAINS: addq $16, %rax
// ASM-CONTAINS: addq $24, %rax
// ASM-CONTAINS: imulq $32, %rax
// Compacto: a@0 b@1 c@4, size 8
// ASM-CONTAINS: imulq $8, %rax
// Linea: Punto(8) Punto(8) char -> size 20, align 4
// ASM-CONTAINS: imulq $20, %rax
// Mixto global: 8 bytes de alineacion y 32 de tamano
// ASM-CONTAINS: .balign 8
// ASM-CONTAINS: .zero 32
// accesos con el tamano correcto
// ASM-CONTAINS: movb %al,
// ASM-CONTAINS: movl %eax,
// ASM-CONTAINS: movsd %xmm0,
// ASM-CONTAINS: movq %rax,
// ASM-CONTAINS: movss %xmm0,
// ASM-ABSENT: movq %rax, -4(%rbp)
#include <iostream>
using namespace std;

struct Mixto {
    char c;
    int i;
    char d;
    double x;
    bool b;
};

struct Compacto {
    char a;
    char b;
    int c;
};

struct Punto {
    int x;
    int y;
};

struct Linea {
    Punto a;
    Punto b;
    char etiqueta;
};

Mixto gm;

int main() {
    Mixto arr[3];
    for (int k = 0; k < 3; k++) {
        arr[k].c = 'a' + k;
        arr[k].i = k * 100;
        arr[k].d = 'z' - k;
        arr[k].x = k + 0.5;
        arr[k].b = k % 2 == 1;
    }
    for (int k = 0; k < 3; k++) {
        cout << arr[k].c << arr[k].i << arr[k].d << arr[k].x << arr[k].b << endl;
    }
    Compacto cc[2];
    cc[0].a = 'x'; cc[0].b = 'y'; cc[0].c = 7;
    cc[1].a = 'p'; cc[1].b = 'q'; cc[1].c = 8;
    cout << cc[0].a << cc[0].b << cc[0].c << cc[1].a << cc[1].b << cc[1].c << endl;
    Linea ls[2];
    ls[1].a.x = 1; ls[1].b.y = 2; ls[1].etiqueta = 'E';
    ls[0].etiqueta = 'D';
    cout << ls[0].etiqueta << ls[1].etiqueta << ls[1].a.x << ls[1].b.y << endl;
    gm.i = 77;
    gm.x = 1.25;
    cout << gm.i << " " << gm.x << endl;
    char c1 = 'a';
    int i1 = 1000;
    double d1 = 0.5;
    float f1 = 0.25;
    int *p1 = &i1;
    cout << c1 << i1 << d1 << f1 << *p1 << endl;
    return 0;
}
