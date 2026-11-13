// OPT-ASM-CONTAINS: movl $14, %eax
// OPT-ASM-CONTAINS: movl $16, %eax
// OPT-ASM-CONTAINS: movl $10, %eax
#include <iostream>
using namespace std;
int main() {
    int a = 2 + 3 * 4;
    int b = (10 - 2) * (1 + 1);
    int c = 100 / 5 / 2;
    bool d = 3 < 4 && 2 == 2;
    int e = -(5 + 1) + ~0 + (7 & 3) + (1 << 4) + (64 >> 2) + (6 ^ 3) + (6 | 1) + 17 % 5;
    char ch = 'a' + 1;
    cout << a << b << c << d << e << ch << endl;
    return 0;
}
