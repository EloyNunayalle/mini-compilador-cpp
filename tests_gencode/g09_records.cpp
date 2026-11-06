#include <iostream>
using namespace std;

struct Mixto {
    char c;
    int i;
    char d;
    double x;
    bool b;
};

struct Punto {
    int x;
    int y;
};

struct Linea {
    Punto a;
    Punto b;
    char etiqueta;
};

struct Rec {
    Punto pts[3];
    int n;
};

int main() {
    Mixto m;
    m.c = 'q';
    m.i = 1234;
    m.d = 'w';
    m.x = 2.5;
    m.b = true;
    cout << m.c << " " << m.i << " " << m.d << " " << m.x << " " << m.b << endl;

    Punto p = {3, 4};
    Punto q;
    q = p;
    q.x = 10;
    cout << p.x << " " << p.y << " " << q.x << " " << q.y << endl;

    Linea l;
    l.a.x = 1; l.a.y = 2;
    l.b.x = 3; l.b.y = 4;
    l.etiqueta = 'L';
    cout << l.a.x + l.b.y << l.etiqueta << endl;

    Linea l2 = l;
    l2.b.y = 99;
    cout << l.b.y << " " << l2.b.y << endl;

    Rec r;
    for (int i = 0; i < 3; i++) {
        r.pts[i].x = i;
        r.pts[i].y = i * i;
    }
    r.n = 3;
    int s = 0;
    for (int i = 0; i < r.n; i++) s = s + r.pts[i].x + r.pts[i].y;
    cout << s << endl;

    Punto *pp = &p;
    pp->x = 7;
    (*pp).y = 8;
    cout << p.x << p.y << endl;

    Punto arr[2] = {{1, 2}, {3, 4}};
    cout << arr[0].x << arr[0].y << arr[1].x << arr[1].y << endl;

    Punto *dyn = new Punto;
    dyn->x = 5;
    dyn->y = 6;
    cout << dyn->x * dyn->y << endl;
    delete dyn;

    Mixto zero = {};
    cout << zero.i << zero.x << zero.b << endl;
    return 0;
}
