// ASM-CONTAINS: addl %ecx, %eax
// ASM-CONTAINS: imull %ecx, %eax
// ASM-CONTAINS: movl $4, %eax
// ASM-ABSENT: movl $11, %eax
// ASM-ABSENT: movl $14, %eax
#include <iostream>
using namespace std;

int main() {
    int x = 4 + 7;
    int y = 2 * 3 + 8;
    cout << x << " " << y << endl;
    return 0;
}
