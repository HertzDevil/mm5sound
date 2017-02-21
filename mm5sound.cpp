#include "mm5sound.h"
#include "mm5sndrom.h"
#include "mm5constants.h"
#include <stdexcept>
#include "chain_int.h"



namespace MM5Sound {

namespace {
	template <class T>
	bool lsr(T &x) {
		bool C = (x & 0x01) != 0;
		x >>= 1;
		return C;
	}
	template <class T>
	bool asl(T &x) {
		bool C = (x & 0x80) != 0;
		x <<= 1;
		return C;
	}
	bool adc(uint8_t &x, uint8_t y) {
		uint16_t z = x + y;
		x = z & 0xFF;
		return z > 0xFFu;
	}
	bool sbc(uint8_t &x, uint8_t y) {
		uint16_t z = x - y;
		x = z & 0xFF;
		return z > 0xFFu;
	}
}



const uint8_t CEngine::NOTE_LENGTH[] = {2, 4, 8, 16, 32, 64, 128}; // $8915
const uint8_t CEngine::NOTE_LENGTH_DOTTED[] = {3, 6, 12, 24, 48, 96, 192}; // $891C
const uint8_t CEngine::OCTAVE_TABLE[] = { // $8923
	0, 12, 24, 36, 48, 60, 72, 84,
	       24, 36, 48, 60, 72, 84, 96, 108,
};
const uint8_t CEngine::ENV_RATE_TABLE[] = { // $8933
	  0,   1,   2,   3,   4,   5,   6,   7,
	  8,   9,  10,  11,  12,  14,  15,  16,
	 18,  19,  20,  22,  24,  27,  30,  35,
	 40,  48,  60,  80, 126, 127, 254, 255,
};
// $895B
const uint8_t CEngine::TRACK_COUNT = 0x4C; // $8A40
const uint16_t CEngine::SONG_TABLE = 0x8A41;
const uint16_t CEngine::INSTRUMENT_TABLE = 0x8ADB;



void CEngine::CallINIT(uint8_t track, uint8_t region) {
	// $8003 - $8005
	A_ = track;
	X_ = region;
	InitDriver();
}

void CEngine::CallPLAY() {
	// $8000 - $8002
	StepDriver();
}

void CEngine::WriteCallback(uint16_t adr, uint8_t value) {
}

uint8_t CEngine::ReadCallback(uint16_t adr) const {
	return MM5ROM.at(adr - 0x8000);
}



void CEngine::Multiply() {
	// $8006 - $8022
	// destroys A
	chain(mem_[0xC1], mem_[0xC2]) = mem_[0xC1] * mem_[0xC4];
	Y_ = 0;
}

void CEngine::SwitchDispatch(FuncList_t funcs) {
	// $8023 - $8039
	// destroys A_
	(this->*(*(funcs.begin() + A_)))();
}

uint8_t CEngine::ReadROM(uint16_t adr) {
	// $803A - $806B
	chain(mem_[0xC2], mem_[0xC1]) = adr;
	A_ = adr >> 8;
	Y_ = 0;
	return ReadCallback(adr); // bankswitching code omitted
}

void CEngine::StepDriver() {
	// $806C - $80D7
	if (mem_[0xC0] & 0x01)
		return;
	if (sfx_currentPtr)
		Func8252();

	chain(mem_[0xC7], mem_[0xC8]) = chain(mem_[0xC9], mem_[0xCA]) + mem_[0xC8];

	auto A1 = mem_[0xCF];
	for (X_ = 0x03; X_ < 0x80; --X_) {
		if (lsr(mem_[0xCF])) {
			mem_[0xCF] |= 0x80;
			Func82DE();
		}
		if (!(mem_[0xC0] & 0x02)) {
			uint8_t X1 = X_;
			A_ = X_;
			ProcessChannel();
			X_ = X1;
		}
	}
	mem_[0xCF] = A1;

	mem_[0xC0] &= 0xFE;
	A_ = mem_[0xCC] & 0x7F;
	if (!A_)
		return;
	mem_[0xC1] = Y_ = 0;
	chain(mem_[0xC1], A_) <<= 4;
	uint8_t C = 0;
	chain(C, mem_[0xCD], mem_[0xC0]) += chain(mem_[0xC1], A_);
	if (C) {
		mem_[0xCC] &= 0x80;
		mem_[0xCD] = 0xFF;
	}
	A_ = mem_[0xCD];
}

void CEngine::SilenceChannel() {
	// $80D8 - $80EB
	Y_ = ((X_ & 0x03) ^ 0x03) << 2;
	WriteCallback(0x4000 + Y_, A_ = (Y_ == 0x08 ? 0 : 0x30));
}

void CEngine::Write2A03() {
	// $80EC - $80FD
	mem_[0xC4] = Y_;
	Y_ |= (((X_ & 0x03) ^ 0x03) << 2);
	WriteCallback(0x4000 + Y_, A_);
}

void CEngine::InitDriver() {
	// $80FE - $8105
	++mem_[0xC0];
	Func8106();
	--mem_[0xC0];
}

void CEngine::Func8106() {
	// $8106 - $8117
	if (A_ < 0xF0u) {
		while (A_ >= TRACK_COUNT)
			A_ -= TRACK_COUNT;
		Func8118();
	}
	else {
		// $81AE - $81C4
		mem_[0xC3] = Y_;
		A_ &= 0x07;
		switch (A_) {
		case 0: L81C5(); break;
		case 1: L81C8(); break;
		case 2: Func81E4(); break;
		case 3: L821E(); break;
		case 4: L8226(); break;
		case 5: L822D(); break;
		case 6: L8234(); break;
		case 7: L824A(); break;
		}
	}
}

void CEngine::Func8118() {
	// $8118 - $816E
	X_ = A_ << 1;
	uint16_t adr = ReadCallback(SONG_TABLE + 3 + X_) | (ReadCallback(SONG_TABLE + 2 + X_) << 8);
	Y_ = adr & 0xFF;
	if (!adr)
		return;
	Y_ = A_ = ReadROM(adr);
	if (A_) {
		++X_;
		mem_[0xC4] = A_;
		A_ &= 0x7F;
		if (A_ < mem_[0xCE])
			return;
		mem_[0xCE] = A_;
		if (!A_ && mem_[0xD6] >= 0x80u && mem_[0xC4] < 0x80u)
			mem_[0xD7] = 0;
		mem_[0xD6] = 0;
		if (asl(mem_[0xC4])) {
			mem_[0xD6] = 0x80;
			mem_[0xD7] = X_;
		}
		sfx_currentPtr = ++chain(mem_[0xC2], mem_[0xC1]);
		mem_[0xD2] = mem_[0xD3] = mem_[0xD4] = mem_[0xD5] = 0;
		for (Y_ = 0x27; Y_ < 0x80u; --Y_)
			mem_[0x700 + Y_] = 0;
		A_ = 0;
	}
	else {
		// $816F - $81AD
		chain(mem_[0xC9], mem_[0xCA]) = 0x0199;
		mem_[0xC8] = var_globalTrsp = mem_[0xCC] = mem_[0xCD] = 0;
		for (X_ = 0x53; X_ < 0x80u; --X_)
			mem_[0x728 + X_] = 0;
		for (X_ = 0x03; X_ < 0x80u; --X_) {
			auto adr = chain(mem_[0xC2], mem_[0xC1]);
			mem_[0x754 + X_] = ReadROM(++adr);
			mem_[0x750 + X_] = ReadROM(++adr);
		}
		Func81F1();
	}
}



void CEngine::L81C5() {
	// $81C5 - $81C7
	Func81E4();
	L81C8();
}

void CEngine::L81C8() {
	// $81C8 - $81D3
	A_ = sfx_currentPtr = mem_[0xCE] = mem_[0xD7] = mem_[0xD8] = 0;
	Func81D4();
}

void CEngine::Func81D4() {
	// $81D4 - $81E3
	A_ = mem_[0xCF];
	if (!A_)
		return;
	A_ ^= 0x0F;
	mem_[0xCF] = A_;
	Func81F1();
	A_ = 0;
	mem_[0xCF] = A_;
}

void CEngine::Func81E4() {
	// $81E4 - $81F0
	A_ = 0;
	for (X_ = 0x03; X_ < 0x80u; --X_)
		mem_[0x754 + X_] = mem_[0x750 + X_] = 0;
	Func81F1();
}

void CEngine::Func81F1() {
	// $81F1 - $821D
	uint8_t A1 = mem_[0xCF];
	for (X_ = 3; X_ < 0x80u; --X_) {
		if (!lsr(mem_[0xCF])) {
			SilenceChannel();
			if (mem_[0x754 + X_] | mem_[0x750 + X_])
				A_ = mem_[0x77C + X_] = 0xFF;
		}
	}
	mem_[0xCF] = A1;
	WriteCallback(0x4001, 0x08);
	WriteCallback(0x4005, 0x08);
	WriteCallback(0x4015, A_ = 0x0F);
}

void CEngine::L821E() {
	// $821E - $8225
	mem_[0xC0] |= 0x02;
	Func81F1();
}

void CEngine::L8226() {
	// $8226 - $822C
	A_ = (mem_[0xC0] &= ~0x02);
}

void CEngine::L822D() {
	// $822D - $8233
	mem_[0xC3] <<= 1;
	if (mem_[0xC3])
		mem_[0xC3] = 0x80 | (mem_[0xC3] >> 1);
	L8234();
}

void CEngine::L8234() {
	// $8234 - $8249
	auto A0 = mem_[0xC0] & 0x0F;
	mem_[0xC0] = A0;
	mem_[0xCC] = Y_ = mem_[0xC3];
	if (Y_) {
		Y_ = 0xFF;
		if (mem_[0xCD] == 0xFF)
			mem_[0xCD] = Y_ = 0;
	}
	else
		mem_[0xCD] = 0;
	A_ = A0;
}

void CEngine::L824A() {
	// $824A - $8251
	mem_[0xD8] = -mem_[0xC3];
	A_ = mem_[0xD8];
}

void CEngine::Func8252() {
	// $8252 - $82A5
	if (mem_[0xD3]) {
		A_ = mem_[0xD3]--;
		--mem_[0xD5];
		return;
	}

	A_ = 0;
	bool C = false; // php/plp
	while (true) {
		GetSFXData();
		mem_[0xC4] = A_;
		if (asl(A_)) {
			mem_[0xCE] = Y_;
			A_ = mem_[0xD7];
			if (!lsr(A_)) {
				L81C8();
				return;
			}
			Func8118();
			continue;
		}
		if (!lsr(mem_[0xC4]))
			break;
		GetSFXData();
		A_ <<= 1;
		if (A_) {
			C = mem_[0xD6] >= 0x80u;
			auto temp = mem_[0xD6];
			mem_[0xD6] <<= 1;
			if (A_ == mem_[0xD6]) {
				A_ = Y_ >> 1;
				if (C)
					A_ |= 0x80;
				mem_[0xD6] = A_;
				sfx_currentPtr += 2;
				break;
			}
			mem_[0xD6] = temp + 1;
		}
		GetSFXData();
		X_ = A_;
		GetSFXData();
		sfx_currentPtr = chain(X_, A_);
		if (A_)
			continue;
		A_ = Y_ >> 1;
		if (C)
			A_ |= 0x80;
		mem_[0xD6] = A_;
		sfx_currentPtr += 2;
		break;
	}

	// $82A6 - $82DD
	if (lsr(mem_[0xC4])) {
		GetSFXData();
		mem_[0xD4] = A_;
	}
	if (lsr(mem_[0xC4])) {
		GetSFXData();
		mem_[0xD2] = A_;
	}
	GetSFXData();
	mem_[0xC1] = mem_[0xD3] = A_;
	mem_[0xC4] = mem_[0xD4];
	Multiply();
	Y_ = mem_[0xC1];
	mem_[0xD5] = Y_ + 1;
	++mem_[0xC0];
	GetSFXData();
	auto A1 = A_;
	A_ ^= mem_[0xCF];
	if (A_) {
		mem_[0xCF] = A_;
		Func81D4();
	}
	mem_[0xCF] = A1;
}

void CEngine::Func82DE() {
	// $82DE - $8309
	Y_ = mem_[0x700 + X_];
	if (Y_)
		Func8684();
	A_ = mem_[0xC0];
	if (!lsr(A_)) {
		Func86BA();
		A_ = mem_[0xD3];
		if (!A_)
			return;
		if (X_ != 0x01) {
			A_ = mem_[0xD5];
			if (A_)
				return;
		}
		else {
			if (--mem_[0x710 + X_])
				return;
		}
		A_ = mem_[0x704 + X_] & 0x04;
		if (A_)
			return;
		Func85A3();
		return;
	}

	// $830A - $8325
	A_ = mem_[0xC4] = 0;
	GetSFXData();
	while (true) {
		if (lsr(A_)) {
			auto A1 = A_;
			GetSFXData();
			mem_[0xC3] = A_;
			A_ = mem_[0xC4];
			Func8326();
			A_ = A1;
		}
		if (!A_)
			break;
		++mem_[0xC4];
		if (!mem_[0xC4]) {
			Func8326();
			return;
		}
	}

	// $8333 - $8385
	GetSFXData();
	Y_ = A_;
	if (!A_) {
		mem_[0x710 + X_] = A_;
		mem_[0x704 + X_] &= 0xF8;
		mem_[0x704 + X_] |= 0x04;
		SilenceChannel();
		return;
	}
	mem_[0x704 + X_] |= 0x20;
	mem_[0x71C + X_] = (mem_[0x718 + X_] >= 0x80u) ? 0x54 : 0x0A;
	A_ = Y_;
	if (A_ >= 0x80u) {
		if (X_ == 0x01)
			Func85AE();
		Func8644();
		return;
	}
	Func85AE();
	mem_[0x77C + X_] = 0xFF;
	--Y_;
	A_ = X_;
	if (!A_) {
		mem_[0xC3] = 0;
		A_ = Y_ ^ 0x0F;
		Func8636();
	}
	else {
		A_ = Y_ + 2;
		Func85DE();
	}
}

void CEngine::Func8326() {
	// $8326 - $8332
	switch (A_) {
	case 0x00: CmdEnvelope(); break;
	case 0x01: CmdDuty(); break;
	case 0x02: CmdVolume(); break;
	case 0x03: CmdPortamento(); break;
	case 0x04: CmdDetune(); break;
	default: throw std::runtime_error {"Unknown SFX command"};
	}
}

void CEngine::GetSFXData() {
	// $8386 - $8392
	A_ = ReadROM(sfx_currentPtr++);
}

void CEngine::ProcessChannel() {
	// $8393 - 83CC
	X_ |= 0x28;
	if (!(mem_[0x728 + X_] | mem_[0x72C + X_]))
		return;
	A_ = mem_[0x738 + X_];
	if (mem_[0x738 + X_]) {
		if (Y_ = mem_[0x700 + X_]) {
			Func8684();
			Func86BA();
		}
		if (mem_[0x740 + X_] <= mem_[0xC7])
			Func85A3();
		mem_[0x740 + X_] -= mem_[0xC7];
		if (mem_[0x738 + X_] > mem_[0xC7]) {
			A_ = mem_[0x738 + X_] -= mem_[0xC7];
			return;
		}
		mem_[0x738 + X_] -= mem_[0xC7];
	}

	// $83CD - $83D9
	while (true) {
		GetTrackData();
		const auto cmd = A_;
		if (cmd >= 0x20u)
			break;
		CommandDispatch();
		if (cmd == 0x17)
			return;
	}

	// $83DA - $840D
	const auto A1 = A_;
	Y_ = (A1 >> 5) - 1;
	A_ = mem_[0x730 + X_] << 2;
	if (A_ >= 0x80u)
		A_ = NOTE_LENGTH[Y_];
	else {
		bool C = (A_ & 0x40) != 0;
		A_ = NOTE_LENGTH_DOTTED[Y_];
		if (C) {
			mem_[0xC3] = A_;
			mem_[0x730 + X_] &= 0xEF;
			A_ += A_ >> 1;
		}
	}
	A_ += mem_[0x738 + X_];
	mem_[0x738 + X_] = A_;
	Y_ = A_;

	// $840E - $842F
	A_ = (A1 & 0x1F);
	if (!A_) {
		Func85A3();
		mem_[0x740 + X_] = A_ = 0xFF;
		return;
	}
	const auto A2 = A_;
	mem_[0xC4] = Y_;
	mem_[0xC1] = mem_[0x73C + X_];
	Multiply();
	A_ = mem_[0xC1] ? mem_[0xC1] : 1;
	mem_[0x740 + X_] = A_;
	A_ = A2;
	Y_ = A_ - 1;

	// $8430 - $847D
	if (mem_[0x730 + X_] < 0x80u) {
		Func85AE();
		A_ = mem_[0xCF];
		if (mem_[0xCF] < 0x80u) {
			mem_[0xC3] = Y_;
			mem_[0x77C + (X_ & 0x03)] = A_ = 0xFF;
		}
	}
	if (mem_[0x730 + X_] < 0x80u || mem_[0x718 + X_])
		if (!(X_ & 0x03)) {
			mem_[0xC3] = 0;
			A_ = (Y_ & 0x0F) ^ 0x0F;
			Func8636();
		}
		else {
			mem_[0xC3] = Y_;
			Y_ = mem_[0x730 + X_] & 0x0F;
			A_ = OCTAVE_TABLE[Y_] + mem_[0xC3] + var_globalTrsp + mem_[0x734 + X_];
			Func85DE();
		}
	else
		Func8644();

	// $847E - $8496
	Y_ = mem_[0x730 + X_];
	mem_[0xC4] = (mem_[0x730 + X_] & 0x40) << 1;
	A_ = mem_[0xC4] | (mem_[0x730 + X_] & 0x7F);
	mem_[0x730 + X_] = A_;
	if (A_ >= 0x80u)
		mem_[0x740 + X_] = A_ = 0xFF;
}

void CEngine::CommandDispatch() {
	// $8497 - $84D8
	if (A_ >= 0x04u) {
		mem_[0xC4] = A_;
		GetTrackData();
		mem_[0xC3] = A_;
		A_ = mem_[0xC4];
	}
	switch (A_) {
	case 0x00: CmdTriplet(); break;
	case 0x01: CmdTie(); break;
	case 0x02: CmdDot(); break;
	case 0x03: Cmd15va(); break;
	case 0x04: CmdFlags(); break;
	case 0x05: CmdTempo(); break;
	case 0x06: CmdGate(); break;
	case 0x07: CmdVolume(); break;
	case 0x08: CmdEnvelope(); break;
	case 0x09: CmdOctave(); break;
	case 0x0A: CmdGlobalTrsp(); break;
	case 0x0B: CmdTranspose(); break;
	case 0x0C: CmdDetune(); break;
	case 0x0D: CmdPortamento(); break;
	case 0x0E: case 0x12: CmdLoop1(); break;
	case 0x0F: case 0x13: CmdLoop2(); break;
	case 0x10: case 0x14: CmdLoop3(); break;
	case 0x11: case 0x15: CmdLoop4(); break;
	case 0x16: CmdGoto(); break;
	case 0x17: CmdHalt(); break;
	case 0x18: CmdDuty(); break;
	default: throw std::runtime_error {"Unknown command"};
	}
}

void CEngine::CmdTriplet() {
	// $84D9 - $84DC
	A_ = mem_[0x730 + X_] ^= 0x20;
}

void CEngine::CmdTie() {
	// $84DD - $84E0
	A_ = mem_[0x730 + X_] ^= 0x40;
}

void CEngine::CmdDot() {
	// $84E1 - $84E7
	A_ = mem_[0x730 + X_] |= 0x10;
}

void CEngine::Cmd15va() {
	// $84E8 - $84F0
	A_ = mem_[0x730 + X_] ^= 0x08;
}

void CEngine::CmdFlags() {
	// $8575 - $857F
	A_ = mem_[0x730 + X_] = ((mem_[0x730 + X_] & 0x97) | mem_[0xC3]);
}

void CEngine::CmdTempo() {
	// $84F1 - $84FE
	A_ = mem_[0xC8] = 0;
	GetTrackData();
	chain(mem_[0xC9], mem_[0xCA]) = chain(mem_[0xC3], A_);
	Y_ = mem_[0xC9];
}

void CEngine::CmdGate() {
	// $84FF - $8504
	A_ = mem_[0x73C + X_] = mem_[0xC3];
}

void CEngine::CmdVolume() {
	// $865A - $866E
	if (X_ != 0x01 || !mem_[0xC3])
		mem_[0x70C + X_] = ((mem_[0x70C + X_] & 0xC0) | mem_[0xC3] | 0x30);
	else
		mem_[0x70C + X_] = mem_[0xC3];
	A_ = mem_[0x70C + X_];
}

void CEngine::CmdEnvelope() {
	// $866F - $8683
	A_ = ++mem_[0xC3];
	if (A_ != mem_[0x700 + X_]) {
		mem_[0x700 + X_] = A_;
		Y_ = A_;
		mem_[0x704 + X_] |= 0x08;
		Func8684();
	}
}

void CEngine::CmdOctave() {
	// $8505 - $850F
	A_ = mem_[0x730 + X_] = ((mem_[0x730 + X_] & 0xF8) | mem_[0xC3]);
}

void CEngine::CmdGlobalTrsp() {
	// $8510 - $8514
	A_ = var_globalTrsp = mem_[0xC3];
}

void CEngine::CmdTranspose() {
	// $8515 - $851A
	A_ = mem_[0x734 + X_] = mem_[0xC3];
}

void CEngine::CmdDetune() {
	// $86A1 - $86A6
	A_ = mem_[0x714 + X_] = mem_[0xC3];
}

void CEngine::CmdPortamento() {
	// $86A7 - $86AC
	A_ = mem_[0x718 + X_] = mem_[0xC3];
}

void CEngine::CmdLoop1() {
	// $851B - $851E
	A_ = 0x00;
	CmdLoopImpl();
}

void CEngine::CmdLoop2() {
	// $851F - $8522
	A_ = 0x04;
	CmdLoopImpl();
}

void CEngine::CmdLoop3() {
	// $8523 - $8526
	A_ = 0x08;
	CmdLoopImpl();
}

void CEngine::CmdLoop4() {
	// $8527 - $8528
	A_ = 0x0C;
	CmdLoopImpl();
}

void CEngine::CmdLoopImpl() {
	// $8529 - $8559
	mem_[0xC2] = A_;
	Y_ = A_ + X_;
	if (mem_[0xC4] < 0x12u) {
		A_ = mem_[0x744 + Y_];
		mem_[0x744 + Y_] = A_ ? (A_ - 1) : mem_[0xC3];
		if (!mem_[0x744 + Y_]) {
			// $8566 - $8574
			chain(mem_[0x72C + X_], mem_[0x728 + X_]) += 2;
			A_ = mem_[0x728 + X_];
			return;
		}
	}
	else {
		A_ = mem_[0x744 + Y_] - 1;
		if (A_) {
			chain(mem_[0x72C + X_], mem_[0x728 + X_]) += 2;
			A_ = mem_[0x728 + X_];
			return;
		}
		mem_[0x744 + Y_] = A_;
		CmdFlags();
	}

	GetTrackData();
	mem_[0xC3] = A_;
	CmdGoto();
}

void CEngine::CmdGoto() {
	// $855A - $8565
	GetTrackData();
	chain(mem_[0x72C + X_], mem_[0x728 + X_]) = chain(mem_[0xC3], A_);
	A_ = mem_[0xC3];
}

void CEngine::CmdHalt() {
	// $8580 - $8591
	chain(mem_[0x72C + X_], mem_[0x728 + X_]) = 0;
	A_ = mem_[0xCF];
	if (mem_[0xCF] < 0x80u)
		SilenceChannel();
}

void CEngine::CmdDuty() {
	// $86AD - $86B9
	A_ = mem_[0x70C + X_] = ((mem_[0x70C + X_] & 0x0F) | mem_[0xC3] | 0x30);
}

void CEngine::GetTrackData() {
	// $8592 - $85A2
	auto adr = chain(mem_[0x72C + X_], mem_[0x728 + X_]);
	A_ = ReadROM(adr++);
}

void CEngine::Func85A3() {
	// $85A3 - $85AD
	mem_[0x704 + X_] &= 0xF8;
	mem_[0x704 + X_] |= 0x03;
	A_ = mem_[0x704 + X_];
}

void CEngine::Func85AE() {
	// $85AE - $85DD
	auto Y1 = Y_;
	Y_ = 0;
	A_ = mem_[0x704 + X_] &= 0xF8;

	if (X_ == 0x01) {
		mem_[0xC1] = mem_[0xD3];
		mem_[0xC4] = mem_[0x70C + X_];
		Multiply();
		Y_ = mem_[0xC1] + 1;
		++mem_[0x704 + X_];
		++mem_[0x704 + X_];
	}
	else if (X_ == 0x29) {
		++Y_;
		++mem_[0x704 + X_];
		++mem_[0x704 + X_];
	}

	mem_[0x710 + X_] = Y_;
	A_ = Y_ = Y1;
}

void CEngine::Func85DE() {
	// $85DE - $8629
	if (A_ >= 0x60u)
		A_ = 0x5F;
	mem_[0xC3] = A_ + 1;
	if (X_ >= 0x28u) {
		if (mem_[0x71C + X_]) {
			if (mem_[0x71C + X_] == mem_[0xC3]) {
				if (mem_[0x730 + X_] >= 0x80u) {
					Func8644();
					return;
				}
				else {
					mem_[0x704 + X_] &= 0xDF;
					A_ = mem_[0x71C + X_] = mem_[0xC3];
				}
			}
			else if (mem_[0x718 + X_]) {
				if (mem_[0x71C + X_] >= mem_[0xC3])
					mem_[0x718 + X_] &= 0x7F;
				else
					mem_[0x718 + X_] |= 0x80;
				mem_[0x704 + X_] |= 0x20;
				A_ = mem_[0xC3];
				Y_ = mem_[0xC3] = mem_[0x71C + X_];
				if (!mem_[0xC3]) {
					mem_[0x704 + X_] &= 0xDF;
					A_ = mem_[0x71C + X_] = mem_[0xC3];
				}
				else
					mem_[0x71C + X_] = A_;
			}
			else {
				mem_[0x704 + X_] &= 0xDF;
				A_ = mem_[0x71C + X_] = mem_[0xC3];
			}
		}
		else {
			mem_[0x704 + X_] &= 0xDF;
			A_ = mem_[0x71C + X_] = mem_[0xC3];
		}
	}

	// $862A - $8635
	mem_[0xC3] <<= 1;
	Y_ = mem_[0xC3];
	mem_[0xC3] = ReadCallback(0x8959 + Y_);
	A_ = ReadCallback(0x895A + Y_);
	Func8636();
}

void CEngine::Func8636() {
	// $8636 - $8643
	mem_[0x724 + X_] = A_;
	mem_[0x720 + X_] = mem_[0xC3];
	Y_ = 0x04;
	A_ = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + Y_);
	if (A_ >= 0x80u) {
		mem_[0x708 + X_] = 0;
		A_ = mem_[0x704 + X_] &= 0x37;
	}
	else
		Func8644();
}

