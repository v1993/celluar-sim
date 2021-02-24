#include "Cell.hpp"

uint8_t Cell::regRead(uint8_t reg, const Point& pos) const {
	reg = reg & 0xF;
	switch (reg) {
	case 0:
		return energy;
	case 1:
		return field.lightMap[pos.toArrayIdx()];
	case 2:
		return age / 4;
	default:
		return gRegs[reg - 3];
	};
}

void Cell::regWrite(uint8_t reg, uint8_t val) {
	reg = reg & 0xF;
	// Non-gRegs registers ignore all writes
	if (reg > 2) gRegs[reg - 3] = val;
}

CellActionRequest* Cell::advanceBegin(Point pos) {
	energy_income = 0;
	if (heavyWait) {
		--heavyWait;
		energy_usage = 4;
		return nullptr;
	} else if (hibernate) {
		--hibernate;
		energy_usage = 1;
		return nullptr;
	} else {
		energy_usage = 2;
	}

	hibernate = gRegs[0];
	action_request.type = CellActionRequestType::NONE;

	// Used a lot, so turned into function
	auto regreadline = [this, &pos]() { return regRead(readAndAdvance(), pos); };
	auto setoreg = [this](uint8_t val) { gRegs[1] = val; };
	auto getIR0 = [this]() { return gRegs[2]; };
	auto getIR1 = [this]() { return gRegs[3]; };

	auto cmd = readAndAdvance();
	switch (cmd) {
	case 0: // HIB
		energy_usage = 1;
		return nullptr;
	case 1:   // JMP
	case 2: { // RJMP
		auto len = (cmd == 1) ? readAndAdvance() : regreadline();
		advancePtr(len);
		return nullptr;
	}
	case 3:   // MOVE
	case 4: { // RMOVE
		energy_usage += 5 - std::min(power / 7, 5);
		auto dir = DirectionHelper::create((cmd == 3) ? readAndAdvance() : regreadline());
		if (pos.canApply(dir)) {
			auto posRes = pos.apply(dir);
			SDL_assert_paranoid(posRes);
			action_request.type = CellActionRequestType::MOVE;
			action_request.dir = dir;
			return &action_request;
		} else {
			setoreg(0);
			return nullptr;
		}
	}
	case 5:   // PROBE
	case 6: { // RPROBE
		if (pos.apply(DirectionHelper::create((cmd == 5) ? readAndAdvance() : regreadline()))) {
			auto other = field.cellsField[pos.toArrayIdx()];
			if (other) {
				setoreg(other->getEnergy());
				return nullptr;
			}
		}
		setoreg(0);
		return nullptr;
	}
	case 7:   // ANALYZE
	case 8: { // RANALYZE
		if (pos.apply(DirectionHelper::create((cmd == 7) ? readAndAdvance() : regreadline()))) {
			auto other = field.cellsField[pos.toArrayIdx()];
			if (other) {
				heavyWait = 1;
				const auto& otherProg = other->getProgram();
				size_t diff = 0;
				for (size_t i = 0; i < opline.size(); ++i) {
					diff += opline[i] != otherProg[i];
				}
				setoreg(diff / 2);
				return nullptr;
			}
		};
		setoreg(0);
		return nullptr;
	}
	case 9: { // SET
		auto val = readAndAdvance();
		regWrite(readAndAdvance(), val);
		return nullptr;
	}
	case 10: { // COPY
		auto val = regRead(readAndAdvance(), pos);
		regWrite(readAndAdvance(), val);
		return nullptr;
	}
	case 11: { // RSET
		auto val = readAndAdvance();
		regWrite(getIR0(), val);
		return nullptr;
	}
	case 12: { // ADD
		setoreg(getIR0() + getIR1());
		return nullptr;
	}
	case 13: { // SUB
		setoreg(getIR0() - getIR1());
		return nullptr;
	}
	case 14: { // MUL
		setoreg(getIR0() * getIR1());
		return nullptr;
	}
	case 15: { // INC
		auto reg = readAndAdvance();
		regWrite(reg, regRead(reg, pos) + 1);
		return nullptr;
	}
	case 16: { // DEC
		auto reg = readAndAdvance();
		regWrite(reg, regRead(reg, pos) - 1);
		return nullptr;
	}
	case 17: { // IFZ
		if (regRead(readAndAdvance(), pos) == 0) {
			advancePtr(readAndAdvance());
		} else {
			advancePtr(1);
		}
		return nullptr;
	}
	case 18: { // IFL
		auto regVal = regRead(readAndAdvance(), pos);
		if (regVal < readAndAdvance()) {
			advancePtr(readAndAdvance());
		} else {
			advancePtr(1);
		}
		return nullptr;
	}
	case 19:   // EAT
	case 20: { // REAT
		auto dir = DirectionHelper::create((cmd == 19) ? readAndAdvance() : regreadline());
		if (pos.apply(dir) and field.cellsField[pos.toArrayIdx()]) {
			// Don't try to eat stuff if you can't do it
			energy_usage += 6;
			action_request.type = CellActionRequestType::EAT;
			action_request.dir = dir;
			return &action_request;
		};
		setoreg(0);
		return nullptr;
	}
	case 21:   // ENG
	case 22: { // RENG
		auto enAmount = (cmd == 21) ? readAndAdvance() : regreadline();
		auto dir = DirectionHelper::create((cmd == 21) ? readAndAdvance() : regreadline());
		if (enAmount < energy and pos.apply(dir) and field.cellsField[pos.toArrayIdx()]) {
			energy_usage += enAmount;
			action_request.type = CellActionRequestType::ENERGY;
			action_request.dir = dir;
			action_request.num = enAmount;
			return &action_request;

		}
		setoreg(0);
		return nullptr;
	}
	// TODO: add FIND and FINDE opcodes
	case 25:   // POW
	case 26: { // RPOW
		auto powAmount = (cmd == 25) ? readAndAdvance() : regreadline();
		if (powAmount < energy) {
			energy -= powAmount;
			if ((uint8_t)(power + powAmount) < power) power = 255;
			else power += powAmount;
			// Side effect: use POW(0) to obtain current power
			setoreg(power);
		} else {
			setoreg(0);
		}
		return nullptr;
	}
	case 27:   // POW2E
	case 28: { // RPOW2E
		auto powAmount = (cmd == 27) ? readAndAdvance() : regreadline();
		if (powAmount < power) {
			addEnergy(powAmount / 2); // It's lossy
			power -= powAmount;
			// Side effect: use POW(0) to obtain current power
			setoreg(power);
		} else {
			setoreg(0);
		}
		return nullptr;
	}
	default:
		advancePtr(cmd);
		return nullptr;
	}
}

