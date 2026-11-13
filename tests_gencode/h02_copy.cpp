#include <iostream>
#include <string>
using namespace std;
struct A {
    int v;
    A(int n) : v(n) { cout << "A(" << v << ")" << endl; }
    A(const A& o) : v(o.v + 100) { cout << "copy A " << v << endl; }
    ~A() { cout << "~A " << v << endl; }
};
struct B {
    A a;
    int k;
    B(int n) : a(n), k(n * 2) {}
};
void porValor(A a) { cout << "porValor " << a.v << endl; }
void porRef(const A& a) { cout << "porRef " << a.v << endl; }
A crear(int n) { A t(n); return t; }
int main() {
    A a(1);
    A b = a;
    A c(b);
    porValor(a);
    porRef(a);
    B x(5);
    B y = x;
    cout << y.a.v << " " << y.k << endl;
    A d = crear(7);
    cout << d.v << endl;
    return 0;
}
