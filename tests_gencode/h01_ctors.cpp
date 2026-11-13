#include <iostream>
#include <string>
using namespace std;
struct Punto {
    int x = 1;
    int y = 2;
    string nombre = "sin nombre";
    Punto() { cout << "default " << x << " " << y << " " << nombre.c_str() << endl; }
    Punto(int a, int b) : y(b), x(a) { cout << "args " << x << " " << y << " " << nombre.c_str() << endl; }
};
struct Solo { int v; Solo(int a) : v(a) { cout << "Solo " << v << endl; } };
struct Cont {
    Solo s;
    int k;
    Cont(int a) : s(a + 1), k(a) { cout << "Cont " << s.v << " " << k << endl; }
};
struct Imp { int a = 7; double d = 2.5; };
int main() {
    Punto p;
    Punto q(3, 4);
    Punto r = Punto(5, 6);
    Cont c(10);
    Imp i;
    cout << i.a << " " << i.d << endl;
    Punto* pp = new Punto(8, 9);
    cout << pp->x << pp->y << endl;
    delete pp;
    return 0;
}
