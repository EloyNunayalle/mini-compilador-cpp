#include <iostream>
using namespace std;

struct P {
    int x;
    int y;
};

int g = 5;
int h = g * 2 + 1;
double pi = 3.5;
int tabla[4] = {1, 2, 3, 4};
P origen = {1, 2};
int contador;
char letra = 'k';
bool activo = true;
int *pg = &g;

void inc() {
    contador++;
    g = g + 1;
}

int sumaTabla() {
    int s = 0;
    for (int i = 0; i < 4; i++) s = s + tabla[i];
    return s;
}

int main() {
    cout << g << " " << h << " " << pi << " " << letra << activo << endl;
    cout << tabla[2] << origen.x << origen.y << contador << endl;
    inc();
    inc();
    cout << g << " " << contador << " " << *pg << endl;
    tabla[0] = 10;
    origen.y = 20;
    cout << sumaTabla() << " " << origen.y << endl;
    int g = 100;
    cout << g << endl;
    return 0;
}
