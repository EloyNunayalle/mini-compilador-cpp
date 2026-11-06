#include <iostream>
using namespace std;

struct Nodo {
    int dato;
    Nodo *sig;
};

void intercambiar(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

int main() {
    int x = 42;
    int *p = &x;
    *p = 100;
    cout << x << " " << *p << endl;
    int **pp = &p;
    **pp = 7;
    cout << x << endl;

    int a = 1, b = 2;
    intercambiar(&a, &b);
    cout << a << b << endl;

    int arr[5] = {10, 20, 30, 40, 50};
    int *q = arr;
    cout << *q << *(q + 2) << q[4] << endl;
    q++;
    cout << *q << endl;
    q = q + 2;
    cout << *q << " " << (q - arr) << endl;
    q--;
    --q;
    cout << *q << endl;
    int *r = &arr[4];
    cout << (r - q) << " " << (r > q) << (q > r) << (q == r) << (q != r) << endl;
    cout << *(1 + q) << endl;

    int *n = new int;
    *n = 55;
    cout << *n << endl;
    delete n;

    int *vec = new int[6];
    for (int i = 0; i < 6; i++) vec[i] = i * 3;
    int s = 0;
    for (int i = 0; i < 6; i++) s = s + vec[i];
    cout << s << endl;
    delete[] vec;

    double *dv = new double[3];
    dv[0] = 1.5; dv[1] = 2.5; dv[2] = 3.5;
    cout << dv[0] + dv[1] + dv[2] << endl;
    delete[] dv;

    Nodo *head = nullptr;
    for (int i = 1; i <= 4; i++) {
        Nodo *nuevo = new Nodo;
        nuevo->dato = i;
        nuevo->sig = head;
        head = nuevo;
    }
    Nodo *cur = head;
    while (cur != nullptr) {
        cout << cur->dato;
        cur = cur->sig;
    }
    cout << endl;
    while (head) {
        Nodo *t = head->sig;
        delete head;
        head = t;
    }
    int *nulo = nullptr;
    cout << (nulo == nullptr) << (nulo != 0) << endl;
    char *cp = "hola";
    cout << cp << " " << *cp << endl;
    return 0;
}
