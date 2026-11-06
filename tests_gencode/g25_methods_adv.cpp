#include <iostream>
#include <string>
using namespace std;

int triple(int x) { return x * 3; }

class Acum {
public:
    int total;
    double media;
    string nombre;
    int fib(int n) {
        if (n < 2) return n;
        return fib(n - 1) + fib(n - 2);
    }
    void sumar(int v) {
        total = total + triple(v);
    }
    double ponderar(double a, double b, int k) {
        media = (a * k + b) / (k + 1);
        return media;
    }
    int *ptrTotal() { return &total; }
    int &refTotal() { return total; }
    void rotular(string &s) {
        nombre = s;
        nombre.append("#");
    }
    int largo() { return nombre.size(); }
    int total9(int a, int b, int c, int d, int e, int f, int g, int h) {
        return a + b + c + d + e + f + g + h + total;
    }
    double mixto(double a, int b, double c, int d, double e) {
        return a + b + c + d + e + media;
    }
};

struct Simple {
    int v;
    void subir(int k) { v = v + triple(k); }
};

Acum global;

int usar(Acum *a) {
    a->sumar(2);
    return a->total;
}

int main() {
    Acum a;
    a.total = 0;
    a.sumar(1);
    a.sumar(2);
    cout << a.total << " " << a.fib(12) << endl;
    cout << a.ponderar(1.5, 2.5, 3) << " " << a.media << endl;
    *a.ptrTotal() = 100;
    a.refTotal() = a.refTotal() + 1;
    cout << a.total << endl;
    string s = "etq";
    a.rotular(s);
    cout << a.nombre << a.largo() << endl;
    cout << a.total9(1, 2, 3, 4, 5, 6, 7, 8) << endl;
    cout << a.mixto(0.5, 1, 0.25, 2, 0.125) << endl;
    cout << usar(&a) << endl;
    global.total = 5;
    global.sumar(3);
    cout << global.total << " " << usar(&global) << endl;
    Simple *pa = new Simple;
    pa->v = 1;
    pa->subir(4);
    cout << pa->v << endl;
    delete pa;
    return 0;
}
