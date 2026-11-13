// OPT-ASM-CONTAINS: call f_dup
// OPT-ASM-CONTAINS: call f_fact
// OPT-ASM-CONTAINS: call f_bump
#include <iostream>
using namespace std;
int g = 10;
int dup(int x) { return x + x; }
int rec(int n) { if (n <= 0) return 0; return rec(n - 1) + 1; }
int fact(int n) { if (n <= 1) return 1; return n * fact(n - 1); }
int bump(int x) { g = g + x; return g; }
int useG(int x) { return x + g; }
int unused(int a, int b) { return a; }
int main() {
    int a = 1;
    int r1 = dup(++a);
    int r2 = fact(5) + rec(3);
    int r3 = bump(2) + bump(3);
    int r4 = useG(5);
    int r5 = unused(1, ++a);
    cout << r1 << " " << r2 << " " << r3 << " " << r4 << " " << r5 << " " << a << " " << g << endl;
    return 0;
}
