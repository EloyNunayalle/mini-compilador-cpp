#include <iostream>
using namespace std;

struct C {
    int n;
    double d;
};

int main() {
    int i = 5;
    cout << i++ << endl;
    cout << i << endl;
    cout << ++i << endl;
    cout << i-- << endl;
    cout << i << endl;
    cout << --i << endl;
    char c = 'a';
    cout << c++ << endl;
    cout << c << endl;
    cout << ++c << endl;
    double d = 1.5;
    cout << d++ << endl;
    cout << d << endl;
    cout << ++d << endl;
    cout << --d << endl;
    float f = 2.5;
    cout << f++ << endl;
    cout << f << endl;
    int arr[3] = {10, 20, 30};
    arr[1]++;
    ++arr[2];
    arr[0]--;
    cout << arr[0] << arr[1] << arr[2] << endl;
    int *p = arr;
    cout << *p++ << endl;
    cout << *p << endl;
    cout << *++p << endl;
    p--;
    cout << *p << endl;
    C s;
    s.n = 1;
    s.d = 0.5;
    s.n++;
    ++s.n;
    s.d++;
    cout << s.n << " " << s.d << endl;
    C *ps = &s;
    ps->n--;
    cout << ps->n << endl;
    int j = 0;
    j++;
    j++;
    cout << j << endl;
    return 0;
}
