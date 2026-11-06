#include <iostream>
#include <string>
using namespace std;

int main() {
    int a;
    double d;
    char c;
    string s;
    cin >> a >> d >> c >> s;
    cout << a << " " << d << " " << c << " " << s << endl;
    int x, y;
    cin >> x;
    cin >> y;
    cout << x + y << endl;
    float f;
    cin >> f;
    cout << f * 2 << endl;
    cout << "tipos: " << 1 << ' ' << 'c' << ' ' << true << ' ' << 2.5 << ' ' << "texto" << " " << -3 << endl;
    float ff = 0.5;
    cout << ff << " " << 1.0 / 3 << endl;
    string t = "str";
    cout << t << endl << "fin" << endl;
    cout << endl;
    return 0;
}
