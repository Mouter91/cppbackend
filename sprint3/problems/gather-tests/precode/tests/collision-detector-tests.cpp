#define _USE_MATH_DEFINES

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sstream>
#include <vector>

#include "../src/collision_detector.h"

namespace Catch {
template <>
struct StringMaker<collision_detector::GatheringEvent> {
  static std::string convert(collision_detector::GatheringEvent const& value) {
    std::ostringstream tmp;
    tmp << "(" << value.gatherer_id << "," << value.item_id << "," << value.sq_distance << ","
        << value.time << ")";
    return tmp.str();
  }
};
}  // namespace Catch

namespace {

class TestProvider : public collision_detector::ItemGathererProvider {
 public:
  TestProvider(std::vector<collision_detector::Item> items,
               std::vector<collision_detector::Gatherer> gatherers)
      : items_(std::move(items)), gatherers_(std::move(gatherers)) {
  }

  size_t ItemsCount() const override {
    return items_.size();
  }

  collision_detector::Item GetItem(size_t idx) const override {
    return items_.at(idx);
  }

  size_t GatherersCount() const override {
    return gatherers_.size();
  }

  collision_detector::Gatherer GetGatherer(size_t idx) const override {
    return gatherers_.at(idx);
  }

 private:
  std::vector<collision_detector::Item> items_;
  std::vector<collision_detector::Gatherer> gatherers_;
};

}  // namespace

TEST_CASE("No gatherers or items") {
  TestProvider provider({}, {});
  auto events = collision_detector::FindGatherEvents(provider);
  REQUIRE(events.empty());
}

TEST_CASE("Single gatherer with no items") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  TestProvider provider({}, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);
  REQUIRE(events.empty());
}

TEST_CASE("Single item with no gatherers") {
  collision_detector::Item item{{5.0, 0.0}, 1.0};
  TestProvider provider({item}, {});
  auto events = collision_detector::FindGatherEvents(provider);
  REQUIRE(events.empty());
}

TEST_CASE("Single gatherer collects single item") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  collision_detector::Item item{{5.0, 0.0}, 1.0};
  TestProvider provider({item}, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 1);
  CHECK(events[0].gatherer_id == 0);
  CHECK(events[0].item_id == 0);
  CHECK(events[0].time == Approx(0.5).margin(1e-10));
  CHECK(events[0].sq_distance == Approx(0.0).margin(1e-10));
}

TEST_CASE("Single gatherer collects multiple items in order") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{
      {{3.0, 0.0}, 1.0},  // First to be collected
      {{7.0, 0.0}, 1.0},  // Second to be collected
      {{5.0, 0.0}, 1.0}   // Third to be collected (but in the middle)
  };
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 3);

  // Check chronological order
  CHECK(events[0].time <= events[1].time);
  CHECK(events[1].time <= events[2].time);

  // Check specific events
  CHECK(events[0].item_id == 0);
  CHECK(events[0].time == Approx(0.3).margin(1e-10));

  CHECK(events[1].item_id == 2);
  CHECK(events[1].time == Approx(0.5).margin(1e-10));

  CHECK(events[2].item_id == 1);
  CHECK(events[2].time == Approx(0.7).margin(1e-10));
}

TEST_CASE("Multiple gatherers collect items") {
  std::vector<collision_detector::Gatherer> gatherers{
      {{0.0, 0.0}, {10.0, 0.0}, 1.0},  // Gatherer 0
      {{0.0, 5.0}, {10.0, 5.0}, 1.0}   // Gatherer 1
  };
  std::vector<collision_detector::Item> items{
      {{5.0, 0.0}, 1.0},  // Collected by gatherer 0
      {{5.0, 5.0}, 1.0},  // Collected by gatherer 1
      {{3.0, 0.0}, 1.0}   // Collected by gatherer 0
  };
  TestProvider provider(items, gatherers);
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 3);

  // Check chronological order
  CHECK(events[0].time <= events[1].time);
  CHECK(events[1].time <= events[2].time);

  // First event - gatherer 0 collects item 2
  CHECK(events[0].gatherer_id == 0);
  CHECK(events[0].item_id == 2);
  CHECK(events[0].time == Approx(0.3).margin(1e-10));

  // Second event - gatherer 0 collects item 0
  CHECK(events[1].gatherer_id == 0);
  CHECK(events[1].item_id == 0);
  CHECK(events[1].time == Approx(0.5).margin(1e-10));

  // Third event - gatherer 1 collects item 1
  CHECK(events[2].gatherer_id == 1);
  CHECK(events[2].item_id == 1);
  CHECK(events[2].time == Approx(0.5).margin(1e-10));
}

TEST_CASE("Items not in path are not collected") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{
      {{5.0, 2.0}, 0.5},   // Too far away (distance 2 > 1.5 (1.0 + 0.5))
      {{-5.0, 0.0}, 1.0},  // Behind the start point
      {{15.0, 0.0}, 1.0}   // Beyond the end point
  };
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.empty());
}

TEST_CASE("Stationary gatherer collects nothing") {
  collision_detector::Gatherer gatherer{
      {5.0, 0.0}, {5.0, 0.0}, 1.0  // Zero-length movement
  };
  collision_detector::Item item{
      {5.0, 0.0}, 1.0  // Exactly at gatherer's position
  };
  TestProvider provider({item}, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.empty());
}

TEST_CASE("Items at the edge of collection radius are collected") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{
      {{5.0, 1.0}, 0.0},  // Exactly at collection radius (distance 1 = 1 + 0)
      {{5.0, 1.1}, 0.0},  // Just beyond collection radius (distance 1.1 > 1 + 0)
      {{5.0, 0.9}, 0.1}   // Within collection radius (distance 0.9 < 1 + 0.1)
  };
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 2);

  CHECK(events[0].item_id == 0);
  CHECK(events[0].sq_distance == Approx(1.0).margin(1e-10));

  CHECK(events[1].item_id == 2);
  CHECK(events[1].sq_distance == Approx(0.81).margin(1e-10));
}

TEST_CASE("Multiple events at same time are allowed") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{
      {{5.0, 0.0}, 1.0},  // Center
      {{5.0, 1.0}, 0.0}   // Edge
  };
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 2);
  CHECK(events[0].time == Approx(0.5).margin(1e-10));
  CHECK(events[1].time == Approx(0.5).margin(1e-10));

  // Order of simultaneous events doesn't matter, but both should be present
  std::set<size_t> item_ids{events[0].item_id, events[1].item_id};
  CHECK(item_ids.count(0) == 1);
  CHECK(item_ids.count(1) == 1);
}
