#include "http_server.h"
#include <boost/asio/dispatch.hpp>

namespace http_server {

    void ReportError(beast::error_code ec, std::string_view what) {
    std::cerr << what << ": " << ec.message() << std::endl;
    }

    void SessionBase::Run() {
    // Вызываем метод Read, используя executor объекта stream_.
    // Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
    }

    void SessionBase::Read() {
        using namespace std::literals;
        // Очищаем запрос от прежнего значения (метод Read может быть вызван несколько раз)
        request_ = {};
        stream_.expires_after(30s);
        // Считываем request_ из stream_, используя buffer_ для хранения считанных данных
        http::async_read(stream_, buffer_, request_,
                         // По окончании операции будет вызван метод OnRead
                         beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
    }

    void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
        using namespace std::literals;
        if (ec == http::error::end_of_stream) {
            // Нормальная ситуация - клиент закрыл соединение
            return Close();
        }
        if (ec) {
            return ReportError(ec, "read"sv);
        }
        HandleRequest(std::move(request_));
    }

    void SessionBase::Close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
        if (ec) {
            return ReportError(ec, "write");
        }

        if (close) {
            // Семантика ответа требует закрыть соединение
            return Close();
        }

        // Считываем следующий запрос
        Read();
    }

    template <typename RequestHandler>
    void Listener<RequestHandler>::Run() {
        DoAccept();
    }

    template <typename RequestHandler>
    void Listener<RequestHandler>::DoAccept() {
            acceptor_.async_accept(
                // Передаём последовательный исполнитель, в котором будут вызываться обработчики
                // асинхронных операций сокета
                net::make_strand(ioc_),
                // С помощью bind_front_handler создаём обработчик, привязанный к методу OnAccept
                // текущего объекта.
                // Так как Listener — шаблонный класс, нужно подсказать компилятору, что
                // shared_from_this — метод класса, а не свободная функция.
                // Для этого вызываем его, используя this
                // Этот вызов bind_front_handler аналогичен
                // namespace ph = std::placeholders;
                // std::bind(&Listener::OnAccept, this->shared_from_this(), ph::_1, ph::_2)
                beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
        }

    template <typename RequestHandler>
    void Listener<RequestHandler>::OnAccept(sys::error_code ec, tcp::socket socket) {
        using namespace std::literals;

        if (ec) {
            return ReportError(ec, "accept"sv);
        }

        // Асинхронно обрабатываем сессию
        AsyncRunSession(std::move(socket));

        // Принимаем новое соединение
        DoAccept();
    }

    template <typename RequestHandler>
    void Listener<RequestHandler>::AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
    }

}  // namespace http_server
