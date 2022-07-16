#ifndef PMWCAS_DESCRIPTOR_POOL_HPP
#define PMWCAS_DESCRIPTOR_POOL_HPP

#include <atomic>
#include <utility>

#include "pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class DescriptorPool
{
 public:
  DescriptorPool()
  {
    const auto initial_data = std::make_pair(false, PMwCASDescriptor{});
    for (auto &entry : pool_) {
      entry = initial_data;
    }
  };

  auto
  Get()  //
      -> PMwCASDescriptor *
  {
    thread_local PMwCASDescriptor *desc = nullptr;
    if (desc != nullptr) return desc;

    for (auto &entry : pool_) {
      if (!entry.first) {
        entry.first = true;
        desc = &entry.second;
        return desc;
      }
    }

    return desc;
  }

 private:
  //記述子配列．長さはとりあえず定数．
  std::pair<std::atomic_bool, PMwCASDescriptor> pool_[kDescriptorPoolSize];
};

}  // namespace dbgroup::atomic::pmwcas

#endif
