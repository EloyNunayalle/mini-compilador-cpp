#include <iostream>
using namespace std;
int calls = 0;
int f() { calls++; return 3; }
int main() {
    int x = 5;
    int a = x + 0;
    int b = 0 + x;
    int c = x * 1;
    int d = 1 * x;
    int e = x / 1;
    int g = x * 2;
    int h = 2 * x;
    int i = x - 0;
    int j = x * 0;
    int k = f() * 0;
    int l = f() + 0;
    int m = f() * 1;
    bool n = false && f() == 3;
    bool o = true || f() == 3;
    cout << a << b << c << d << e << g << h << i << j << k << l << m << n << o << " " << calls << endl;
    return 0;
}
