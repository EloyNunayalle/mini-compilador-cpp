// EXPECT-ERROR: No se puede modificar un objeto const
int main() { const int a = 1; a++; return 0; }
