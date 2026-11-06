#include <iostream>
using namespace std;

int uno(int a) { return a + 1; }
int cinco(int a, int b, int c, int d, int e) { return a + b * 2 + c * 3 + d * 4 + e * 5; }
int seis(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f * 100; }
int siete(int a, int b, int c, int d, int e, int f, int g) { return a - b + c - d + e - f + g * 1000; }
int diez(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h + 9 * i + 10 * j;
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int mcd(int a, int b) {
    if (b == 0) return a;
    return mcd(b, a % b);
}

void saludar(int veces) {
    for (int i = 0; i < veces; i++) cout << "hola ";
    cout << endl;
}


int main() {
    cout << uno(1) << " " << cinco(1, 2, 3, 4, 5) << " " << seis(1, 2, 3, 4, 5, 6) << endl;
    cout << siete(1, 2, 3, 4, 5, 6, 7) << " " << diez(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) << endl;
    cout << fib(15) << " " << fact(10) << " " << mcd(48, 18) << endl;
    saludar(3);
    cout << uno(uno(uno(1))) + cinco(uno(0), 0, 0, 0, uno(1)) << endl;
    return 0;
}