void CEngine::Func8644() {
	// $8644 - $8659
	if (mem_[0x704 + X_] & 0x08) {
		mem_[0x708 + X_] = 0;
		A_ = mem_[0x704 + X_] &= 0x37;
	}
}

void CEngine::Func8684() {
	// $8684 - $86A0
	mem_[0xC3] = 0;
	A_ = --Y_;
	chain(mem_[0xC3], A_) <<= 3;
	chain(mem_[0xC6], mem_[0xC5]) = INSTRUMENT_TABLE + chain(mem_[0xC3], A_);
	A_ = mem_[0xC6];
}

void CEngine::Func86BA() {
	// $86BA - $86D0
	mem_[0xC4] = mem_[0x710 + X_];
	A_ = mem_[0x704 + X_] & 0x07;
	switch (A_) {
	case 0: L86D1(); break;
	case 1: L86E6(); break;
	case 2: break;
	case 3: L8702(); break;
	case 4: return;
	default: throw std::runtime_error {"unknown envelope state"};
	}
	L8720(); // merged
}

void CEngine::L86D1() {
	// $86D1 - $86E5
	Y_ = ReadCallback(chain(mem_[0xC6], mem_[0xC5]));
	A_ = mem_[0xC4];
	if (A_ + ENV_RATE_TABLE[Y_] >= 0xF0u) {
		A_ = 0xF0;
		++mem_[0x704 + X_];
	}
	else
		A_ += ENV_RATE_TABLE[Y_];
	mem_[0x710 + X_] = A_;
}

