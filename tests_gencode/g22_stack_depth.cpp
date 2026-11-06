#include <iostream>
using namespace std;

double f(double a, double b) { return a * b; }

int g(int a, int b, int c, int d, int e, int f2, int g2, int h) {
    return a + b + c + d + e + f2 + g2 + h;
}

int main() {
    // temporales pendientes de distinta paridad al llamar
    int r = 1 + (2 * (3 + g(1, 2, 3, 4, 5, 6, 7, 8)));
    cout << r << endl;
    double d = 1.5 + f(2, f(3, 4)) * (2 + f(1, 1));
    cout << d << endl;
    cout << 1 + (2 + (3 + (4 + g(1, 1, 1, 1, 1, 1, 1, f(1, 1))))) << endl;
    return 0;
}
