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

#ifndef MAIDSAFE_TRANSPORT_RUDP_PEER_H_
#define MAIDSAFE_TRANSPORT_RUDP_PEER_H_

#include "boost/asio/ip/udp.hpp"
#include "boost/cstdint.hpp"
#include "maidsafe/transport/rudp_multiplexer.h"

namespace maidsafe {

namespace transport {

class RudpPeer {
 public:
  explicit RudpPeer(RudpMultiplexer &multiplexer)  // NOLINT (Fraser)
    : multiplexer_(multiplexer), endpoint_(), id_(0) {}

  const boost::asio::ip::udp::endpoint &Endpoint() const { return endpoint_; }
  void SetEndpoint(const boost::asio::ip::udp::endpoint &ep) { endpoint_ = ep; }

  boost::uint32_t Id() const { return id_; }
  void SetId(boost::uint32_t id) { id_ = id; }

  template <typename Packet>
  TransportCondition Send(const Packet &packet) {
    return multiplexer_.SendTo(packet, endpoint_);
  }

 private:
  // Disallow copying and assignment.
  RudpPeer(const RudpPeer&);
  RudpPeer &operator=(const RudpPeer&);

  // The multiplexer used to send and receive UDP packets.
  RudpMultiplexer &multiplexer_;

  // The remote socket's endpoint and identifier.
  boost::asio::ip::udp::endpoint endpoint_;
  boost::uint32_t id_;
};

}  // namespace transport

}  // namespace maidsafe

#endif  // MAIDSAFE_TRANSPORT_RUDP_PEER_H_