#include <iostream>
using namespace std;

bool esPar(int n) {
    return n % 2 == 0;
}

int main() {
    char c = 'A';
    char d = c + 2;
    cout << c << d << endl;
    cout << c + 1 << endl;
    char e = 'z';
    e = e - 25;
    cout << e << endl;
    bool t = true;
    bool f = false;
    cout << t << f << (t && f) << (t || f) << !t << endl;
    bool g = 5;
    bool h = 0;
    cout << g << h << endl;
    cout << esPar(4) << esPar(7) << endl;
    char big = 200;
    int asInt = big;
    cout << asInt << endl;
    char n = '\n';
    cout << "line" << n << "next" << endl;
    cout << 'a' << 'b' << 'c' << endl;
    char cnt = 'a';
    cnt++;
    ++cnt;
    cout << cnt << endl;
    return 0;
}
