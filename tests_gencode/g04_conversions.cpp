#include <iostream>
using namespace std;

int toInt(double d) {
    return d;
}

double toDouble(int i) {
    return i;
}

char toChar(int i) {
    return i;
}

bool toBool(double d) {
    return d;
}

int main() {
    double d = 7.9;
    int i = d;
    cout << i << " " << toInt(-7.9) << " " << toInt(2.5) << endl;
    cout << toDouble(3) << " " << toDouble(-1) / 2 << endl;
    float f = 3;
    f = f / 2;
    cout << f << endl;
    cout << toChar(65) << toChar(321) << endl;
    cout << toBool(0.5) << toBool(0.0) << toBool(-1.0) << endl;
    int n = 5;
    double r = n / 2;
    double r2 = n / 2.0;
    cout << r << " " << r2 << endl;
    float ff = 0.1;
    double dd = ff;
    cout << (dd == 0.1) << endl;
    int fromChar = 'a';
    cout << fromChar << endl;
    bool b = 3.7;
    int bi = b;
    cout << bi << endl;
    double sum = 1 + 2.5 + 'a';
    cout << sum << endl;
    return 0;
}
