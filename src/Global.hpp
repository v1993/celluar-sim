#pragma once

#include <cstddef>
#include <optional>
#include <memory>
#include <random>
#include <unordered_map>
#include <SDL_assert.h>

// We don't really need good RNG, speed is much more important
using randomGenerator = std::minstd_rand;

struct GlobalSettingsType {
	size_t fieldH;
	size_t fieldW;
};

extern GlobalSettingsType global;

enum class Direction : uint8_t {
	UPLEFT		= 0,
	UP			= 1,
	UPRIGHT		= 2,
	RIGHT		= 3,
	DOWNRIGHT	= 4,
	DOWN		= 5,
	DOWNLEFT	= 6,
	LEFT		= 7
};

constexpr uint8_t DirectionMax = 8;

namespace DirectionHelper {
	inline Direction create(uint8_t num) {
		return Direction(num & 0x7u);
	};
	inline bool isleft(const Direction& dir) {return uint8_t(dir) < 2 or uint8_t(dir) == 7; };
	inline bool isup(const Direction& dir) {return uint8_t(dir) < 3; };
	inline bool isright(const Direction& dir) {return uint8_t(dir) > 2 and uint8_t(dir) < 5; };
	inline bool isdown(const Direction& dir) {return uint8_t(dir) > 4 and uint8_t(dir) < 7; };
};

struct Point {
	size_t y;
	size_t x;

	Point(size_t y_, size_t x_): y(y_), x(x_) {};

	bool operator==(const Point& other) const {return other.y == y and other.x == x; };

	void checkBounds() { SDL_assert_paranoid(y < global.fieldH and x < global.fieldW); };

	bool canApply(Direction dir) {
		switch (dir) {
		case Direction::UPLEFT:
			return !(y == 0 or x == 0);
		case Direction::UP:
			return !(y == 0);
		case Direction::UPRIGHT:
			return !(y == 0 or x == global.fieldW - 1);
		case Direction::RIGHT:
			return !(x == global.fieldW - 1);
		case Direction::DOWNRIGHT:
			return !(y == global.fieldH - 1 or x == global.fieldW - 1);
		case Direction::DOWN:
			return !(y == global.fieldH - 1);
		case Direction::DOWNLEFT:
			return !(y == global.fieldH - 1 or x == 0);
		case Direction::LEFT:
			return !(x == 0);
		};
		abort(); // Clearly something is terribly off
	};

	bool apply(Direction dir) {
		switch (dir) {
		case Direction::UPLEFT:
			if (y == 0 or x == 0) return false;
			--y;
			--x;
			break;
		case Direction::UP:
			if (y == 0) return false;
			--y;
			break;
		case Direction::UPRIGHT:
			if (y == 0 or x == global.fieldW - 1) return false;
			--y;
			++x;
			break;
		case Direction::RIGHT:
			if (x == global.fieldW - 1) return false;
			++x;
			break;
		case Direction::DOWNRIGHT:
			if (y == global.fieldH - 1 or x == global.fieldW - 1) return false;
			++y;
			++x;
			break;
		case Direction::DOWN:
			if (y == global.fieldH - 1) return false;
			++y;
			break;
		case Direction::DOWNLEFT:
			if (y == global.fieldH - 1 or x == 0) return false;
			++y;
			--x;
			break;
		case Direction::LEFT:
			if (x == 0) return false;
			--x;
			break;
		};
		return true;
	};

	void applyWithoutChecks(Direction dir) {
		switch (dir) {
		case Direction::UPLEFT:
			--y;
			--x;
			break;
		case Direction::UP:
			--y;
			break;
		case Direction::UPRIGHT:
			--y;
			++x;
			break;
		case Direction::RIGHT:
			++x;
			break;
		case Direction::DOWNRIGHT:
			++y;
			++x;
			break;
		case Direction::DOWN:
			++y;
			break;
		case Direction::DOWNLEFT:
			++y;
			--x;
			break;
		case Direction::LEFT:
			--x;
			break;
		};
	};

	std::optional<Point> applyNew(Direction dir) {
		Point other = *this;
		if (other.apply(dir))
			return other;
		else
			return std::nullopt;
	};
};

struct Point_hash {
	std::size_t operator()(const Point &obj) const {
		return static_cast<size_t>(obj.y ^ obj.x);
	}
};

class Cell;

struct GlobalFieldType {
	std::unordered_map<Point, std::shared_ptr<Cell>, Point_hash> cells;
	// Note: light map is stored column-by-column to optimize memory access
	std::unique_ptr<uint8_t[]> lightMap;
};

extern GlobalFieldType field;