void CEngine::L86E6() {
	// $86E6 - $8701
	Y_ = 1;
	const auto sustainLv = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 2);
	A_ = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 1);
	if (!A_) {
		Y_ = 2;
		A_ = mem_[0x710 + X_] = sustainLv;
		++mem_[0x704 + X_];
	}
	else {
		Y_ = A_;
		A_ = mem_[0xC4] - ENV_RATE_TABLE[Y_];
		if (mem_[0xC4] < ENV_RATE_TABLE[Y_]) {
			Y_ = 2;
			A_ = mem_[0x710 + X_] = sustainLv;
			++mem_[0x704 + X_];
		}
		else {
			Y_ = 2;
			if (A_ < sustainLv) {
				A_ = sustainLv;
				++mem_[0x704 + X_];
			}
			mem_[0x710 + X_] = A_;
		}
	}
}

void CEngine::L8702() {
	// $8702 - $871F
	if ((X_ & 0x03) == 0x01) {
		++mem_[0x704 + X_];
		A_ = mem_[0x710 + X_] = 0;
	}
	else {
		Y_ = 3;
		A_ = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 3);
		if (A_) {
			Y_ = A_;
			A_ = mem_[0xC4] - ENV_RATE_TABLE[Y_];
			if (mem_[0xC4] < ENV_RATE_TABLE[Y_]) {
				A_ = 0;
				++mem_[0x704 + X_];
			}
			mem_[0x710 + X_] = A_;
		}
	}
}

