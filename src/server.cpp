//
// Created by n8 on 3/16/18.
//

#include <iostream>
#include "server.h"
#include "client_waiter.h"

namespace nxudp
{

server::server(asio::io_service& io) :
    _io(io),
    _socket(io, udp::endpoint(udp::v4(), 0))
{
    unsigned short port = _socket.local_endpoint().port();
    std::cout << "Listening port " << port << std::endl;

    start_receive();
}

server::~server()
{
}

void server::start_receive()
{
    auto func = std::bind(&server::async_receive_callback, this,
                          std::placeholders::_1,
                          std::placeholders::_2);

    _socket.async_receive_from(asio::buffer(_receive_buffer), _remote_endpoint, func);
}

void server::async_receive_callback(const asio::error_code &error, std::size_t bytes_transferred)
{
    int timeout;
    if (error || !parse_timeout(_receive_buffer, bytes_transferred, timeout))
    {
        std::cout << "The server received an invalid timeout from client: "<< _remote_endpoint
                  << ". Ignoring..." << std::endl;
    }
    else
    {
        auto waiter = std::make_shared<client_waiter>(*this, _io, _remote_endpoint, timeout);

        add_waiter(waiter);

        waiter->start_wait();
        std::cout << "Received request from " << _remote_endpoint << " with value \"" << timeout << "\"" << std::endl;
    }

    start_receive();
}

void server::async_send_callback(const std::shared_ptr<client_waiter> &waiter,
                                 const std::string &message,
                                 const asio::error_code &error,
                                 std::size_t /*bytes_transferred*/)
{
    if(error)
    {
        std::cout << "Error sending: " << error << std::endl;
    }

    remove_waiter(waiter);

    std::string partial_response = "Sent response \"" + message + "\" to ";
    std::cout << partial_response << waiter->remote_endpoint() << " Thread: " << std::this_thread::get_id() << std::endl;
}

bool server::parse_timeout(server::receive_buffer &buffer, size_t bytes_transferred, int &out_timeout)
{
    if(bytes_transferred < sizeof(int))
    {
        out_timeout = std::numeric_limits<int>::min();
        return false;
    }

    out_timeout = nxudp::utils::buffer_to_value(buffer);
    return true;
}

void server::wait_completed(const std::shared_ptr<client_waiter>& waiter)
{
    auto func = std::bind(&server::async_send_callback, this,
                            waiter,
                            _RESPONSE,
                            std::placeholders::_1,
                            std::placeholders::_2);

    _socket.async_send_to(asio::buffer(_RESPONSE), waiter->remote_endpoint(), func);
}

void server::add_waiter(const std::shared_ptr<client_waiter> &waiter)
{
    std::lock_guard<std::mutex> lock(_waiters_mutex);
    _waiters.insert(waiter);
}

void server::remove_waiter(const std::shared_ptr<client_waiter> &waiter)
{
    std::lock_guard<std::mutex> lock(_waiters_mutex);
    _waiters.erase(waiter);
}

}// namespace nxudp