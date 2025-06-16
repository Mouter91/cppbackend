#pragma once

using Coord = double;
using Movement = double;

struct MoveInfo {
    enum class Direction {NORTH, SOUTH, WEST, EAST};
    struct Point {
        Coord x, y;
    };
    struct Speed {
        Movement x, y;
    };

    Point position{0, 0};
    Speed speed{0, 0};
    Direction direction{Direction::NORTH};
};
