#include <iostream>
using namespace std;

class Recurso {
public:
    int id;

    Recurso(int i) : id(i) {
        cout << "ctor " << id << "\n";
    }

    Recurso(const Recurso& o) : id(o.id + 100) {
        cout << "copia de " << o.id << " -> " << id << "\n";
    }

    Recurso& operator=(const Recurso& o) {
        cout << "asignacion " << o.id << " a " << id << "\n";
        id = o.id;
        return *this;
    }

    ~Recurso() {
        cout << "dtor " << id << "\n";
    }
};

void por_valor(Recurso r) {
    cout << "dentro de por_valor con " << r.id << "\n";
}

Recurso crear(int i) {
    Recurso t(i);
    return t;
}

int main() {
    Recurso a(1);
    Recurso b(a);
    Recurso c(2);
    c = a;
    por_valor(c);
    Recurso d = crear(7);
    cout << "fin de main\n";
    return 0;
}
