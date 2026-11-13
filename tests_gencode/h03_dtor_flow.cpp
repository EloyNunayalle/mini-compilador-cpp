#include <iostream>
using namespace std;
struct T {
    int id;
    T(int i) : id(i) { cout << "+" << id << endl; }
    ~T() { cout << "-" << id << endl; }
};
int f(int n) {
    T a(10 * n);
    if (n > 1) {
        T b(10 * n + 1);
        return n;
    }
    T c(10 * n + 2);
    return 0;
}
int main() {
    T m(1);
    {
        T x(2);
        T y(3);
    }
    for (int i = 0; i < 3; i++) {
        T l(20 + i);
        if (i == 0) continue;
        if (i == 1) break;
    }
    int k = 0;
    while (k < 2) {
        T w(30 + k);
        k++;
    }
    cout << f(1) << f(2) << endl;
    return 0;
}
