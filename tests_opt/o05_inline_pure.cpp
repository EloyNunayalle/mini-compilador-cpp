// OPT-ASM-ABSENT: call f_add
#include <iostream>
using namespace std;
int add(int a, int b) { return a + b; }
int sq(int a) { return a * a; }
double half(double d) { return d / 2; }
bool pos(int a) { return a > 0; }
int main() {
    int x = 4, y = 6;
    int r = add(x, y) + add(1, 2) + sq(x) + sq(add(1, 1));
    cout << r << " " << half(5.0) << " " << pos(x) << pos(0 - 3) << endl;
    return 0;
}
