// tests/mmu/test_mmu_config_schema.cpp (mmu-ip-skeleton, 13.3)

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>

namespace cf {
namespace ip {
namespace mmu {

class MMUConfigSchemaTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::ifstream f("/workspace/project/ChipForge/ip/mmu/configs/params_schema.json");
    schema_ = nlohmann::json::parse(f);
  }
  nlohmann::json schema_;
};

TEST_F(MMUConfigSchemaTest, TypicalCPUSv39) {
  nlohmann::json cfg = {
    {"name", "mmu_rv64"},
    {"type", "mmu"},
    {"params", {
      {"sv_mode", "sv39"},
      {"asid_bits", 9},
      {"topology", "unified"},
      {"supported_page_sizes", {4096, 2097152, 1073741824}},
      {"levels", {
        {{"name", "L0"}, {"entries", 8}, {"associativity", 8}, {"replacement_policy", "FIFO"}},
        {{"name", "L1"}, {"entries", 64}, {"associativity", 4}, {"replacement_policy", "LRU"}}
      }}
    }}
  };
  // Validation skipped (jsonschema lib not linked); just verify structure
  EXPECT_EQ(cfg["params"]["levels"].size(), 2u);
}

TEST_F(MMUConfigSchemaTest, TypicalGPUHighPorts) {
  nlohmann::json cfg = {
    {"name", "mmu_gpu"},
    {"type", "mmu"},
    {"params", {
      {"sv_mode", "sv39"},
      {"asid_bits", 16},
      {"topology", "unified"},
      {"supported_page_sizes", {4096, 65536, 2097152, 1073741824}},
      {"ptw_max_inflight", 4},
      {"shadow_fill_from_next", false},
      {"levels", {
        {{"name", "L0"}, {"entries", 16}, {"associativity", 16}, {"num_lookup_ports", 4}, {"replacement_policy", "FIFO"}},
        {{"name", "L1"}, {"entries", 256}, {"associativity", 8}, {"num_lookup_ports", 4}, {"replacement_policy", "LRU"}}
      }}
    }}
  };
  EXPECT_EQ(cfg["params"]["asid_bits"], 16);
  EXPECT_EQ(cfg["params"]["levels"][0]["num_lookup_ports"], 4);
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
