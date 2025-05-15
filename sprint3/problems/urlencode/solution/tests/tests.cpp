#include <gtest/gtest.h>

#include "../src/urlencode.h"

using namespace std::literals;

TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
  EXPECT_EQ(UrlEncode("hello"sv), "hello"s);
}

TEST(UrlEncodeTestSuite, NoEncodingNeeded) {
  EXPECT_EQ(UrlEncode("abcABC123-._~"sv), "abcABC123-._~"s);
}

TEST(UrlEncodeTestSuite, ReservedChars) {
  EXPECT_EQ(UrlEncode("!#$&'()*+,/:;=?@[]"sv),
            "%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"s);
}

TEST(UrlEncodeTestSuite, Spaces) {
  EXPECT_EQ(UrlEncode("Hello World"sv), "Hello+World"s);
  EXPECT_EQ(UrlEncode("  "sv), "++"s);
}

TEST(UrlEncodeTestSuite, ControlChars) {
  EXPECT_EQ(UrlEncode("\x01\x1F"sv), "%01%1F"s); // Символы < 32
}

TEST(UrlEncodeTestSuite, NonAsciiChars) {
  EXPECT_EQ(UrlEncode("\x80\xFF"sv), "%80%FF"s); // Символы >= 128
  EXPECT_EQ(UrlEncode("Привет"sv), "%D0%9F%D1%80%D0%B8%D0%B2%D0%B5%D1%82"s);
}

TEST(UrlEncodeTestSuite, MixedContent) {
  EXPECT_EQ(UrlEncode("Hello World!"sv), "Hello+World%21"s);
  EXPECT_EQ(UrlEncode("file name.doc"sv), "file+name.doc"s);
  EXPECT_EQ(UrlEncode("100% true"sv), "100%25+true"s);
}
