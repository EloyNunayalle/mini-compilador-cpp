#include <iostream>
#include <string>
using namespace std;

struct Etiqueta {
    string texto;
    int id;
};

int buscar(string &s, char c) {
    string copia = s;
    for (int i = 0; i < copia.size(); i++) {
        string tmp = copia;
        tmp.push_back('x');
        if (copia[i] == c) return i;
    }
    return -1;
}

double medida(string &s) {
    string t = s;
    t.append("123");
    return t.size() * 1.5;
}

char primera(string *s) {
    string local = "?";
    if (s->empty()) return local[0];
    return (*s)[0];
}

int contar(int n) {
    Etiqueta e;
    e.texto = "e";
    int total = 0;
    for (int i = 0; i < n; i++) {
        string t = "ab";
        e.texto.append(t);
        if (i == 3) break;
        total = total + e.texto.size();
    }
    return total;
}

int main() {
    string s = "hola mundo";
    cout << buscar(s, 'm') << buscar(s, 'z') << endl;
    cout << medida(s) << endl;
    cout << primera(&s) << endl;
    string vacio;
    cout << primera(&vacio) << endl;
    cout << contar(10) << endl;
    for (int k = 0; k < 3; k++) {
        string loop = "k";
        loop.append(loop);
        cout << loop;
    }
    cout << endl;
    return 0;
}
