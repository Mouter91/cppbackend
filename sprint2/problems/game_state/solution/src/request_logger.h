#pragma once
#include "request_handler.h"
#include "log.h" 
#include <boost/beast/http.hpp>
#include <chrono>

namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;
using namespace std::chrono;

template<class SomeRequestHandler>
class LoggingRequestHandler {
    template <typename Body, typename Allocator>
    static void LogRequest(http::request<Body, http::basic_fields<Allocator>>& req) {
        boost::json::value jv = {
            {"URI", static_cast<std::string>(req.target())},
            {"method", req.method_string()}
        };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, jv) << "request received";
    }

    static void LogResponse(int resp_duration, int code, const std::string& content_type) {
        json::value jv = {
            {"response_time", resp_duration},
            {"code", code},
            {"content_type", content_type}
        };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, jv) << "response sent";
    }

public:
    LoggingRequestHandler(SomeRequestHandler& handler) : request_handler_(handler) {};

template <typename Body, typename Allocator, typename Send>
void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
    LogRequest(req);
    const auto start_time = steady_clock::now();

    // Оборачиваем отправку ответа, чтобы залогировать ответ в момент отправки
    auto wrapped_send = [start_time, &send](auto&& response) {
        auto duration = duration_cast<milliseconds>(steady_clock::now() - start_time).count();

        std::string content_type;
        if (response.find(http::field::content_type) != response.end()) {
            content_type = std::string(response[http::field::content_type]);
        }

        // Получаем статус
        int status = response.result_int();

        LogResponse(duration, status, content_type.empty() ? "null" : content_type);
        send(std::forward<decltype(response)>(response));
    };

    request_handler_(std::move(req), std::move(wrapped_send));
}

private:
    SomeRequestHandler& request_handler_;
};
