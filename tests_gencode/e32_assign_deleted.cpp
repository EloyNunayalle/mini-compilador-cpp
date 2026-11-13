// EXPECT-ERROR: asignación de copia
struct P { const int a; P(int x) : a(x) {} };
int main() { P p(1); P q(2); p = q; return 0; }
