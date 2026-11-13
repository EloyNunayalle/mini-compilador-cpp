#include <iostream>
using namespace std;

struct Rectangulo {
    int ancho;
    int alto;

    int area() {
        return ancho * alto;
    }

    int perimetro() {
        return 2 * (ancho + alto);
    }

    void escalar(int k) {
        ancho = ancho * k;
        alto = alto * k;
    }
};

struct Cuenta {
    int saldo;

    void depositar(int monto) {
        saldo = saldo + monto;
    }

    bool retirar(int monto) {
        if (monto > saldo) {
            return false;
        }
        saldo = saldo - monto;
        return true;
    }
};

int main() {
    Rectangulo r;
    r.ancho = 4;
    r.alto = 3;
    cout << "area = " << r.area() << ", perimetro = " << r.perimetro() << "\n";
    r.escalar(2);
    cout << "escalado: " << r.ancho << " x " << r.alto << " area " << r.area() << "\n";

    Cuenta c;
    c.saldo = 100;
    c.depositar(50);
    if (c.retirar(500)) {
        cout << "retiro de 500 aceptado\n";
    } else {
        cout << "retiro de 500 rechazado\n";
    }
    c.retirar(120);
    cout << "saldo final = " << c.saldo << "\n";
    return 0;
}
