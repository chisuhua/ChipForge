// ip/cache/policies/replacement_policy.cpp
//
// 功能描述: ReplacementPolicy 工厂方法实现 (Phase 1.4)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-18
//
// 工厂方法 create() 需要具体策略的完整定义, 因此单独编译:
//   - replacement_policy.h 只声明 class (前向声明 NoReplacementPolicy / LRUPolicy)
//   - replacement_policy.cpp 实现 create() 并包含具体策略头文件
//
// 命名空间: cf::ip::cache::policies (与 cpptlm::Policy 隔离)
//
// 详见:
//   - openspec/changes/cache-policy-foundation/proposal.md
//   - ip/cache/policies/replacement_policy.h (声明)

#include "ip/cache/policies/replacement_policy.h"

#include <stdexcept>
#include <string>
#include <utility>

#include "ip/cache/policies/lru_policy.h"
#include "ip/cache/policies/no_replacement_policy.h"

namespace cf {
namespace ip {
namespace cache {
namespace policies {

std::unique_ptr<ReplacementPolicy>
ReplacementPolicy::create(const std::string& name) {
  if (name == "None") {
    return std::unique_ptr<ReplacementPolicy>(
        new NoReplacementPolicy());  // 不使用 make_unique (避免模板依赖传播)
  }
  if (name == "LRU") {
    return std::unique_ptr<ReplacementPolicy>(
        new LRUPolicy());  // 不使用 make_unique (避免模板依赖传播)
  }
  throw std::runtime_error(
      "ReplacementPolicy::create(): unknown policy name '" + name +
      "' (supported: 'None', 'LRU')");
}

}  // namespace policies
}  // namespace cache
}  // namespace ip
}  // namespace cf
