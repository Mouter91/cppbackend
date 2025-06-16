#include "urlencode.h"
#include <cctype>
#include <iomanip>
#include <sstream>

std::string UrlEncode(std::string_view str) {
  std::ostringstream encoded;
  encoded.fill('0'); // Заполнитель для hex-значений
  encoded << std::hex
          << std::uppercase; // Шестнадцатеричный вывод в верхнем регистре

  for (unsigned char c : str) {
    if (c == ' ') {
      // Кодируем пробел как '+'
      encoded << '+';
    } else if (c < 32 || c >= 127 || // Символы с кодами <32 и >=127
               c == '!' || c == '#' || c == '$' || c == '&' || c == '\'' ||
               c == '(' || c == ')' || c == '*' || c == '+' || c == ',' ||
               c == '/' || c == ':' || c == ';' || c == '=' || c == '?' ||
               c == '@' || c == '[' || c == ']') {
      // Кодируем специальные символы в %XX формате
      encoded << '%' << std::setw(2) << static_cast<int>(c);
    } else {
      // Оставляем обычные символы без изменений
      encoded << c;
    }
  }

  return encoded.str();
}
