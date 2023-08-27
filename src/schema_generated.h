// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SCHEMA_SHMSEQUENCER_H_
#define FLATBUFFERS_GENERATED_SCHEMA_SHMSEQUENCER_H_

#include "flatbuffers/flatbuffers.h"

// Ensure the included flatbuffers.h is the same version as when this file was
// generated, otherwise it may not be compatible.
static_assert(FLATBUFFERS_VERSION_MAJOR == 23 &&
              FLATBUFFERS_VERSION_MINOR == 5 &&
              FLATBUFFERS_VERSION_REVISION == 26,
             "Non-compatible flatbuffers version included");

namespace ShmSequencer {

struct Header;

struct DownstreamPacket;
struct DownstreamPacketBuilder;

struct UpstreamPacket;
struct UpstreamPacketBuilder;

struct RequestPacket;
struct RequestPacketBuilder;

FLATBUFFERS_MANUALLY_ALIGNED_STRUCT(8) Header FLATBUFFERS_FINAL_CLASS {
 private:
  uint64_t sequence_number_;
  uint16_t message_count_;
  int16_t padding0__;  int32_t padding1__;

 public:
  Header()
      : sequence_number_(0),
        message_count_(0),
        padding0__(0),
        padding1__(0) {
    (void)padding0__;
    (void)padding1__;
  }
  Header(uint64_t _sequence_number, uint16_t _message_count)
      : sequence_number_(::flatbuffers::EndianScalar(_sequence_number)),
        message_count_(::flatbuffers::EndianScalar(_message_count)),
        padding0__(0),
        padding1__(0) {
    (void)padding0__;
    (void)padding1__;
  }
  uint64_t sequence_number() const {
    return ::flatbuffers::EndianScalar(sequence_number_);
  }
  uint16_t message_count() const {
    return ::flatbuffers::EndianScalar(message_count_);
  }
};
FLATBUFFERS_STRUCT_END(Header, 16);

struct DownstreamPacket FLATBUFFERS_FINAL_CLASS : private ::flatbuffers::Table {
  typedef DownstreamPacketBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_HEADER = 4,
    VT_MESSAGE_LENGTH = 6,
    VT_MESSAGE_DATA = 8
  };
  const ShmSequencer::Header *header() const {
    return GetStruct<const ShmSequencer::Header *>(VT_HEADER);
  }
  uint16_t message_length() const {
    return GetField<uint16_t>(VT_MESSAGE_LENGTH, 0);
  }
  const ::flatbuffers::Vector<uint8_t> *message_data() const {
    return GetPointer<const ::flatbuffers::Vector<uint8_t> *>(VT_MESSAGE_DATA);
  }
  bool Verify(::flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyFieldRequired<ShmSequencer::Header>(verifier, VT_HEADER, 8) &&
           VerifyField<uint16_t>(verifier, VT_MESSAGE_LENGTH, 2) &&
           VerifyOffsetRequired(verifier, VT_MESSAGE_DATA) &&
           verifier.VerifyVector(message_data()) &&
           verifier.EndTable();
  }
};

struct DownstreamPacketBuilder {
  typedef DownstreamPacket Table;
  ::flatbuffers::FlatBufferBuilder &fbb_;
  ::flatbuffers::uoffset_t start_;
  void add_header(const ShmSequencer::Header *header) {
    fbb_.AddStruct(DownstreamPacket::VT_HEADER, header);
  }
  void add_message_length(uint16_t message_length) {
    fbb_.AddElement<uint16_t>(DownstreamPacket::VT_MESSAGE_LENGTH, message_length, 0);
  }
  void add_message_data(::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> message_data) {
    fbb_.AddOffset(DownstreamPacket::VT_MESSAGE_DATA, message_data);
  }
  explicit DownstreamPacketBuilder(::flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ::flatbuffers::Offset<DownstreamPacket> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = ::flatbuffers::Offset<DownstreamPacket>(end);
    fbb_.Required(o, DownstreamPacket::VT_HEADER);
    fbb_.Required(o, DownstreamPacket::VT_MESSAGE_DATA);
    return o;
  }
};

inline ::flatbuffers::Offset<DownstreamPacket> CreateDownstreamPacket(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    const ShmSequencer::Header *header = nullptr,
    uint16_t message_length = 0,
    ::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> message_data = 0) {
  DownstreamPacketBuilder builder_(_fbb);
  builder_.add_message_data(message_data);
  builder_.add_header(header);
  builder_.add_message_length(message_length);
  return builder_.Finish();
}

inline ::flatbuffers::Offset<DownstreamPacket> CreateDownstreamPacketDirect(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    const ShmSequencer::Header *header = nullptr,
    uint16_t message_length = 0,
    const std::vector<uint8_t> *message_data = nullptr) {
  auto message_data__ = message_data ? _fbb.CreateVector<uint8_t>(*message_data) : 0;
  return ShmSequencer::CreateDownstreamPacket(
      _fbb,
      header,
      message_length,
      message_data__);
}

