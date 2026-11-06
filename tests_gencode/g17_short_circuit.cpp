#include <iostream>
using namespace std;

int calls = 0;

bool t() {
    calls = calls + 1;
    return true;
}

bool f() {
    calls = calls + 10;
    return false;
}

int main() {
    if (f() && t()) cout << "no" << endl;
    cout << calls << endl;
    calls = 0;
    if (t() || f()) cout << "si" << endl;
    cout << calls << endl;
    calls = 0;
    bool r = t() && t() && f() && t();
    cout << r << " " << calls << endl;
    calls = 0;
    bool s = f() || f() || t() || f();
    cout << s << " " << calls << endl;
    int *p = nullptr;
    if (p && *p > 3) cout << "x" << endl; else cout << "seguro" << endl;
    int a = 5;
    cout << (a > 1 && a < 10) << (a < 1 || a > 10) << (!(a == 5) || a == 5) << endl;
    return 0;
}
