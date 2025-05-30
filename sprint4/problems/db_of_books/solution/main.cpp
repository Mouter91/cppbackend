#include <iostream>
#include <pqxx/pqxx>
#include <boost/json.hpp>

#define BOOST_BEAST_USE_STD_STRING_VIEW
#define BOOST_BEAST_USE_STD_STRING

using namespace std::literals;
using pqxx::operator"" _zv;
namespace json = boost::json;

// Создание таблицы, если её не существует
void create_table_if_not_exists(pqxx::connection& conn) {
  pqxx::work w(conn);
  w.exec(R"(
        CREATE TABLE IF NOT EXISTS books (
            id SERIAL PRIMARY KEY,
            title VARCHAR(100) NOT NULL,
            author VARCHAR(100) NOT NULL,
            year INTEGER NOT NULL,
            ISBN CHAR(13) UNIQUE
        )
    )"_zv);
  w.commit();
}

// Добавление книги
bool add_book(pqxx::connection& conn, const json::value& payload) {
  try {
    pqxx::work w(conn);

    const std::string title = std::string(payload.at("title").as_string());
    const std::string author = std::string(payload.at("author").as_string());
    int year = static_cast<int>(payload.at("year").as_int64());

    const char* isbn = nullptr;
    if (payload.as_object().contains("ISBN") && !payload.at("ISBN").is_null()) {
      isbn = payload.at("ISBN").as_string().c_str();
    }

    w.exec_params("INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv, title,
                  author, year, isbn);

    w.commit();
    return true;
  } catch (const pqxx::sql_error& e) {
    std::cerr << "SQL error: " << e.what() << std::endl;
    return false;
  } catch (const std::exception& e) {
    std::cerr << "Exception in add_book: " << e.what() << std::endl;
    return false;
  }
}

// Получение списка всех книг
json::array all_books(pqxx::connection& conn) {
  pqxx::read_transaction w(conn);
  auto rows = w.exec(R"(
        SELECT id, title, author, year, ISBN
        FROM books
        ORDER BY year DESC, title, author, ISBN
    )"_zv);

  json::array books;
  for (const auto& row : rows) {
    json::object book;
    book["id"] = row["id"].as<int>();
    book["title"] = std::string(row["title"].c_str());
    book["author"] = std::string(row["author"].c_str());
    book["year"] = row["year"].as<int>();
    book["ISBN"] = row["ISBN"].is_null() ? json::value(nullptr)
                                         : json::value(std::string(row["ISBN"].c_str()));
    books.push_back(book);
  }

  return books;
}

// Точка входа
int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: book_manager <connection_string>" << std::endl;
    return EXIT_FAILURE;
  }

  const std::string conn_str = argv[1];

  try {
    pqxx::connection conn(conn_str);
    create_table_if_not_exists(conn);

    std::string line;
    while (std::getline(std::cin, line)) {
      json::value request = json::parse(line);
      const std::string action = std::string(request.at("action").as_string());
      const json::value& payload =
          request.as_object().contains("payload") ? request.at("payload") : json::object{};

      if (action == "add_book") {
        bool result = add_book(conn, payload);
        std::cout << json::serialize(json::object{{"result", result}}) << std::endl;
      } else if (action == "all_books") {
        json::array books = all_books(conn);
        std::cout << json::serialize(books) << std::endl;
      } else if (action == "exit") {
        break;
      } else {
        std::cerr << "Unknown action: " << action << std::endl;
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
