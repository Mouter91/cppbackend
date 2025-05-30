#include "actions.h"

#include <boost/algorithm/string/trim.hpp>

#include <pqxx/pqxx>
#include <cassert>
#include <iostream>
#include <sstream>
#include <regex>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfoWithAuthor& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

Actions::Actions(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&Actions::AddAuthor, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "name"s, "Deletes author"s, std::bind(&Actions::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "name"s, "Edits author"s, std::bind(&Actions::EditAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&Actions::AddBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&Actions::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&Actions::ShowBooks, this));
    menu_.AddAction("ShowBook"s, "title"s, "Show detail info of the book"s, std::bind(&Actions::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "title"s, "Deletes book"s, std::bind(&Actions::DeleteBook, this, ph::_1));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s, std::bind(&Actions::ShowAuthorBooks, this));
    menu_.AddAction("EditBook"s, "title"s, "Edits book"s, std::bind(&Actions::EditBook, this, ph::_1));
}

bool Actions::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) {
            output_ << "Failed to add author"sv << std::endl;
            return true;
        }
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool Actions::DeleteAuthor(std::istream &cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        if (name.empty()) {
            const std::optional<detail::AuthorInfo> author_info = SelectAuthorFromList();
            if (!author_info.has_value()) {
                return true;
            }
            use_cases_.DeleteAuthor(author_info.value().id);
            return true;
        }

        const auto author_id = FindAuthorByName(name);
        if (!author_id.has_value()) {
            output_ << "Failed to delete author"sv << std::endl;
            return true;
        }
        use_cases_.DeleteAuthor(author_id.value());
    } catch (const std::exception&) {
        output_ << "Failed to delete author (catch)"sv << std::endl;
    }
    return true;
}

bool Actions::EditAuthor(std::istream &cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        // std::optional<std::string> author_id;
        if (name.empty()) {
            output_ << "Select author:" << std::endl;
            const std::optional<detail::AuthorInfo> author_info = SelectAuthorFromList();
            if (!author_info.has_value()) {
                // output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
            name = author_info.value().name;
        }
        output_ << "Enter new name:"sv << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        if (new_name.empty()) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        if (!FindAuthorByName(name)) {
            output_ << "Failed to edit author"sv << std::endl;
            return true;
        }
        use_cases_.EditAuthor(name, new_name);
    } catch (std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool Actions::AddBook(std::istream& cmd_input) const {
    try {
        if (const auto params = GetBookParams(cmd_input)) {
            use_cases_.AddBook(params.value());
        }
    } catch (const std::exception&) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool Actions::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}

bool Actions::ShowBooks() const {
    PrintVector(output_, GetBooks());
    return true;
}

bool Actions::ShowBook(std::istream &cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        if (title.empty()) {
            const std::optional<detail::BookInfoWithAuthor> book_info = SelectBookFromList();
            if (!book_info.has_value()) {
                return true;
            }
            PrintBookFull(book_info.value());
            return true;
        }
        std::vector<detail::BookInfoWithAuthor> books = FindBooksByTitle(title);
        if (books.empty()) {
            return true;
        }
        std::optional<detail::BookInfoWithAuthor> book_info;
        if (books.size() == 1) {
            book_info = books.front();
        } else {
            book_info = SelectBookFromList(books);
        }
        if (book_info.has_value()) {
            PrintBookFull(book_info.value());
        }
    } catch (std::exception&) {
        output_ << "Failed to show books"sv << std::endl;
    }
    return true;
}

bool Actions::DeleteBook(std::istream &cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        if (title.empty()) {
            const std::optional<detail::BookInfoWithAuthor> book_info = SelectBookFromList();
            if (!book_info.has_value()) {
                return true;
            }
            use_cases_.DeleteBook(book_info.value().id);
            return true;
        }
        std::vector<detail::BookInfoWithAuthor> books = FindBooksByTitle(title);
        if (books.empty()) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }
        std::optional<detail::BookInfoWithAuthor> book_info;
        if (books.size() == 1) {
            book_info = books.front();
        } else {
            book_info = SelectBookFromList(books);
        }
        if (book_info.has_value()) {
            use_cases_.DeleteBook(book_info.value().id);
        }
    } catch (std::exception& e) {
        output_ << "Failed to delete book"sv << "-" << e.what() << std::endl;
    }
    return true;
}

