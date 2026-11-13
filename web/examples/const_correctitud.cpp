#include <iostream>
using namespace std;

class Vector2 {
public:
    int x;
    int y;

    Vector2(int a, int b) : x(a), y(b) {
    }

    int norma1() const {
        return x + y;
    }

    int get_x() const {
        return x;
    }

    void mover(int dx, int dy) {
        x = x + dx;
        y = y + dy;
    }
};

int producto_punto(const Vector2& a, const Vector2& b) {
    return a.x * b.x + a.y * b.y;
}

int sumar(const int* v, int n) {
    int s = 0;
    for (int i = 0; i < n; i = i + 1) {
        s = s + v[i];
    }
    return s;
}

void poner_cero(int* const p) {
    *p = 0;
}

int main() {
    const int limite = 10;
    const Vector2 u(2, 3);
    Vector2 v(4, 5);
    cout << "limite = " << limite << "\n";
    cout << "norma1(u) = " << u.norma1() << "\n";
    cout << "u . v = " << producto_punto(u, v) << "\n";
    v.mover(1, 1);
    cout << "v.x = " << v.get_x() << "\n";

    int notas[4];
    for (int i = 0; i < 4; i = i + 1) {
        notas[i] = (i + 1) * 5;
    }
    cout << "suma = " << sumar(notas, 4) << "\n";
    poner_cero(&notas[1]);
    cout << "suma tras poner_cero = " << sumar(notas, 4) << "\n";
    return 0;
}