void CEngine::L8720() {
	// $8720 - $8762
	if (X_ >= 0x28u) {
		if (mem_[0xCF] >= 0x80u) {
			L88A0();
			return;
		}
		A_ = mem_[0xCD];
		Y_ = mem_[0xCC];
		if (Y_ < 0x80u)
			A_ ^= 0xFF;
		if (A_ != 0xFF) {
			if (X_ == 0x29) {
				mem_[0xC4] = A_;
				A_ = mem_[0xC1] = mem_[0x740 + X_];
				Multiply();
				A_ = mem_[0xC1];
				if (!mem_[0xC1]) {
					WriteVolumeReg();
					return;
				}
				A_ = mem_[0x710 + X_] ? 0xFF : 0;
				WriteVolumeReg();
				return;
			}
			else if (A_ >= mem_[0x710 + X_])
				A_ = mem_[0x710 + X_];
		}
		else {
			if ((X_ & 0x03) == 0x01) {
				A_ = mem_[0x710 + X_] ? 0xFF : 0;
				WriteVolumeReg();
				return;
			}
			A_ = mem_[0x710 + X_];
		}
	}
	else {
		if ((X_ & 0x03) == 0x01) {
			A_ = mem_[0x710 + X_] ? 0xFF : 0;
			WriteVolumeReg();
			return;
		}
		A_ = mem_[0x710 + X_];
	}

	// $8763 - $87A9
	A_ = (A_ >> 4) ^ 0x0F;
	mem_[0xC3] = A_;
	Y_ = 6;
	A_ = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 6);
	if (A_ >= 0x05) {
		mem_[0xC4] = A_;
		Y_ = mem_[0x708 + X_];
		A_ = mem_[0x704 + X_];
		bool C = (A_ & 0x40) != 0;
		A_ = Y_;
		if (C)
			A_ ^= 0xFF;
		if (A_) {
			mem_[0xC1] = A_;
			Multiply();
			A_ = mem_[0xC1] >> 2;
			if (A_ >= 0x10) {
				A_ = mem_[0x70C + X_] & 0xF0;
				WriteVolumeReg();
				return;
			}
			if (A_ >= mem_[0xC3])
				mem_[0xC3] = A_;
		}
	}
	mem_[0xC4] = 0x10;
	A_ = mem_[0x70C + X_] - mem_[0xC3];
	if (!(A_ & 0x10))
		A_ = mem_[0x70C + X_] & 0xF0;
	WriteVolumeReg();
}

