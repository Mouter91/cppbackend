#define _USE_MATH_DEFINES

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sstream>
#include <vector>
#include <cmath>
#include <set>

#include "../src/collision_detector.h"

using Catch::Matchers::WithinAbs;

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
  CHECK_THAT(events[0].time, WithinAbs(0.5, 1e-10));
  CHECK_THAT(events[0].sq_distance, WithinAbs(0.0, 1e-10));
}

TEST_CASE("Single gatherer collects multiple items in order") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{
      {{3.0, 0.0}, 1.0},  // item 0
      {{7.0, 0.0}, 1.0},  // item 1
      {{5.0, 0.0}, 1.0}   // item 2
  };
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 3);

  // Сортируем события по времени сбора
  std::sort(events.begin(), events.end(),
            [](const auto& a, const auto& b) { return a.time < b.time; });

  // Проверяем, что предметы собираются в правильном порядке и в правильное время
  CHECK(events[0].item_id == 0);
  CHECK_THAT(events[0].time, WithinAbs(0.3, 1e-10));

  CHECK(events[1].item_id == 2);
  CHECK_THAT(events[1].time, WithinAbs(0.5, 1e-10));

  CHECK(events[2].item_id == 1);
  CHECK_THAT(events[2].time, WithinAbs(0.7, 1e-10));
}

TEST_CASE("Multiple gatherers collect items") {
  std::vector<collision_detector::Gatherer> gatherers{{{0.0, 0.0}, {10.0, 0.0}, 1.0},
                                                      {{0.0, 5.0}, {10.0, 5.0}, 1.0}};
  std::vector<collision_detector::Item> items{
      {{5.0, 0.0}, 1.0},  // item 0 — для gatherer 0
      {{5.0, 5.0}, 1.0},  // item 1 — для gatherer 1
      {{3.0, 0.0}, 1.0}   // item 2 — для gatherer 0
  };
  TestProvider provider(items, gatherers);
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 3);

  // Упрощаем проверку через сет ожидаемых значений
  std::set<std::tuple<size_t, size_t, double>> expected = {
      {0, 0, 0.5},  // gatherer 0 -> item 0
      {0, 2, 0.3},  // gatherer 0 -> item 2
      {1, 1, 0.5}   // gatherer 1 -> item 1
  };

  std::set<std::tuple<size_t, size_t, double>> actual;
  for (const auto& ev : events) {
    actual.emplace(ev.gatherer_id, ev.item_id, ev.time);
  }

  for (const auto& exp : expected) {
    CHECK(actual.count(exp) == 1);
  }
}

TEST_CASE("Items not in path are not collected") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{
      {{5.0, 2.0}, 0.5}, {{-5.0, 0.0}, 1.0}, {{15.0, 0.0}, 1.0}};
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.empty());
}

TEST_CASE("Stationary gatherer collects nothing") {
  collision_detector::Gatherer gatherer{{5.0, 0.0}, {5.0, 0.0}, 1.0};
  collision_detector::Item item{{5.0, 0.0}, 1.0};
  TestProvider provider({item}, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.empty());
}

TEST_CASE("Items slightly inside the collection radius are collected") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};  // радиус 1.0
  std::vector<collision_detector::Item> items{
      {{5.0, 0.5}, 0.5},   // расстояние 0.5 < 1.5 (1.0 + 0.5) - должно собраться
      {{5.0, -0.5}, 0.5},  // расстояние 0.5 < 1.5 - должно собраться
      {{5.0, 1.6}, 0.5}    // расстояние 1.6 > 1.5 - не должно собраться
  };
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 2);  // Ожидаем два события

  std::set<size_t> collected_items;
  for (const auto& ev : events) {
    collected_items.insert(ev.item_id);
    CHECK_THAT(ev.time, WithinAbs(0.5, 1e-10));
    CHECK(ev.sq_distance <= 2.25);  // (1.0 + 0.5)^2 = 2.25
  }

  CHECK(collected_items.count(0) == 1);
  CHECK(collected_items.count(1) == 1);
  CHECK(collected_items.count(2) == 0);
}

TEST_CASE("Items outside the collection radius are not collected") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{
      {{5.0, 0.9999}, 1.0},  // В пределах радиуса
      {{5.0, 2.1}, 1.0}      // За пределами радиуса (расстояние = 2.1 > 2.0)
  };
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 1);

  CHECK(events[0].item_id == 0);
  CHECK_THAT(events[0].time, WithinAbs(0.5, 1e-10));
}

TEST_CASE("Multiple events at same time are allowed") {
  collision_detector::Gatherer gatherer{{0.0, 0.0}, {10.0, 0.0}, 1.0};
  std::vector<collision_detector::Item> items{{{5.0, 0.0}, 1.0},      // соберется на 0.5s
                                              {{5.0, 0.9999}, 1.0}};  // тоже соберется на 0.5s
  TestProvider provider(items, {gatherer});
  auto events = collision_detector::FindGatherEvents(provider);

  REQUIRE(events.size() == 2);

  CHECK_THAT(events[0].time, WithinAbs(0.5, 1e-10));
  CHECK_THAT(events[1].time, WithinAbs(0.5, 1e-10));

  std::set<size_t> item_ids{events[0].item_id, events[1].item_id};
  CHECK(item_ids.count(0) == 1);
  CHECK(item_ids.count(1) == 1);
}
