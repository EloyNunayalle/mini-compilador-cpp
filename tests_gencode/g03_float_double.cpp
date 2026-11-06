#include <iostream>
#include <cmath>
using namespace std;

double cuadrado(double x) {
    return x * x;
}

float mitad(float x) {
    return x / 2;
}

int main() {
    double a = 3.5;
    double b = 2.0;
    cout << a + b << " " << a - b << " " << a * b << " " << a / b << endl;
    float f = 1.5;
    float g = 0.25;
    cout << f + g << " " << f * g << " " << f - g << " " << f / g << endl;
    cout << (a > b) << (a < b) << (a >= 3.5) << (a <= 3.4) << (a == 3.5) << (a != 3.5) << endl;
    cout << (f > g) << (f < g) << (f == 1.5) << (f != 1.5) << endl;
    cout << cuadrado(1.5) << " " << mitad(5) << endl;
    cout << sqrt(16.0) << " " << sqrt(2) << endl;
    double neg = -a;
    float nf = -f;
    cout << neg << " " << nf << endl;
    cout << 100000000.0 << " " << 0.000123 << " " << 123456789.0 << endl;
    double z = 0.0;
    cout << (z == 0) << endl;
    double acc = 0;
    int i;
    for (i = 0; i < 10; i++) {
        acc = acc + 0.1;
    }
    cout << acc << endl;
    double d = 5;
    d++;
    ++d;
    d--;
    cout << d << endl;
    float ff = 1;
    ff++;
    cout << ff << endl;
    return 0;
}
