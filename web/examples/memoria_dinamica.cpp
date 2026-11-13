#include <iostream>
using namespace std;

class Nodo {
public:
    int valor;

    Nodo(int v) : valor(v) {
        cout << "crea nodo " << valor << "\n";
    }

    Nodo() : valor(0) {
        cout << "crea nodo por defecto\n";
    }

    ~Nodo() {
        cout << "destruye nodo " << valor << "\n";
    }
};

int main() {
    Nodo* uno = new Nodo(42);
    cout << "uno->valor = " << uno->valor << "\n";
    delete uno;

    Nodo* varios = new Nodo[3];
    for (int i = 0; i < 3; i = i + 1) {
        varios[i].valor = (i + 1) * 11;
    }
    cout << "ultimo = " << varios[2].valor << "\n";
    delete[] varios;

    int* enteros = new int[5];
    for (int i = 0; i < 5; i = i + 1) {
        enteros[i] = i * i;
    }
    int s = 0;
    for (int i = 0; i < 5; i = i + 1) {
        s = s + enteros[i];
    }
    cout << "suma de cuadrados = " << s << "\n";
    delete[] enteros;
    return 0;
}
