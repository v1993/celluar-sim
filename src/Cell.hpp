#pragma once

#include <vector>
#include <array>

#include "Global.hpp"

enum class CellActionRequestType {
	NONE,
	MOVE,
	ENERGY,
	EAT
};

enum class EndMoveAction {
	NONE,
	DIVIDE,
	DIE
};

struct CellActionRequest {
	CellActionRequestType type;
	Direction dir;
	uint8_t num;
	uint8_t res;
};

class Cell {
	public:
		// Object's lifecycle
		Cell() = default;
		explicit Cell(const Cell&) = delete;
		Cell(Cell&& o):
			execPtr(std::move(o.execPtr)),
			age(std::move(o.age)),
			energy(std::move(o.energy)),

			opline(std::move(o.opline)),
			gRegs(std::move(o.gRegs)),

			heavyWait(std::move(o.heavyWait)),
			hibernate(std::move(o.hibernate)),

			energy_income(std::move(o.energy_income)),
			energy_usage(std::move(o.energy_usage)),

			action_request(std::move(o.action_request))
		{};

		// Main functions, they can be called in threaded context
		// Called at the beginning of handling cycle
		CellActionRequest* advanceBegin(Point pos);
		// Called at the end of it
		EndMoveAction advanceEnd(Point pos, randomGenerator& rng);

		// Useful for creating new cells
		// Doesn't trigger mutation by itself!
		std::shared_ptr<Cell> fork() const;

		// Used to access cell's internals
		void addEnergy(uint8_t eeng) {
			if ((uint8_t)(eeng + energy_income) < energy_income) energy_income = 255;
			else energy_income += eeng;
		};
		uint8_t getEnergy() { return energy; };
		auto const& getProgram() {
			return opline;
		};
		// Needed if map was touched
		CellActionRequest* getActionPtr() {return &action_request;};

		// Rarely called but VERY important function
		// Change up to cnt bytes in cell's program
		void mutate(size_t cnt, randomGenerator& rng);
	private:
		size_t execPtr = 0;
		size_t age = 0;
		size_t energy = 100;

		std::array<uint8_t, 127> opline = {0};
		std::array<uint8_t, 13> gRegs = {0}; // Registers that don't require special reads.

		size_t heavyWait = 0;
		size_t hibernate = 0;

		// Reset every tick
		uint8_t energy_income = 0;
		uint8_t energy_usage = 0;
		CellActionRequest action_request;

		uint8_t regRead(uint8_t reg, const Point& pos) const;
		void regWrite(uint8_t reg, uint8_t val);

		uint8_t read(size_t addr) const {
			return opline[addr % opline.size()];
		};
		void advancePtr(size_t s) {
			execPtr = (execPtr + s) % opline.size();
		};
		[[nodiscard]] uint8_t readAndAdvance() {
			auto val = opline[execPtr];
			execPtr = (execPtr + 1) % opline.size();
			return val;
		};
};
