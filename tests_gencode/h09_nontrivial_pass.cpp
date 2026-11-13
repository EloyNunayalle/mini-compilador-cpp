#include <iostream>
using namespace std;
struct N {
    int v;
    N(int x) : v(x) { cout << "ctor " << v << endl; }
    N(const N& o) : v(o.v + 1) { cout << "copy " << v << endl; }
    ~N() { cout << "dtor " << v << endl; }
};
N id(N n) { cout << "in id " << n.v << endl; return n; }
int val(N n) { return n.v; }
N make(int x) { return N(x); }
int main() {
    N a(1);
    cout << "--1" << endl;
    N b = id(a);
    cout << "--2" << endl;
    cout << val(make(5)) << endl;
    cout << "--3" << endl;
    cout << val(a) + val(b) << endl;
    cout << "--4" << endl;
    return 0;
}
