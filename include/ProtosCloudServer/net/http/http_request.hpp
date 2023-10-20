#pragma once

#include <optional>

#include "ProtosCloudServer/net/http/http_req_arg.h"
#include "ProtosCloudServer/net/http/http_parser.h"
#include "ProtosCloudServer/net/http/header_map.hpp"

namespace ProtosCloudServer{
namespace http {

class HttpRequest {
public:
    static const size_t kHeaderSize = 1024;
    explicit HttpRequest(const std::string&, const std::string&);
    void StoreData(std::string data);
    unsigned long long GetSize() const;
    bool IsEmptyRequest() const;
    std::string& GetPayload();
    std::string_view GetOwner();
private:
    HttpParser parser_;
    unsigned long long size_;
    HeaderMapT headers_map_;
    std::optional<std::string> payload_;
    std::vector<HttpReqArg> args_;
    std::string owner_;
};

} //namespace http
} //namespace ProtosCloudServer