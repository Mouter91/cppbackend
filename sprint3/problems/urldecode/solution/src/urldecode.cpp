#include "urldecode.h"

#include <charconv>
#include <stdexcept>

std::string UrlDecode(std::string_view str) {
  std::string result;
  result.reserve(str.size());

  for (size_t i = 0; i < str.size(); ++i) {
    const char ch = str[i];

    if (ch == '+') {
      result += ' ';
    } else if (ch == '%') {
      if (i + 2 >= str.size()) {
        throw std::invalid_argument("Incomplete percent encoding");
      }

      const char hex1 = str[i + 1];
      const char hex2 = str[i + 2];

      if (!std::isxdigit(hex1) || !std::isxdigit(hex2)) {
        throw std::invalid_argument("Invalid percent encoding");
      }

      unsigned char byte;
      auto res = std::from_chars(&hex1, &hex1 + 1, byte, 16);
      byte *= 16;
      res = std::from_chars(&hex2, &hex2 + 1, byte, 16);

      result += static_cast<char>(byte);
      i += 2;
    } else {
      // Не кодированные символы (включая зарезервированные)
      result += ch;
    }
  }

  return result;
}
