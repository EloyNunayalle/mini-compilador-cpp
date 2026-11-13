#include <iostream>
using namespace std;
int main() {
    int a = 1, b = 2, c = 3, d = 4;
    int r = a + (b * (c + d));
    int q = (a + b) * (c + d);
    int w = a == (b + c * d);
    int e = d & (a | (b ^ c));
    int u = a - (b - (c - d));
    cout << r << q << w << e << u << endl;
    return 0;
}
