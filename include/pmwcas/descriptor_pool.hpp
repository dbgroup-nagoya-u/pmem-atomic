#ifndef PMWCAS_DESCRIPTOR_POOL_HPP
#define PMWCAS_DESCRIPTOR_POOL_HPP

#include <atomic>
#include <iostream>
#include <memory>
#include <utility>

#include "element_holder.hpp"
#include "pmwcas_descriptor.hpp"

namespace dbgroup::atomic::pmwcas
{

class DescriptorPool
{
 public:
  DescriptorPool() {}

  auto
  Get()  //
      -> PMwCASDescriptor *
  {
    thread_local std::unique_ptr<ElementHolder> ptr = nullptr;

    while (!ptr) {
      for (auto &element : pool_) {
        auto is_reserved = element.is_reserved.load(std::memory_order_relaxed);
        if (!is_reserved) {
          auto is_changed = element.is_reserved.compare_exchange_strong(is_reserved, true,
                                                                        std::memory_order_relaxed);
          if (is_changed) {
            ptr = std::make_unique<ElementHolder>(&element);
            break;
          }
        }
      }
    }

    return ptr->GetHoldDescriptor();
  }

 private:
  //記述子配列．長さはとりあえず定数．
  PoolElement pool_[kDescriptorPoolSize];
};

}  // namespace dbgroup::atomic::pmwcas

#endif
