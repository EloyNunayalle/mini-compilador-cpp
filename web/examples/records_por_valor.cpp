#include <iostream>
#include <string>
using namespace std;

struct Par {
    int a;
    int b;
};

struct Grande {
    int v0;
    int v1;
    int v2;
    int v3;
    int v4;
    int v5;
};

struct Rotulo {
    string texto;
    int numero;
};

Par intercambiar(Par p) {
    Par r;
    r.a = p.b;
    r.b = p.a;
    return r;
}

Grande duplicar(Grande g) {
    g.v0 = g.v0 * 2;
    g.v1 = g.v1 * 2;
    g.v2 = g.v2 * 2;
    g.v3 = g.v3 * 2;
    g.v4 = g.v4 * 2;
    g.v5 = g.v5 * 2;
    return g;
}

int total(Grande g) {
    return g.v0 + g.v1 + g.v2 + g.v3 + g.v4 + g.v5;
}

Rotulo renombrar(Rotulo r) {
    r.texto.append("-copia");
    r.numero = r.numero + 1;
    return r;
}

int main() {
    Par p;
    p.a = 1;
    p.b = 2;
    Par q = intercambiar(p);
    cout << "p = (" << p.a << ", " << p.b << ") q = (" << q.a << ", " << q.b << ")\n";

    Grande g;
    g.v0 = 1;
    g.v1 = 2;
    g.v2 = 3;
    g.v3 = 4;
    g.v4 = 5;
    g.v5 = 6;
    Grande h = duplicar(g);
    cout << "total g = " << total(g) << ", total h = " << total(h) << "\n";

    Rotulo r;
    r.texto = "etiqueta";
    r.numero = 1;
    Rotulo s = renombrar(r);
    cout << r.texto << " " << r.numero << "\n";
    cout << s.texto << " " << s.numero << "\n";
    return 0;
}