void CEngine::WriteVolumeReg() {
	// $87AA - $880B
	Y_ = 0;
	Write2A03();
	Y_ = X_ & 0x03;
	A_ = mem_[0x77C + Y_];
	bool write = true;
	do {
		if (A_ >= 0x80u)
			break;
		Y_ = 0x05;
		A_ = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 5);
		if (!A_)
			break;
		mem_[0xC4] = A_;
		Y_ = mem_[0x708 + X_];
		A_ = mem_[0x704 + X_];
		bool C = (A_ & 0x40) != 0;
		A_ = Y_;
		if (C)
			A_ ^= 0xFF;
		if (!A_)
			break;
		mem_[0xC1] = A_;
		Multiply();
		A_ = mem_[0xC1];
		chain(A_, mem_[0xC2]) >>= 4;
		Y_ = A_;
		if (!(A_ | mem_[0xC2]))
			break;
		A_ = mem_[0x704 + X_];
		if (A_ < 0x80u) {
			auto Y1 = Y_;
			chain(Y_, mem_[0xC2]) += chain(mem_[0x724 + X_], mem_[0x720 + X_]);
			A_ = Y_;
			Y_ = Y1;
			if (A_) {
				Y_ = A_;
				write = false;
				break;
			}
		}
		mem_[0xC1] = Y_;
		chain(A_, mem_[0xC2]) = chain(mem_[0x724 + X_], mem_[0x720 + X_])
				      - chain(mem_[0xC1], mem_[0xC2]);
		Y_ = A_;
		if (A_)
			write = false;
	} while (false);

	// $880C - $8834
	if (write) {
		A_ = mem_[0xC2] = mem_[0x720 + X_];
		Y_ = mem_[0x724 + X_];
	}
	if (X_ < 0x28u && mem_[0xD6] >= 0x80u && mem_[0xD8]) {
		mem_[0xC4] = mem_[0xD8];
		mem_[0xC1] = Y_;
		A_ = mem_[0xC2];
		auto A1 = A_;
		Multiply();
		A_ = 0;
		chain(A_, mem_[0xC2]) += chain(mem_[0xC1], A1);
		Y_ = A_;
	}

	// $8835 - $8883
	A_ = X_ & 0x03;
	if (!A_) {
		A_ = Y_ & 0x0F;
		Y_ = 0x07;
		mem_[0xC2] = A_ | ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 7);
		A_ = mem_[0xC1] = 0;
	}
	else {
		A_ = Y_;
		Y_ = 0x08;
		do {
			--Y_;
		} while (A_ < Y_ * 7); // $8953 - $895A
		mem_[0xC1] = A_;
		A_ = Y_ + mem_[0xC1];
		Y_ = A_;
		mem_[0xC1] = 0x07 + (A_ & 0x07);
		A_ = (Y_ & 0x38) ^ 0x38;
		while (A_) {
			chain(mem_[0xC1], mem_[0xC2]) >>= 1;
			A_ -= 8;
		}
		Y_ = 0;
		A_ = mem_[0x714 + X_];
		if (A_) {
			if (A_ >= 0x80u)
				--Y_;
			chain(mem_[0xC1], mem_[0xC2]) += chain(Y_, A_);
		}
	}

	// WritePitchReg
	// $8884 - $889F
	Y_ = 2;
	A_ = mem_[0xC2];
	Write2A03();
	Y_ = X_ & 0x03;
	A_ = mem_[0x77C + Y_];
	if (mem_[0xC1] != mem_[0x77C + Y_]) {
		mem_[0x77C + Y_] = mem_[0xC1];
		A_ = mem_[0xC1] | 0x08;
		Y_ = 3;
		Write2A03();
	}
	L88A0();
}

