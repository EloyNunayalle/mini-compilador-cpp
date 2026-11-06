#include <iostream>
using namespace std;

int main() {
    int a = 10;
    int b = 20;
    int c = a + b * 2 - (30 / 5) % 3;
    cout << c << endl;
    cout << -7 / 2 << " " << -7 % 3 << " " << 7 % -3 << endl;
    int s = 1 << 10;
    cout << s << " " << (s >> 3) << " " << (-16 >> 2) << endl;
    cout << (a & b) << " " << (a | b) << " " << (a ^ b) << " " << ~a << endl;
    int x = 0;
    int y = 0;
    x = y = 42;
    cout << x << " " << y << endl;
    cout << (a < b) << (a > b) << (a <= 10) << (a >= 11) << (a == 10) << (a != 10) << endl;
    cout << -a << " " << +a << " " << !a << " " << !0 << endl;
    return c;
}