struct UpstreamPacket FLATBUFFERS_FINAL_CLASS : private ::flatbuffers::Table {
  typedef UpstreamPacketBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_HEADER = 4,
    VT_MESSAGE_LENGTH = 6,
    VT_MESSAGE_DATA = 8
  };
  const ShmSequencer::Header *header() const {
    return GetStruct<const ShmSequencer::Header *>(VT_HEADER);
  }
  uint16_t message_length() const {
    return GetField<uint16_t>(VT_MESSAGE_LENGTH, 0);
  }
  const ::flatbuffers::Vector<uint8_t> *message_data() const {
    return GetPointer<const ::flatbuffers::Vector<uint8_t> *>(VT_MESSAGE_DATA);
  }
  bool Verify(::flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyFieldRequired<ShmSequencer::Header>(verifier, VT_HEADER, 8) &&
           VerifyField<uint16_t>(verifier, VT_MESSAGE_LENGTH, 2) &&
           VerifyOffsetRequired(verifier, VT_MESSAGE_DATA) &&
           verifier.VerifyVector(message_data()) &&
           verifier.EndTable();
  }
};

struct UpstreamPacketBuilder {
  typedef UpstreamPacket Table;
  ::flatbuffers::FlatBufferBuilder &fbb_;
  ::flatbuffers::uoffset_t start_;
  void add_header(const ShmSequencer::Header *header) {
    fbb_.AddStruct(UpstreamPacket::VT_HEADER, header);
  }
  void add_message_length(uint16_t message_length) {
    fbb_.AddElement<uint16_t>(UpstreamPacket::VT_MESSAGE_LENGTH, message_length, 0);
  }
  void add_message_data(::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> message_data) {
    fbb_.AddOffset(UpstreamPacket::VT_MESSAGE_DATA, message_data);
  }
  explicit UpstreamPacketBuilder(::flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ::flatbuffers::Offset<UpstreamPacket> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = ::flatbuffers::Offset<UpstreamPacket>(end);
    fbb_.Required(o, UpstreamPacket::VT_HEADER);
    fbb_.Required(o, UpstreamPacket::VT_MESSAGE_DATA);
    return o;
  }
};

inline ::flatbuffers::Offset<UpstreamPacket> CreateUpstreamPacket(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    const ShmSequencer::Header *header = nullptr,
    uint16_t message_length = 0,
    ::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> message_data = 0) {
  UpstreamPacketBuilder builder_(_fbb);
  builder_.add_message_data(message_data);
  builder_.add_header(header);
  builder_.add_message_length(message_length);
  return builder_.Finish();
}

inline ::flatbuffers::Offset<UpstreamPacket> CreateUpstreamPacketDirect(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    const ShmSequencer::Header *header = nullptr,
    uint16_t message_length = 0,
    const std::vector<uint8_t> *message_data = nullptr) {
  auto message_data__ = message_data ? _fbb.CreateVector<uint8_t>(*message_data) : 0;
  return ShmSequencer::CreateUpstreamPacket(
      _fbb,
      header,
      message_length,
      message_data__);
}

struct RequestPacket FLATBUFFERS_FINAL_CLASS : private ::flatbuffers::Table {
  typedef RequestPacketBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_HEADER = 4
  };
  const ShmSequencer::Header *header() const {
    return GetStruct<const ShmSequencer::Header *>(VT_HEADER);
  }
  bool Verify(::flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyFieldRequired<ShmSequencer::Header>(verifier, VT_HEADER, 8) &&
           verifier.EndTable();
  }
};

struct RequestPacketBuilder {
  typedef RequestPacket Table;
  ::flatbuffers::FlatBufferBuilder &fbb_;
  ::flatbuffers::uoffset_t start_;
  void add_header(const ShmSequencer::Header *header) {
    fbb_.AddStruct(RequestPacket::VT_HEADER, header);
  }
  explicit RequestPacketBuilder(::flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ::flatbuffers::Offset<RequestPacket> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = ::flatbuffers::Offset<RequestPacket>(end);
    fbb_.Required(o, RequestPacket::VT_HEADER);
    return o;
  }
};

inline ::flatbuffers::Offset<RequestPacket> CreateRequestPacket(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    const ShmSequencer::Header *header = nullptr) {
  RequestPacketBuilder builder_(_fbb);
  builder_.add_header(header);
  return builder_.Finish();
}

inline const ShmSequencer::RequestPacket *GetRequestPacket(const void *buf) {
  return ::flatbuffers::GetRoot<ShmSequencer::RequestPacket>(buf);
}

inline const ShmSequencer::RequestPacket *GetSizePrefixedRequestPacket(const void *buf) {
  return ::flatbuffers::GetSizePrefixedRoot<ShmSequencer::RequestPacket>(buf);
}

inline bool VerifyRequestPacketBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<ShmSequencer::RequestPacket>(nullptr);
}

inline bool VerifySizePrefixedRequestPacketBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<ShmSequencer::RequestPacket>(nullptr);
}

inline void FinishRequestPacketBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<ShmSequencer::RequestPacket> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedRequestPacketBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<ShmSequencer::RequestPacket> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace ShmSequencer

#endif  // FLATBUFFERS_GENERATED_SCHEMA_SHMSEQUENCER_H_
