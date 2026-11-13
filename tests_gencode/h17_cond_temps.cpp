#include <iostream>
using namespace std;
struct Q {
    int v;
    Q(int x) : v(x) { cout << "Q" << v << endl; }
    ~Q() { cout << "~Q" << v << endl; }
    bool ok() const { return v > 0; }
};
Q mk(int x) { return Q(x); }
int main() {
    if (mk(1).ok() && mk(0).ok()) cout << "a" << endl;
    if (mk(0).ok() && mk(2).ok()) cout << "b" << endl;
    int i = 0;
    while (mk(i).ok() == false) { i++; if (i > 1) break; }
    cout << i << endl;
    return 0;
}
