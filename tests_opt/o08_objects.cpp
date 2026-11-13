#include <iostream>
#include <string>
using namespace std;
struct P {
    int v;
    P(int x) : v(x + 0) { cout << "P" << v << endl; }
    P(const P& o) : v(o.v * 1) { cout << "copy" << endl; }
    ~P() { cout << "~P" << v << endl; }
    int twice() const { return v * 2; }
};
int addp(const P& a, int k) { return a.v + k + 0; }
P mk(int x) { return P(x * 1); }
int main() {
    P a(2 + 3);
    P b = a;
    const int k = 4 * 1;
    string s = "x";
    s.append("y");
    cout << addp(b, k + 0) << mk(7).twice() << s.c_str() << endl;
    return 0;
}
