#include <iostream>
using namespace std;
struct S1 { char a; };
struct S4 { int a; };
struct S8 { int a; int b; };
struct S12 { int a; int b; int c; };
struct S16 { int* p; int a; int b; };
struct F8 { float a; float b; };
struct D8 { double d; };
struct M16 { int a; double d; };
struct M16b { double d; int a; };
struct D16 { double a; double b; };
S1 r1(S1 s) { S1 t; t.a = s.a + 1; return t; }
S4 r4(S4 s) { S4 t; t.a = s.a * 2; return t; }
S8 r8(S8 s) { S8 t; t.a = s.b; t.b = s.a; return t; }
S12 r12(S12 s) { S12 t; t.a = s.c; t.b = s.b; t.c = s.a; return t; }
F8 rf(F8 s) { F8 t; t.a = s.a + s.b; t.b = s.a - s.b; return t; }
D8 rd(D8 s) { D8 t; t.d = s.d * 2; return t; }
M16 rm(M16 s) { M16 t; t.a = s.a + 1; t.d = s.d + 0.5; return t; }
M16b rmb(M16b s) { M16b t; t.a = s.a + 2; t.d = s.d + 1.5; return t; }
D16 rdd(D16 s) { D16 t; t.a = s.b; t.b = s.a; return t; }
int main() {
    S1 a; a.a = 'a';
    S4 b; b.a = 21;
    S8 c; c.a = 1; c.b = 2;
    S12 d; d.a = 1; d.b = 2; d.c = 3;
    F8 f; f.a = 1.5; f.b = 0.25;
    D8 g; g.d = 1.25;
    M16 m; m.a = 4; m.d = 2.0;
    M16b mb; mb.a = 5; mb.d = 3.0;
    D16 dd; dd.a = 1.5; dd.b = 2.5;
    S1 a2 = r1(a); S4 b2 = r4(b); S8 c2 = r8(c); S12 d2 = r12(d);
    F8 f2 = rf(f); D8 g2 = rd(g); M16 m2 = rm(m); M16b mb2 = rmb(mb); D16 dd2 = rdd(dd);
    cout << a2.a << " " << b2.a << " " << c2.a << c2.b << " " << d2.a << d2.b << d2.c << endl;
    cout << f2.a << " " << f2.b << " " << g2.d << " " << m2.a << " " << m2.d << " " << mb2.a << " " << mb2.d << " " << dd2.a << " " << dd2.b << endl;
    return 0;
}
