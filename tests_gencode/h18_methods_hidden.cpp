#include <iostream>
#include <string>
using namespace std;
struct W {
    string name;
    int n;
    W(const char* s, int k) : name(s), n(k) {}
    W twin() const { W t(name.c_str(), n + 1); return t; }
    string label() const { string r = name; r.append("#"); return r; }
    int total(int a, int b, int c, int d, int e) const { return n + a + b + c + d + e; }
};
int main() {
    W w("w", 1);
    W t = w.twin();
    cout << t.name.c_str() << t.n << w.label().c_str() << w.total(1, 2, 3, 4, 5) << endl;
    W* p = &w;
    cout << p->twin().n << p->label().size() << endl;
    return 0;
}
