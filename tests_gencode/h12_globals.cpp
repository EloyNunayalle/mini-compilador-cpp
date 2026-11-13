#include <iostream>
#include <string>
using namespace std;
struct G {
    int v;
    G(int x) : v(x) { cout << "G+" << v << endl; }
    ~G() { cout << "G-" << v << endl; }
};
G g1(1);
G g2(2);
string gs = "global";
int counter = 5;
G g3 = G(3);
int main() {
    cout << "main " << g1.v << g2.v << g3.v << gs.c_str() << counter << endl;
    G l(4);
    return 0;
}
