#include <iostream>
#include <string>
using namespace std;

struct Persona {
    string nombre;
    int edad;

    void saludar() {
        cout << "Soy " << nombre << " y tengo " << edad << " anios\n";
    }
};

class Etiqueta {
public:
    string texto;

    Etiqueta(string t) : texto(t) {
    }

    void agregar(string s) {
        texto.append(s);
    }

    int largo() const {
        return texto.size();
    }
};

int main() {
    Persona a;
    a.nombre = "Ana";
    a.edad = 30;

    Persona b = a;
    b.nombre.append(" Maria");
    b.edad = 31;

    a.saludar();
    b.saludar();

    Etiqueta e("hola");
    Etiqueta f = e;
    f.agregar(" mundo");
    f.texto.push_back('!');
    cout << e.texto << " (" << e.largo() << ")\n";
    cout << f.texto << " (" << f.largo() << ")\n";

    Persona vacia;
    cout << "vacia: " << vacia.nombre.empty() << "\n";
    vacia.nombre = "temporal";
    vacia.nombre.clear();
    cout << "tras clear: " << vacia.nombre.size() << "\n";
    return 0;
}
