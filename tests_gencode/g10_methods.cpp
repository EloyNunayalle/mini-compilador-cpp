#include <iostream>
using namespace std;

class Contador {
private:
    int valor;
    int paso;
public:
    void iniciar(int v, int p) {
        valor = v;
        paso = p;
    }
    void avanzar() {
        valor = valor + paso;
    }
    int actual() {
        return valor;
    }
    int doble() {
        return actual() * 2;
    }
    void mismo(int a, int b, int c, int d, int e, int f) {
        valor = a + b + c + d + e + f + this->paso;
    }
};

struct Rect {
    int w;
    int h;
    int area() { return w * h; }
    double escala(double k) { return area() * k; }
    void asignar(Rect *o) {
        o->w = w;
        o->h = h;
    }
    Rect *yo() { return this; }
};

int main() {
    Contador c;
    c.iniciar(10, 3);
    c.avanzar();
    c.avanzar();
    cout << c.actual() << " " << c.doble() << endl;
    c.mismo(1, 2, 3, 4, 5, 6);
    cout << c.actual() << endl;

    Contador *pc = new Contador;
    pc->iniciar(1, 1);
    pc->avanzar();
    cout << pc->actual() << endl;
    delete pc;

    Rect r;
    r.w = 3;
    r.h = 4;
    cout << r.area() << " " << r.escala(0.5) << endl;
    Rect o;
    r.asignar(&o);
    cout << o.area() << endl;
    cout << r.yo()->area() << endl;
    return 0;
}
