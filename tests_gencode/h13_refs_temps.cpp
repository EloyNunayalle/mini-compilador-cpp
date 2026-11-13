#include <iostream>
#include <string>
using namespace std;
struct R {
    int v;
    R(int x) : v(x) { cout << "R" << v << endl; }
    R(const R& o) : v(o.v) { cout << "Rcopy" << v << endl; }
    ~R() { cout << "~R" << v << endl; }
};
R dame(int x) { return R(x); }
int usa(const R& r) { return r.v; }
void mod(R& r) { r.v = r.v + 1; }
int doble(const int& x) { return x * 2; }
int largo(const string& s) { return s.size(); }
int main() {
    R a(1);
    cout << usa(a) << usa(dame(2)) << endl;
    mod(a);
    cout << a.v << endl;
    const R& ref = dame(3);
    cout << ref.v << endl;
    R& r2 = a;
    r2.v = 10;
    cout << a.v << doble(21) << doble(a.v) << largo("hola") << endl;
    return 0;
}
