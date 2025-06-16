#include "http_server.h"
#include "log.h"
#include <boost/asio/dispatch.hpp>

namespace http_server {

void ReportError(beast::error_code ec, std::string_view where) {
  ServerErrorLog(ec.value(), ec.what(), where);
}
}  // namespace http_server
