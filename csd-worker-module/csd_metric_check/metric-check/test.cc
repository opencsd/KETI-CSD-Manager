#include <iostream>
#include <iomanip>

int main() {
    int totalByte = 1622751;
    double result = static_cast<double>(totalByte) / 1048576.0;

    std::cout << std::fixed << std::setprecision(6) << result << std::endl;

    return 0;
}
