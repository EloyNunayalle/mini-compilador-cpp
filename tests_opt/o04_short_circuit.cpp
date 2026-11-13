#include <iostream>
using namespace std;
int n = 0;
bool t() { n++; cout << "t"; return true; }
bool f() { n++; cout << "f"; return false; }
int main() {
    bool a = true && t();
    bool b = false && t();
    bool c = true || t();
    bool d = false || f();
    bool e = f() && t();
    bool g = t() || f();
    cout << endl << a << b << c << d << e << g << " " << n << endl;
    return 0;
}
