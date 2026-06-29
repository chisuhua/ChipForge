// Bundles extension: TlbReq / TlbResp (mmu-ip-skeleton, 9.1-9.2)
// 追加到 bundles/mem_bundles.h

#include "cf/plugin/uint_t.h"

namespace cf {
namespace bundles {

struct TlbReq {
  cf::plugin::uint_t<64> vaddr{0};
  cf::plugin::uint_t<16> asid{0};
  cf::plugin::bool_t    is_fetch{false};
  cf::plugin::uint_t<8> id{0};
};

struct TlbResp {
  cf::plugin::uint_t<64> paddr{0};
  cf::plugin::uint_t<8>  perms{0};
  cf::plugin::bool_t     hit{false};
  cf::plugin::bool_t     fault{false};
  cf::plugin::uint_t<4>  fault_code{0};
  cf::plugin::uint_t<8>  id{0};
};

}  // namespace bundles
}  // namespace cf
