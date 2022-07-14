#ifndef PMWCAS_DESCRIPTOR_POOL_HPP
#define PMWCAS_DESCRIPTOR_POOL_HPP

#include <atomic>
#include <utility>

#include "pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class DescriptorPool
{
  using PMwCASDescriptor = component::PMwCASDescriptor;

 public:
  DescriptorPool() : pool_.fill(std::make_pair(false, PMwCASDescriptor{})){};

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

    // ここまできちゃったときの処理が必要？
  }

 private:
  std::array<std::pair<std::atomic_bool, PMwCASDescriptor>, 1024> pool_;
};

}  // namespace dbgroup::atomic::pmwcas

#endif
