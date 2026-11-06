#include <iostream>
using namespace std;

double mezcla(int a, double b, int c, double d, char e, float f, bool g, int h) {
    return a + b * 10 + c * 100 + d * 1000 + e + f + g + h * 7;
}

double nueve(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j) {
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 + i * 9 + j * 10;
}

int ocho(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a * 1 + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}

double enteros_y_dobles(int a, int b, int c, int d, int e, int f, int g, int h,
                        double x1, double x2, double x3, double x4, double x5, double x6, double x7, double x8,
                        double x9, double y) {
    return a + b + c + d + e + f + g + h + x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 * 100 + y * 1000;
}

char siguiente(char c) { return c + 1; }
bool mayor(double a, double b) { return a > b; }
float prom(float a, float b) { return (a + b) / 2; }

int main() {
    cout << mezcla(1, 2.5, 3, 0.5, 'a', 1.25, true, 2) << endl;
    cout << nueve(1, 2, 3, 4, 5, 6, 7, 8, 9, 10) << endl;
    cout << ocho(1, 2, 3, 4, 5, 6, 7, 8) << endl;
    cout << enteros_y_dobles(1, 2, 3, 4, 5, 6, 7, 8, 0.5, 0.25, 1, 1, 1, 1, 1, 1, 2, 3) << endl;
    cout << siguiente('a') << " " << mayor(2.5, 1) << mayor(1, 2.5) << " " << prom(3, 4) << endl;
    cout << nueve(ocho(1, 1, 1, 1, 1, 1, 1, 1), 0, 0, 0, 0, 0, 0, 0, 0, prom(2, 4)) << endl;
    return 0;
}
