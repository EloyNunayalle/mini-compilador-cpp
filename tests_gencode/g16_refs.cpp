#include <iostream>
#include <string>
using namespace std;

struct S {
    int v;
};

void swap2(int &a, int &b) {
    int t = a;
    a = b;
    b = t;
}

void incr(int &x) {
    x++;
}

int &mayor(int &a, int &b) {
    if (a > b) return a;
    return b;
}

void setv(S &s) {
    s.v = 99;
}

double &primero(double *p) {
    return p[0];
}

void duplicar(string &s) {
    s.append(s);
}

int main() {
    int x = 1, y = 2;
    swap2(x, y);
    cout << x << y << endl;
    int &r = x;
    r = 50;
    cout << x << " " << r << endl;
    incr(y);
    incr(r);
    cout << x << " " << y << endl;
    mayor(x, y) = 7;
    cout << x << " " << y << endl;
    int arr[3] = {1, 2, 3};
    int &e = arr[1];
    e = 5;
    cout << arr[0] << arr[1] << arr[2] << endl;
    S s;
    setv(s);
    cout << s.v << endl;
    double ds[2] = {1.5, 2.5};
    primero(ds) = 9.5;
    cout << ds[0] << " " << primero(ds) << endl;
    string t = "ab";
    duplicar(t);
    cout << t << endl;
    double dd = 2.5;
    double &rd = dd;
    rd = rd * 2;
    cout << dd << endl;
    return 0;
}
