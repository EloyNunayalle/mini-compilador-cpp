#include <iostream>
#include <string>
using namespace std;
struct U {
    int v;
    U(int n) : v(n) {}
    U& operator=(const U& o) {
        cout << "assign " << v << " <- " << o.v << endl;
        v = o.v;
        return *this;
    }
};
struct Imp { int a; string s; Imp(int x, const char* t) : a(x), s(t) {} };
int main() {
    U a(1), b(2), c(3);
    a = b;
    a = b = c;
    a = a;
    cout << a.v << b.v << c.v << endl;
    Imp p(1, "uno"), q(2, "dos");
    p = q;
    q.s.append("!");
    cout << p.a << p.s.c_str() << q.a << q.s.c_str() << endl;
    return 0;
}