EndMoveAction Cell::advanceEnd(Point pos, randomGenerator& rng) {
	auto lightEng = field.lightMap[pos.toArrayIdx()] / 32;
	auto powerMod = power / 10;
	if (lightEng > powerMod) addEnergy(lightEng - powerMod);

	if (energy_income < energy_usage) {
		energy_usage -= energy_income;
		// Dead from energy underflow
		if (energy_usage > energy) return EndMoveAction::DIE;
		energy -= energy_usage;
	} else {
		energy_income -= energy_usage;
		if (uint8_t(energy_income + energy) < energy) energy = 255;
		else energy += energy_income;
	}

	++age;
	std::uniform_int_distribution<size_t> dist(age, 1024);
	// Dead from old age
	if (dist(rng) == 1024) return EndMoveAction::DIE;

	switch (action_request.type) {
	case CellActionRequestType::MOVE:
	case CellActionRequestType::ENERGY:
	case CellActionRequestType::EAT:
		gRegs[1] = action_request.res;
		break;
	case CellActionRequestType::NONE:
		break;
	}

	if (energy >= 200) { // Division time!
		energy /= 2;
		return EndMoveAction::DIVIDE;
	}

	return EndMoveAction::NONE;
}

std::unique_ptr<Cell> Cell::fork() const {
	auto n = std::make_unique<Cell>();

	n->energy = energy;
	n->power = power / 10;
	n->opline = opline;

	return n;
}

void Cell::mutate(size_t cnt, randomGenerator& rng) {
	std::uniform_int_distribution<size_t> posDist(0, opline.size());
	std::uniform_int_distribution<size_t> cmdDist(0, 0xFF);
	for (size_t i = 0; i < cnt; ++i) {
		opline[posDist(rng)] = cmdDist(rng);
	}
}
