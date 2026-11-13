#include <iostream>
#include <string>
using namespace std;
struct E {
    int v;
    E() : v(9) { cout << "E()" << endl; }
    E(int x) : v(x) { cout << "E(" << v << ")" << endl; }
    E(const E& o) : v(o.v) { cout << "copy" << endl; }
    ~E() { cout << "~E" << v << endl; }
};
struct Pr { int a = 4; int b = 5; };
int main() {
    E arr[3];
    arr[1].v = 1;
    E pares[2] = {E(7), E(8)};
    E parcial[3] = {E(1)};
    string ss[2];
    ss[0] = "a";
    ss[1].append("bc");
    cout << ss[0].c_str() << ss[1].c_str() << endl;
    Pr ps[2];
    Pr q = {};
    Pr w = {1};
    cout << q.a << q.b << w.a << w.b << endl;
    cout << arr[0].v << arr[1].v << pares[1].v << parcial[2].v << endl;
    return 0;
}
