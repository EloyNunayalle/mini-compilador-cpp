// EXPECT-ERROR: No se puede asignar a un objeto const
struct C { int v; int get() const { v = 1; return v; } };
int main() { return 0; }
