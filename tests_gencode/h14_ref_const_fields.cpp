#include <iostream>
using namespace std;
struct H {
    int& r;
    const int c;
    int plain;
    H(int& x, int k) : r(x), c(k), plain(0) {}
    void bump() { r = r + c; }
    int look() const { return r + c; }
};
int main() {
    int v = 10;
    H h(v, 5);
    h.bump();
    cout << v << h.look() << h.c << endl;
    H h2 = h;
    h2.bump();
    cout << v << endl;
    return 0;
}
