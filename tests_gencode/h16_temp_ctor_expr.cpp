#include <iostream>
using namespace std;
struct V {
    int x; int y;
    V(int a, int b) : x(a), y(b) { cout << "V" << x << y << endl; }
    V(const V& o) : x(o.x), y(o.y) { cout << "Vc" << endl; }
    ~V() { cout << "~V" << x << endl; }
    V sum(const V& o) const { return V(x + o.x, y + o.y); }
    int dot(V o) const { return x * o.x + y * o.y; }
};
int main() {
    V a(1, 2);
    V b = a.sum(V(3, 4));
    cout << b.x << b.y << endl;
    cout << a.dot(b) << endl;
    return 0;
}
