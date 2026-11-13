// EXPECT-ERROR: se perdería const
void f(int& a) { a = 1; }
int main() { const int x = 1; f(x); return 0; }
