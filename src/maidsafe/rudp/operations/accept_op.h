/*******************************************************************************
 *  Copyright 2012 MaidSafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of MaidSafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of MaidSafe.net. *
 ******************************************************************************/
// Original author: Christopher M. Kohlhoff (chris at kohlhoff dot com)

#ifndef MAIDSAFE_TRANSPORT_RUDP_ACCEPT_OP_H_
#define MAIDSAFE_TRANSPORT_RUDP_ACCEPT_OP_H_

#include "boost/asio/handler_alloc_hook.hpp"
#include "boost/asio/handler_invoke_hook.hpp"
#include "boost/system/error_code.hpp"
#include "maidsafe/transport/transport.h"

#include "maidsafe/transport/rudp_socket.h"

namespace maidsafe {

namespace transport {

// Helper class to adapt an accept handler into a waiting operation.
template <typename AcceptHandler>
class RudpAcceptOp {
 public:
  RudpAcceptOp(AcceptHandler handler, RudpSocket &socket)  // NOLINT (Fraser)
    : handler_(handler),
      socket_(socket) {}

  void operator()(boost::system::error_code) {
    boost::system::error_code ec;
    if (socket_.RemoteId() == 0)
      ec = boost::asio::error::operation_aborted;
    handler_(ec);
  }

  friend void *asio_handler_allocate(size_t n, RudpAcceptOp *op) {
    using boost::asio::asio_handler_allocate;
    return asio_handler_allocate(n, &op->handler_);
  }

  friend void asio_handler_deallocate(void *p, size_t n, RudpAcceptOp *op) {
    using boost::asio::asio_handler_deallocate;
    asio_handler_deallocate(p, n, &op->handler_);
  }

  template <typename Function>
  friend void asio_handler_invoke(const Function &f, RudpAcceptOp *op) {
    using boost::asio::asio_handler_invoke;
    asio_handler_invoke(f, &op->handler_);
  }

 private:
  AcceptHandler handler_;
  RudpSocket &socket_;
};

}  // namespace transport

}  // namespace maidsafe

#endif  // MAIDSAFE_TRANSPORT_RUDP_ACCEPT_OP_H_