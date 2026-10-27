#include <iostream>
using namespace std;

int main() {
    int x = 42;
    int* ptr = &x;
    *ptr = 100;

    int arr[5];
    arr[0] = 10;
    arr[1] = 20;
    int val = arr[0] + arr[1];

    int* p = new int;
    *p = 50;
    delete p;

    return *ptr;
}
