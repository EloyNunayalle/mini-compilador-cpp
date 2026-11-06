#include <iostream>
using namespace std;

int suma(int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s = s + a[i];
    return s;
}

int sumaArr(int a[5]) {
    return a[0] + a[4];
}

void llenar(int *a, int n) {
    for (int i = 0; i < n; i++) a[i] = i * i;
}

int main() {
    int a[5];
    for (int i = 0; i < 5; i++) a[i] = i + 1;
    cout << suma(a, 5) << " " << sumaArr(a) << endl;

    int b[4] = {10, 20, 30};
    cout << b[0] << b[1] << b[2] << b[3] << endl;
    int z[3] = {};
    cout << z[0] << z[1] << z[2] << endl;

    int m[2][3];
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++)
            m[i][j] = i * 10 + j;
    cout << m[0][0] << m[0][2] << m[1][0] << m[1][2] << endl;

    int n[2][2] = {{1, 2}, {3, 4}};
    cout << n[0][0] + n[1][1] << n[0][1] * n[1][0] << endl;

    int cubo[2][2][2] = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
    cout << cubo[0][0][0] << cubo[0][1][1] << cubo[1][0][1] << cubo[1][1][1] << endl;

    double d[3] = {1.5, 2.5, 3.5};
    double tot = 0;
    for (int i = 0; i < 3; i++) tot = tot + d[i];
    cout << tot << endl;

    char cs[4];
    cs[0] = 'o'; cs[1] = 'k'; cs[2] = '!'; cs[3] = 0;
    cout << cs << endl;

    int c[8];
    llenar(c, 8);
    cout << c[7] << " " << suma(c, 8) << endl;

    int dim[2 + 1][2 * 2];
    dim[2][3] = 9;
    cout << dim[2][3] << endl;

    char *nombres[2] = {"uno", "dos"};
    cout << nombres[0] << nombres[1] << endl;

    int idx = 2;
    a[idx] = a[idx - 1] + a[idx + 1];
    cout << a[2] << endl;
    return 0;
}
