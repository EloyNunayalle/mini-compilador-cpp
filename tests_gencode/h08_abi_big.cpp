#include <iostream>
using namespace std;
struct Big { int a[10]; double d; };
struct Mid { double a; double b; double c; };
struct P { int a; int b; };
Big mkBig(int s) { Big b; for (int i = 0; i < 10; i++) b.a[i] = s + i; b.d = s * 0.5; return b; }
int sumBig(Big b) { int s = 0; for (int i = 0; i < 10; i++) s = s + b.a[i]; return s; }
double sumMid(Mid m) { return m.a + m.b + m.c; }
int muchos(int a, int b, int c, int d, int e, int f, int g, int h, P p, P q) {
    return a + b + c + d + e + f + g + h + p.a + p.b + q.a + q.b;
}
double flot(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j) {
    return a + b + c + d + e + f + g + h + i + j;
}
int mixto(P p1, P p2, P p3, P p4, P p5, P p6, P p7, int z) { return p1.a + p2.b + p3.a + p4.b + p5.a + p6.b + p7.a + z; }
struct Obj {
    int base;
    Big mk() const { return mkBig(base); }
    P par(P p) const { P r; r.a = p.a + base; r.b = p.b + base; return r; }
};
int main() {
    Big b = mkBig(1);
    cout << sumBig(b) << " " << b.d << endl;
    Mid m; m.a = 1.5; m.b = 2; m.c = 3;
    cout << sumMid(m) << endl;
    P p; p.a = 1; p.b = 2;
    P q; q.a = 3; q.b = 4;
    cout << muchos(1, 2, 3, 4, 5, 6, 7, 8, p, q) << endl;
    cout << flot(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) << endl;
    cout << mixto(p, q, p, q, p, q, p, 100) << endl;
    Obj o; o.base = 10;
    Big bb = o.mk();
    P pr = o.par(p);
    cout << sumBig(bb) << " " << pr.a << pr.b << endl;
    cout << sumBig(o.mk()) << endl;
    return 0;
}
