#include <iostream>
#include <string>
using namespace std;

struct Persona {
    string nombre;
    int edad;
};

string global = "global";

void mostrar(string &s) {
    cout << s << "(" << s.size() << ")" << endl;
}

void agregar(string *s) {
    s->append("!");
}

int main() {
    string a;
    cout << a.empty() << a.size() << endl;
    a = "hola";
    string b = a;
    b.append(" mundo");
    cout << a << "|" << b << endl;
    mostrar(a);
    mostrar(b);
    agregar(&a);
    cout << a << endl;
    a.push_back('?');
    cout << a << endl;
    a.clear();
    cout << a.size() << a.empty() << endl;

    string c = "abc";
    cout << c[0] << c[2] << endl;
    c[1] = 'X';
    cout << c << endl;
    cout << (c == "aXc") << (c != "aXc") << (b == a) << ("aXc" == c) << endl;
    string d = c;
    cout << (d == c) << endl;
    b = c;
    cout << b << endl;
    b.append(c);
    b.append(b);
    cout << b << endl;

    Persona p;
    p.nombre = "Ana";
    p.edad = 30;
    cout << p.nombre << p.edad << endl;
    Persona q;
    q.nombre = p.nombre;
    q.nombre.append("_q");
    cout << q.nombre << " " << p.nombre << endl;
    Persona *pp = &p;
    pp->nombre.append("!!");
    cout << pp->nombre << endl;

    cout << global << endl;
    global.append("+");
    cout << global << endl;

    for (int i = 0; i < 3; i++) {
        string t = "x";
        t.append("y");
        cout << t;
    }
    cout << endl;

    cout << c.c_str() << endl;
    string big;
    for (int i = 0; i < 100; i++) big.push_back('a');
    cout << big.size() << endl;
    string vacio = "";
    cout << vacio.empty() << endl;
    return 0;
}
