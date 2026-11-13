#include <iostream>
using namespace std;

void invertir(int* v, int n) {
    int i = 0;
    int j = n - 1;
    while (i < j) {
        int t = v[i];
        v[i] = v[j];
        v[j] = t;
        i = i + 1;
        j = j - 1;
    }
}

int suma(int* v, int n) {
    int s = 0;
    for (int i = 0; i < n; i = i + 1) {
        s = s + *(v + i);
    }
    return s;
}

void intercambiar(int* a, int* b) {
    int t = *a;
    *a = *b;
    *b = t;
}

int main() {
    int datos[6];
    for (int i = 0; i < 6; i = i + 1) {
        datos[i] = (i + 1) * 10;
    }
    cout << "suma = " << suma(datos, 6) << "\n";
    invertir(datos, 6);
    for (int i = 0; i < 6; i = i + 1) {
        cout << datos[i] << " ";
    }
    cout << "\n";

    int p = 3;
    int q = 9;
    intercambiar(&p, &q);
    cout << "p = " << p << ", q = " << q << "\n";

    int* ptr = &datos[2];
    *ptr = 99;
    cout << "datos[2] = " << datos[2] << "\n";
    return 0;
}
