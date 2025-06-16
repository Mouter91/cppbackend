#define BOOST_TEST_MODULE urlencode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
  using namespace std::literals;

  BOOST_TEST(UrlDecode(""sv) == ""s);
  // Напишите остальные тесты для функции UrlDecode самостоятельно
  // 2. Строки без %-последовательностей
  BOOST_TEST(UrlDecode("Hello"sv) == "Hello"s);
  BOOST_TEST(UrlDecode("Hello World"sv) == "Hello World"s);
  BOOST_TEST(UrlDecode("!@#$&'()*+,/:;=?@[]"sv) == "!@#$&'()*+,/:;=?@[]"s);

  // 3. Валидные %-последовательности (разный регистр)
  BOOST_TEST(UrlDecode("Hello%20World"sv) == "Hello World"s);
  BOOST_TEST(UrlDecode("%41%42%43"sv) == "ABC"s);
  BOOST_TEST(UrlDecode("%4a%4B%4c"sv) == "JKL"s);
  BOOST_TEST(UrlDecode("%3C%3E"sv) == "<>"s);

  // 4. Символ + (кодирование пробела)
  BOOST_TEST(UrlDecode("Hello+World"sv) == "Hello World"s);
  BOOST_TEST(UrlDecode("+++"sv) == "   "s);

  // 5. Невалидные %-последовательности
  BOOST_CHECK_THROW(UrlDecode("%G1"sv), std::invalid_argument);
  BOOST_CHECK_THROW(UrlDecode("%1G"sv), std::invalid_argument);
  BOOST_CHECK_THROW(UrlDecode("%ZZ"sv), std::invalid_argument);

  // 6. Неполные %-последовательности
  BOOST_CHECK_THROW(UrlDecode("%"sv), std::invalid_argument);
  BOOST_CHECK_THROW(UrlDecode("%1"sv), std::invalid_argument);
  BOOST_CHECK_THROW(UrlDecode("test%"sv), std::invalid_argument);

  // 7. Специальные символы и Unicode
  BOOST_TEST(UrlDecode("%7E"sv) == "~"s);
  BOOST_TEST(UrlDecode("%5C%22"sv) == "\\\""s);
  BOOST_TEST(UrlDecode("%D0%9F%D1%80%D0%B8%D0%B2%D0%B5%D1%82"sv) == "Привет"s);
}
