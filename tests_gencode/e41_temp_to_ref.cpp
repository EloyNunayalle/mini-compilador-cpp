// EXPECT-ERROR: solo una referencia a const admite temporales
struct P { int a; };
void f(P& p) {}
P mk() { P p; return p; }
int main() { f(mk()); return 0; }
