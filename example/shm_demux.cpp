// #include <boost/log/core.hpp>
// #include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include "../src/demultiplexer.h"
#include "./demux_util.h"

namespace bipc = boost::interprocess;

struct ShmRemover {
  ShmRemover(const char* name) : name_(name) {
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(warning) << "[startup] removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(info) << "[startup] could not remove shared_memory_object: " << this->name_
                              << ", possible it did not exist.";
    }
  }

  ~ShmRemover() {
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(info) << "[shutdown] removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(error) << "[shutdown] could not remove shared_memory_object: " << this->name_;
    }
  }

 private:
  const char* name_;
};

auto main() -> int {
  using std::size_t;

  lshl::demux::init_logging();

  // the size of the shared memory should be  a multiple of the page size (4kB on Linux). Because the operating system
  // performs mapping operations over whole pages. So, you don't waste memory.
  const size_t shared_memory_size = 65536;
  const char* shared_memory_name = "demux_example";

  BOOST_LOG_TRIVIAL(info) << "shm_demux_pub shared_memory_name: " << shared_memory_name;

  const ShmRemover remover(shared_memory_name);

  bipc::managed_shared_memory shm_obj(bipc::create_only, shared_memory_name, shared_memory_size);
  BOOST_LOG_TRIVIAL(info) << "created shared_memory_object: " << shared_memory_name;

  //   lshl::demux::DemultiplexerPublisher pub();

  return 0;
}