bool Actions::EditBook(std::istream &cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);
        std::optional<detail::BookInfoWithAuthor> book_info;
        if (title.empty()) {
            book_info = SelectBookFromList();
            if (!book_info.has_value()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
        } else {
            std::vector<detail::BookInfoWithAuthor> books = FindBooksByTitle(title);
            if (books.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            if (books.size() == 1) {
                book_info = books.front();
            } else {
                book_info = SelectBookFromList(books);
                if (!book_info.has_value()) {
                    output_ << "Book not found"sv << std::endl;
                    return true;
                }
            }
        }
        const auto new_book_info = EnterNewBookInfo(book_info.value());
        if (!new_book_info.has_value()) {
            output_ << "Failed to edit book"sv << std::endl;
            return true;
        }
        const auto new_tags = EnterNewTags(new_book_info.value());
        use_cases_.EditBook(new_book_info.value(), new_tags);
    } catch (std::exception&) {
        output_ << "Failed to edit book"sv << std::endl;
    }
    return true;
}

bool Actions::ShowAuthorBooks() const {
    try {
        output_ << "Select author:" << std::endl;
        if (const auto author_info = SelectAuthorFromList()) {
            if (author_info) {
                PrintVector(output_, GetAuthorBooks(author_info.value().id));
            }
        }
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to Show Books");
    }
    return true;
}

std::optional<detail::AddBookParams> Actions::GetBookParams(std::istream& cmd_input) const {
    detail::AddBookParams params;

    cmd_input >> params.publication_year;
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);
    if (params.title.empty()) {
        return std::nullopt;
    }
    const auto author_info = SelectAuthor();
    if (!author_info.has_value()) {
        return std::nullopt;
    }
    params.author_id = author_info.value().id;
    output_ << "Enter tags (comma separated):" << std::endl;
    params.tags = SelectTags();
    return params;
}

std::optional<detail::AuthorInfo> Actions::SelectAuthorFromList() const {
    auto authors = GetAuthors();
    if (authors.empty()) {
        return std::nullopt;
    }
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 or author_idx >= authors.size()) {
        throw std::runtime_error("Invalid author num");
    }
    return authors[author_idx];
}


std::optional<detail::AuthorInfo> Actions::SelectAuthorByName(const std::string &name) const {
    auto author_id = FindAuthorByName(name);
    if (author_id) {
        return detail::AuthorInfo{author_id.value(), name};
    }
    output_ << "No author found. Do you want to add " << name << " (y/n)?" << std::endl;
    std::string str;
    getline(input_, str);
    if (str != "y" && str != "Y") {
        output_ << "Failed to add book" << std::endl;
        return std::nullopt;
    }
    return detail::AuthorInfo{use_cases_.AddAuthor(name).value(), name};
}

std::optional<detail::AuthorInfo> Actions::SelectAuthor() const {
    output_ << "Enter author name or empty line to select from list:" << std::endl;
    std::string str;
    if (!std::getline(input_, str)) {
        return std::nullopt;
    }
    if (str.empty()) {
        output_ << "Select author:" << std::endl;
        return SelectAuthorFromList();
    }
    return SelectAuthorByName(str);
}

std::string Actions::NormalizeTag(const std::string &tag) {
    std::string result;
    std::ranges::unique_copy(tag, std::back_inserter(result),
                             [](const char a, const char b) {
                                 return std::isspace(a) && std::isspace(b);
                             });
    const size_t start = result.find_first_not_of(' ');
    const size_t end = result.find_last_not_of(' ');
    return (start == std::string::npos) ? "" : result.substr(start, end - start + 1);
}

