#include <iostream>
#include <string>
using namespace std;

int main() {
    int n;
    cin >> n;
    int suma = 0;
    int mayor = 0;
    for (int i = 0; i < n; i = i + 1) {
        int v;
        cin >> v;
        suma = suma + v;
        if (i == 0) {
            mayor = v;
        }
        if (v > mayor) {
            mayor = v;
        }
    }
    string nombre;
    cin >> nombre;
    cout << "Hola, " << nombre << "\n";
    cout << "leidos " << n << " numeros, suma = " << suma << ", mayor = " << mayor << "\n";
    if (n > 0) {
        double promedio = suma;
        promedio = promedio / n;
        cout << "promedio = " << promedio << "\n";
    }
    return 0;
}
