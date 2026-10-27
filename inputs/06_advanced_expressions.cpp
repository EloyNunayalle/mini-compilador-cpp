#include <iostream>
using namespace std;

/*
   Prueba de operadores avanzados
   a nivel de bits y desplazamientos
*/
int main() {
    int a = 8;
    int b = 2;
    int c = (a << 2) | (b >> 1);
    int d = a & b;
    int e = a ^ b;
    int f = ~a;
    // Asignación encadenada
    int x = 0;
    int y = 0;
    x = y = 42;
    return c + d + e + f + x;
}
