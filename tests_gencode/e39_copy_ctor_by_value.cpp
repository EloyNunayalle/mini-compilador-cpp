// EXPECT-ERROR: debe recibir una referencia
struct P { int a; P(P o) : a(o.a) {} };
int main() { return 0; }
