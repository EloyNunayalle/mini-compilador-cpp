#include <iostream>
using namespace std;
int main() {
    int z = 0;
    int r = 7;
    if (z != 0) r = 10 / z;
    if (z != 0) r = 1 / 0;
    int big = 2147483647;
    int w = big + 1;
    int lit = 2147483647 + 1;
    int neg = -2147483647 - 1;
    cout << r << " " << (w < 0) << " " << (lit < 0) << " " << neg << endl;
    double dd = 0.1 + 0.2;
    cout << dd << " " << (dd == 0.3) << endl;
    return 0;
}
