// EXPECT-ERROR: es privado
class P { P() {} public: int a; };
int main() { P p; return 0; }
