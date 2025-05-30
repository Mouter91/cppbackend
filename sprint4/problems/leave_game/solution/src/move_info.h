// move_info.h
#pragma once

#include <cmath>
#include <boost/serialization/access.hpp>

using Coord = double;
using Movement = double;

const double EPSILON = 1e-9;

struct MoveInfo {
  enum class Direction { NORTH, SOUTH, WEST, EAST };

  struct Position {
    Coord x, y;
    Position() = default;
    Position(double x, double y) : x(x), y(y) {
    }

    Position& operator=(const Position& other) = default;

    bool operator!=(const Position& other) const {
      return std::abs(x - other.x) > EPSILON || std::abs(y - other.y) > EPSILON;
    }

    bool operator==(const Position& other) const {
      return !(*this != other);
    }

    Position operator-(const Position& other) const {
      return {x - other.x, y - other.y};
    }

    Position operator+(const Position& other) const {
      return {x + other.x, y + other.y};
    }

    Position operator*(double scalar) const {
      return {x * scalar, y * scalar};
    }

    double Dot(const Position& other) const {
      return x * other.x + y * other.y;
    }

   private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
      ar & x & y;
    }
  };

  struct Speed {
    Movement x, y;

   private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
      ar & x & y;
    }
  };

  Position position{0, 0};
  Speed speed{0, 0};
  Direction direction{Direction::NORTH};

 private:
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar & position & speed & direction;
  }
};

struct RoadInfo {
  double min_x, max_x;
  double min_y, max_y;
  bool is_horizontal;

  bool Contains(const MoveInfo::Position pos) const {
    return pos.x >= min_x && pos.x <= max_x && pos.y >= min_y && pos.y <= max_y;
  }
};
