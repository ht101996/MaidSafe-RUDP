/* Copyright (c) 2010 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Author: Christopher M. Kohlhoff (chris at kohlhoff dot com)

#include "rudp_sender.h"

#include <algorithm>
#include <cassert>

#include "rudp_ack_of_ack_packet.h"
#include "rudp_congestion_control.h"
#include "rudp_peer.h"
#include "rudp_tick_timer.h"
#include "maidsafe/common/utils.h"

namespace asio = boost::asio;
namespace ip = asio::ip;
namespace bptime = boost::posix_time;

namespace maidsafe {

namespace transport {

RudpSender::RudpSender(RudpPeer &peer, RudpTickTimer &tick_timer,
                       RudpCongestionControl &congestion_control)
  : peer_(peer),
    tick_timer_(tick_timer),
    congestion_control_(congestion_control),
    unacked_packets_() {
}

boost::uint32_t RudpSender::GetNextPacketSequenceNumber() const {
  return unacked_packets_.End();
}

bool RudpSender::Flushed() const {
  return unacked_packets_.IsEmpty();
}

size_t RudpSender::AddData(const asio::const_buffer &data) {
  if ((congestion_control_.SendWindowSize() == 0) &&
      (unacked_packets_.Size() == 0)) {
    unacked_packets_.SetMaximumSize(16);
  } else {
    unacked_packets_.SetMaximumSize(congestion_control_.SendWindowSize());
  }
  const unsigned char *begin = asio::buffer_cast<const unsigned char*>(data);
  const unsigned char *ptr = begin;
  const unsigned char *end = begin + asio::buffer_size(data);

  while (!unacked_packets_.IsFull() && (ptr < end)) {
    boost::uint32_t n = unacked_packets_.Append();

    UnackedPacket &p = unacked_packets_[n];
    p.packet.SetPacketSequenceNumber(n);
    p.packet.SetFirstPacketInMessage(true);
    p.packet.SetLastPacketInMessage(true);
    p.packet.SetInOrder(true);
    p.packet.SetMessageNumber(0);
    p.packet.SetTimeStamp(0);
    p.packet.SetDestinationSocketId(peer_.Id());
    size_t length = std::min<size_t>(RudpDataPacket::kMaxDataSize, end - ptr);
    p.packet.SetData(ptr, ptr + length);
    p.lost = true; // Mark as lost so that DoSend() will send it.

    ptr += length;
  }

  DoSend();

  return ptr - begin;
}

void RudpSender::HandleAck(const RudpAckPacket &packet) {
  boost::uint32_t seqnum = packet.PacketSequenceNumber();

  if (packet.HasOptionalFields()) {
    congestion_control_.OnAck(seqnum,
                              packet.RoundTripTime(),
                              packet.RoundTripTimeVariance(),
                              packet.AvailableBufferSize(),
                              packet.PacketsReceivingRate(),
                              packet.EstimatedLinkCapacity());
  } else {
    congestion_control_.OnAck(seqnum);
  }

  RudpAckOfAckPacket response_packet;
  response_packet.SetDestinationSocketId(peer_.Id());
  response_packet.SetAckSequenceNumber(packet.AckSequenceNumber());
  peer_.Send(response_packet);

  if (unacked_packets_.Contains(seqnum) || unacked_packets_.End() == seqnum) {
    while (unacked_packets_.Begin() != seqnum)
      unacked_packets_.Remove();

    DoSend();
  }
}

void RudpSender::HandleNegativeAck(const RudpNegativeAckPacket &packet) {
  // Mark the specified packets as lost.
  for (boost::uint32_t n = unacked_packets_.Begin();
       n != unacked_packets_.End();
       n = unacked_packets_.Next(n)) {
    if (packet.ContainsSequenceNumber(n)) {
      congestion_control_.OnNegativeAck(n);
      unacked_packets_[n].lost = true;
    }
  }

  DoSend();
}

void RudpSender::HandleTick() {
  if (send_timeout_ <= tick_timer_.Now()) {
    // Clear timeout. Will be reset next time a data packet is sent.
    send_timeout_ = bptime::pos_infin;

    // Mark all unacknowledged packets as lost.
    for (boost::uint32_t n = unacked_packets_.Begin();
        n != unacked_packets_.End();
        n = unacked_packets_.Next(n)) {
      congestion_control_.OnSendTimeout(n);
      unacked_packets_[n].lost = true;
    }
  }

  DoSend();
}

void RudpSender::DoSend() {
  bptime::ptime now = tick_timer_.Now();

  for (boost::uint32_t n = unacked_packets_.Begin();
       n != unacked_packets_.End();
       n = unacked_packets_.Next(n)) {
    UnackedPacket &p = unacked_packets_[n];

    if (p.lost) {
      // Check whether we are allowed to send another packet at this time.
      bptime::time_duration send_delay = congestion_control_.SendDelay();
//       if (send_delay > bptime::milliseconds(0)) {
//         tick_timer_.TickAt(now + send_delay);
//         return;
//       }

      // Send the packet.
      peer_.Send(p.packet);
      p.lost = false;
      p.last_send_time = now;
//       tick_timer_.TickAt(now + send_delay);
//       return;
//       congestion_control_.OnDataPacketSent(n);
    }
  }

  // Set the send timeout so that unacknowledged packets can be marked as lost.
  if (!unacked_packets_.IsEmpty()) {
    send_timeout_ = unacked_packets_.Front().last_send_time +
                    congestion_control_.SendTimeout();
    tick_timer_.TickAt(send_timeout_);
  }
}

void RudpSender::NotifyClose() {
  RudpShutdownPacket shut_down_packet;
  shut_down_packet.SetDestinationSocketId(peer_.Id());
  peer_.Send(shut_down_packet);
}

}  // namespace transport

}  // namespace maidsafe
