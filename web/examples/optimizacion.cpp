#include <iostream>
using namespace std;

int cuadrado(int x) {
    return x * x;
}

int main() {
    int segundos_por_dia = 24 * 60 * 60;
    int area = cuadrado(7) + cuadrado(3);
    int a = 2;
    int b = 3;
    int c = 4;
    int d = 5;
    int expr = (a + b) * (c + d) - (a * b + c * d);
    cout << "segundos por dia = " << segundos_por_dia << "\n";
    cout << "area = " << area << "\n";
    cout << "expr = " << expr << "\n";
    cout << "constante = " << (100 - 4 * 5) / 2 << "\n";
    return 0;
}