std::set<std::string> Actions::SelectTags() const {
    std::string str;
    std::getline(input_, str);
    if (str.empty()) {
        return {};
    }
    std::set<std::string> unique_tags;

    std::istringstream stream(str);
    std::string tag;
    while (std::getline(stream, tag, ',')) {
        std::string normalized_tag = NormalizeTag(tag);
        if (!normalized_tag.empty()) {
            unique_tags.insert(normalized_tag);
        }
    }
    return unique_tags;
}

std::vector<detail::AuthorInfo> Actions::GetAuthors() const {
    return use_cases_.GetAuthors();
}

std::optional<std::string> Actions::FindAuthorByName(const std::string &name) const {
    return use_cases_.FindAuthorByName(name);
}

std::vector<detail::BookInfoWithAuthor> Actions::GetBooks() const {
    return use_cases_.GetBooks();
}

std::vector<detail::BookInfo> Actions::GetAuthorBooks(const std::string &author_id) const {
    return use_cases_.GetAuthorBooks(author_id);
}

std::vector<detail::BookInfoWithAuthor> Actions::FindBooksByTitle(const std::string &title) const {
    return use_cases_.FindBooksByTitle(title);
}


std::optional<detail::BookInfoWithAuthor> Actions::SelectBookFromList() const {
    return SelectBookFromList(GetBooks());
}

std::optional<detail::BookInfoWithAuthor> Actions::SelectBookFromList(const std::vector<detail::BookInfoWithAuthor> &books) const {
    if (books.empty()) {
        return std::nullopt;
    }
    PrintVector(output_, books);
    output_ << "Enter book # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }
    int book_idx;
    try {
        book_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid book num");
    }

    --book_idx;
    if (book_idx < 0 or book_idx >= books.size()) {
        throw std::runtime_error("Invalid book num");
    }
    return books[book_idx];
}

void Actions::PrintBook(const detail::BookInfoWithAuthor &book_info) const {
    output_ << "Title: " << book_info.title << std::endl;
    output_ << "Author: " << book_info.author_name << std::endl;
    output_ << "Publication year: " << book_info.publication_year << std::endl;
}

void Actions::PrintBookTags(const std::string &book_id, const std::string &start_with) const {
    const auto tags = use_cases_.GetTagsByBookId(book_id);
    if (tags.empty()) {
        return;
    }
    if (!start_with.empty()) {
        output_ << start_with;
    }
    for (auto it  = tags.cbegin(); it != tags.cend(); ++it) {
        output_ << *it;
        if (it + 1 < tags.cend()) {
            output_ << ", ";
        }
    }
}

void Actions::PrintBookFull(const detail::BookInfoWithAuthor &book_info) const {
    PrintBook(book_info);
    PrintBookTags(book_info.id, "Tags: "s);
    output_ << std::endl;
}

bool HasNoLetters(const std::string& str) {
    return !std::regex_search(str, std::regex("[a-zA-Z]"));
}

std::optional<detail::BookInfoWithAuthor> Actions::EnterNewBookInfo(const detail::BookInfoWithAuthor &book_info) const {
    std::string str;
    output_ << "Enter new title of empty line to use the current one (" << book_info.title << "):" << std::endl;
    std::getline(input_, str);
    boost::algorithm::trim(str);
    detail::BookInfoWithAuthor new_book_info;
    if (str.empty()) {
        new_book_info.title = book_info.title;
    } else {
        if (HasNoLetters(str)) {
            return std::nullopt;
        }
        new_book_info.title = str;
    }
    output_ << "Enter publication year or empty line to use the current one (" << book_info.publication_year << "):" << std::endl;
    std::getline(input_, str);
    boost::algorithm::trim(str);
    if (str.empty()) {
        new_book_info.publication_year = book_info.publication_year;
    } else {
        new_book_info.publication_year = stoi(str);
    }
    new_book_info.id = book_info.id;
    new_book_info.author_name = book_info.author_name;
    return new_book_info;
}

std::set<std::string> Actions::EnterNewTags(const detail::BookInfoWithAuthor &book_info) const {
    output_ << "Enter tags (current tags: ";
    PrintBookTags(book_info.id, ""s);
    output_ << "):" << std::endl;
    return SelectTags();
}
}  // namespace ui
