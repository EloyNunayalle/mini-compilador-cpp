#include <iostream>
#include <string>
using namespace std;
struct Persona {
    string nombre;
    int edad;
    Persona(const char* n, int e) : nombre(n), edad(e) {}
};
struct Equipo {
    Persona lider;
    string nombre;
    Equipo(const char* l, const char* n) : lider(l, 30), nombre(n) {}
};
string saludo(Persona p) { p.nombre.append("!"); return p.nombre; }
Persona mk(const char* n) { return Persona(n, 1); }
int main() {
    Persona a("ana", 20);
    Persona b = a;
    b.nombre.append("_copia");
    cout << a.nombre.c_str() << " " << b.nombre.c_str() << endl;
    Equipo e("luis", "rojo");
    Equipo f = e;
    f.lider.nombre.append("X");
    f.nombre.clear();
    cout << e.lider.nombre.c_str() << e.nombre.c_str() << "|" << f.lider.nombre.c_str() << f.nombre.c_str() << endl;
    string s = saludo(a);
    cout << s.c_str() << a.nombre.c_str() << endl;
    Persona c = mk("zoe");
    cout << c.nombre.c_str() << c.edad << endl;
    a = c;
    cout << a.nombre.c_str() << endl;
    string tmp = "abc";
    string tmp2 = tmp;
    tmp2.append("d");
    cout << tmp.c_str() << tmp2.c_str() << endl;
    return 0;
}
