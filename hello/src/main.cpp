// Подключим библиотеку Boost.Optional, чтобы убедиться, что Boost подключен успешно
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>
#include <iostream>

int main() {
    // Шаблон boost::optional — прообраз std::optional, используем его здесь для примера
    boost::optional<int> opt;
    std::cout << opt << std::endl;
}

