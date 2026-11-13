// EXPECT-ERROR: no tiene un constructor que acepte
struct P { int a; P() : a(0) {} private: int b; };
struct Q { int a; private: int b; };
int main() { Q q = {1}; return 0; }
