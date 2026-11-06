#include <iostream>
#include <string>
#include <cmath>

int main() {
    std::string s = "hola";
    s.append(" std");
    std::cout << s << " " << std::sqrt(9.0) << std::endl;
    int v;
    std::cin >> v;
    std::cout << v + 1 << std::endl;
    return 0;
}
