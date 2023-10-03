//
// Created by user on 16.08.2023.
//

#include "ProtosCloudServer/net/boost_asio_http_session.hpp"
#include "ProtosCloudServer/logging/log.hpp"

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;
using namespace ProtosCloudServer::logging;
using namespace ProtosCloudServer::http;

namespace ProtosCloudServer::net {

    BoostAsioHttpSession::BoostAsioHttpSession(tcp::socket &&socket):
        socket_(std::move(socket))
    {}

    std::string BoostAsioHttpSession::GetAddr(){
        error_code error;
        return socket_.remote_endpoint(error).address().to_string();
        if(error){}
    }

    unsigned short BoostAsioHttpSession::GetPort(){
        error_code error;
        return socket_.remote_endpoint(error).port();
        if(error){}
    }

    void BoostAsioHttpSession::Start(std::shared_ptr<HttpClientHandlerBase> clientHandler) {
        clientHandler_ = std::move(clientHandler);
        ReadHeader();
    }

    void BoostAsioHttpSession::Post(const std::string& message){
        outgoingQueue_.enqueue(message);
        std::string msg;
        auto hasElem = outgoingQueue_.try_dequeue(msg);
        if (hasElem)
            AsyncWrite(msg);
    }

//    void BoostAsioHttpSession::AsyncRead() {
//        stream_buf_ptr_.prepare(1024);
//        io::async_read_until(
//            socket_,
//            stream_buf_ptr_,
//            "\r\n",
//            [self = shared_from_this()](auto &&error, auto &&bytes_transferred){
//                self->OnRead(
//                        std::forward<decltype(error)>(error),
//                        std::forward<decltype(bytes_transferred)>(bytes_transferred)
//                );
//            });
//    }

    void BoostAsioHttpSession::ReadHeader() {
        stream_buf_ptr_.prepare(1024);
        io::async_read_until(
            socket_,
            stream_buf_ptr_,
            "\r\n\r\n",
            [this, self = shared_from_this()](auto &&error, auto &&bytes_transferred){
                if(error){
                    if(!socket_.is_open()) return;
                    auto msg = "Error in BoostAsioHttpSession::ReadHeader - async read failure. Remote disconnected";
                    LOG_ERROR() << msg;
                    std::cerr << msg << "\n";
                    return;
                }
                io::streambuf::const_buffers_type bufs = stream_buf_ptr_.data();

                std::string dataAsString(boost::asio::buffers_begin(bufs),
                                         boost::asio::buffers_begin(bufs) + bytes_transferred);

                auto request = std::make_shared<HttpRequest>(dataAsString);
                auto size = request->GetSize();
                if(request->IsEmptyRequest()){
                    HttpResponse response(http::HttpStatus::kBadRequest);
                    Post(response.toString());
                    stream_buf_ptr_.consume(bytes_transferred);
                    ReadHeader();
                    return;
                }
                stream_buf_ptr_.consume(bytes_transferred);
                self->ReadBody(size, request);
            });
    }

    void BoostAsioHttpSession::ReadBody(unsigned long long size, std::shared_ptr<HttpRequest>& request) {
        stream_buf_ptr_.prepare(size);
        boost::asio::async_read(
                socket_,
                stream_buf_ptr_,
                io::transfer_exactly(size - stream_buf_ptr_.size()),
                [this, self = shared_from_this(), request](auto &&error, auto &&bytes_transferred){
                    if(error){
                        LOG_ERROR() << "Error in BoostAsioHttpSession::ReadBody - async read failure" << error;
                        return;
                    }
                    io::streambuf::const_buffers_type bufs = stream_buf_ptr_.data();
                    std::string dataAsString(boost::asio::buffers_begin(bufs),
                                             boost::asio::buffers_begin(bufs) + bytes_transferred);

                    try {
                        auto handlerRef = *clientHandler_;
                        handlerRef(request);
                    } catch (std::exception& e) {
                        LOG_ERROR() << "Error in BoostAsioHttpSession::ReadBody - passing message to handler " << error;
                        HttpResponse response(http::HttpStatus::kInternalServerError);
                        Post(response.toString());
                    }
                    stream_buf_ptr_.consume(bytes_transferred);
                    ReadHeader();
                });
    }

//    void BoostAsioHttpSession::OnRead(error_code error, std::size_t bytes_transferred){
//        if (!error){
//            std::stringstream message;
//            message << socket_.remote_endpoint(error) << ": "
//                    << std::istream(&stream_buf_ptr_).rdbuf();
//            stream_buf_ptr_.consume(bytes_transferred);
//            on_message_(message.str(), {});
//            AsyncRead();
//        }else{
//            socket_.close(error);
//            on_error_(error);
//        }
//    }

    void BoostAsioHttpSession::AsyncWrite(std::string msg) {
        io::async_write(
                socket_,
                io::buffer(msg),
                [self = shared_from_this()](auto &&error, auto &&bytes_transferred){
                    if(error){
                        LOG_ERROR() << "Error in BoostAsioHttpSession::AsyncWrite - async write failure" << error;
                        return;
                    }
                    self->OnWrite(
                            std::forward<decltype(error)>(error),
                            std::forward<decltype(bytes_transferred)>(bytes_transferred)
                    );
                });
    }

    void BoostAsioHttpSession::OnWrite(error_code error, std::size_t bytes_transferred){
        if (!error) {
            std::string msg;
            auto hasElem = outgoingQueue_.try_dequeue(msg);
            if (hasElem)
                AsyncWrite(msg);
        }else{
            socket_.close(error);
            LOG_ERROR() << "Error on write to socket: " << error;
        }
    }

    void BoostAsioHttpSession::CloseSession() {
        LOG_INFO() << "Closing session: " << GetAddr() << ":" << GetPort();
        error_code error;
        socket_.close(error);
        if(error){
            LOG_ERROR() << "Error on closing socket: " << GetAddr() << ":" << GetPort();
        }
    }

    BoostAsioHttpSession::~BoostAsioHttpSession() {
        CloseSession();
    }
}


