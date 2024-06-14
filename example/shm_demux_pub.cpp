// #include <boost/log/core.hpp>
// #include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include "../src/demultiplexer.h"
#include "./demux_util.h"

auto main() -> int {
  namespace bipc = boost::interprocess;
  //   using boost::interprocess::read_write;
  //   using boost::interprocess::shared_memory_object;

  lshl::demux::init_logging();

  const char* shared_memory_name = "demux_example";

  BOOST_LOG_TRIVIAL(info) << "shm_demux_pub shared_memory_name: " << shared_memory_name << '\n';

  struct shm_remove {
    shm_remove(const char* name) : name_(name) {
      bipc::shared_memory_object::remove(this->name_);
      BOOST_LOG_TRIVIAL(info) << "removed shared_memory_object: " << this->name_ << '\n';
    }
    ~shm_remove() {
      bipc::shared_memory_object::remove(this->name_);
      BOOST_LOG_TRIVIAL(info) << "removed shared_memory_object: " << this->name_ << '\n';
    }
    const char* name_;
  } remover(shared_memory_name);

  bipc::managed_shared_memory shm_obj(bipc::open_or_create, shared_memory_name, 65536);
  BOOST_LOG_TRIVIAL(info) << "created shared_memory_object: " << shared_memory_name << '\n';

  //   lshl::demux::DemultiplexerPublisher pub();

  return 0;
}