void CEngine::L88A0() {
	// $88A0 - $88F9
	if (mem_[0x704 + X_] & 0x20) {
		A_ = mem_[0x718 + X_];
		if (A_) {
			auto current = chain(mem_[0x724 + X_], mem_[0x720 + X_]);
//			const auto target = chain(ReadCallback(0x895A + mem_[0x71C + X_] * 2),
//			                          ReadCallback(0x8959 + mem_[0x71C + X_] * 2));
			const uint16_t target = ReadCallback(0x895A + mem_[0x71C + X_] * 2) * 0x100
			                      + ReadCallback(0x8959 + mem_[0x71C + X_] * 2);
			bool C = A_ >= 0x80u;
			A_ <<= 1;
			if (C) {
				A_ = -A_;
				Y_ = 0xFF;
			}
			else
				Y_ = 0;
			current += chain(Y_, A_);
			Y_ = mem_[0x71C + X_] << 1;
			bool C2 = (current & 0x3FFF) >= target;
			A_ = C + C2 - 1;
			if (!A_ && X_) {
				current = target;
				A_ = mem_[0x704 + X_] &= 0xDF;
			}
		}
		else
			A_ = mem_[0x704 + X_] &= 0xDF;
	}

	// $88FA - $8914
	Y_ = 0x04;
	A_ = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 4) & 0x7F;
	if (A_) {
		uint8_t C = 0;
		chain(C, mem_[0x708 + X_]) += A_;
		if (C)
			mem_[0x704 + X_] += 0x40;
	}
}

} // namespace MM5Sound
