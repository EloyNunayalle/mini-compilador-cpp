#include <iostream>
using namespace std;

class Punto {
public:
    int x;
    int y;

    Punto(int a, int b) : x(a), y(b) {
        cout << "Punto(" << x << ", " << y << ")\n";
    }

    int suma() {
        return x + y;
    }
};

class Contador {
public:
    int cuenta = 0;
    int paso = 1;

    Contador() {
        cout << "Contador() cuenta=" << cuenta << " paso=" << paso << "\n";
    }

    Contador(int p) : paso(p) {
        cout << "Contador(" << p << ") cuenta=" << cuenta << " paso=" << paso << "\n";
    }

    void avanzar() {
        cuenta = cuenta + paso;
    }
};

class Segmento {
public:
    Punto inicio;
    Punto fin;

    Segmento(int x1, int y1, int x2, int y2) : inicio(x1, y1), fin(x2, y2) {
        cout << "Segmento listo\n";
    }

    int dx() {
        return fin.x - inicio.x;
    }
};

int main() {
    Punto p(3, 4);
    cout << "suma = " << p.suma() << "\n";

    Contador a;
    Contador b(5);
    a.avanzar();
    b.avanzar();
    b.avanzar();
    cout << a.cuenta << " " << b.cuenta << "\n";

    Segmento s(1, 2, 10, 20);
    cout << "dx = " << s.dx() << "\n";
    return 0;
}
