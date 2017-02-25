#include "mm5sound.h"
#include "mm5sndrom.h"
#include "mm5constants.h"
#include <stdexcept>



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



CSFXTrack::CSFXTrack(uint8_t *memory, uint8_t id) :
	envNumber(memory[0x700 + id]),
	envState(memory[0x704 + id]),
	oscPhase(memory[0x708 + id]),
	volumeDuty(memory[0x70C + id]),
	envLevel(memory[0x710 + id]),
	detune(memory[0x714 + id]),
	portamento(memory[0x718 + id]),
	note(memory[0x71C + id]),
	pitch(memory[0x724 + id], memory[0x720 + id]),
	index(id),
	channelID(id & 0x03)
{
}

void CSFXTrack::Reset() {
	envNumber = 0;
	envState = 0;
	oscPhase = 0;
	volumeDuty = 0;
	envLevel = 0;
	detune = 0;
	portamento = 0;
	note = 0;
	pitch = 0;
}



CMusicTrack::CMusicTrack(uint8_t *memory, uint8_t id) :
	CSFXTrack(memory, id),
	patternAdr(memory[0x72C + id], memory[0x728 + id]),
	octaveFlag(memory[0x730 + id]),
	transpose(memory[0x734 + id]),
	noteWait(memory[0x738 + id]),
	gateTime(memory[0x73C + id]),
	sustainWait(memory[0x740 + id]),
	loopCount1(memory[0x744 + id]),
	loopCount2(memory[0x748 + id]),
	loopCount3(memory[0x74C + id]),
	loopCount4(memory[0x750 + id]),
	periodCache(memory[0x754 + id])
{
}

void CMusicTrack::Reset() {
	CSFXTrack::Reset();
	patternAdr = 0;
	octaveFlag = 0;
	transpose = 0;
	noteWait = 0;
	gateTime = 0;
	sustainWait = 0;
	loopCount1 = 0;
	loopCount2 = 0;
	loopCount3 = 0;
	loopCount4 = 0;
	periodCache = 0;
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



CEngine::CEngine() {
	for (int i = 0; i < 4; ++i) {
		sfx_[3 - i] = new CSFXTrack(mem_, i);
		mus_[3 - i] = new CMusicTrack(mem_, i | 0x28);
	}
}

CEngine::~CEngine() {
	for (int i = 0; i < 4; ++i) {
		delete sfx_[i];
		delete mus_[i];
	}
}

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

CSFXTrack *CEngine::GetSFXTrack(uint8_t id) const {
	switch (id) {
	case 0x00: return sfx_[3];
	case 0x01: return sfx_[2];
	case 0x02: return sfx_[1];
	case 0x03: return sfx_[0];
	}
	return GetMusicTrack(id);
}

CMusicTrack *CEngine::GetMusicTrack(uint8_t id) const {
	switch (id) {
	case 0x28: return mus_[3];
	case 0x29: return mus_[2];
	case 0x2A: return mus_[1];
	case 0x2B: return mus_[0];
	}
	return nullptr;
}



uint16_t CEngine::Multiply(uint8_t a, uint8_t b) {
	// $8006 - $8022
	// destroys A
	uint16_t res = a * b;
	chain(mem_[0xC1], mem_[0xC2]) = res;
	mem_[0xC4] = b;
	Y_ = 0;
	return res;
}

/*
void CEngine::SwitchDispatch(FuncList_t funcs) {
	// $8023 - $8039
	// destroys A_
	(this->*(*(funcs.begin() + A_)))();
}
*/

uint8_t CEngine::ReadROM(uint16_t adr) {
	// $803A - $806B
	chain(mem_[0xC2], mem_[0xC1]) = adr;
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
			X_ = X_ | 0x28;
			ProcessChannel(X_);
			X_ = X_ & ~0x28;
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

void CEngine::SilenceChannel(uint8_t id) {
	// $80D8 - $80EB
	uint16_t adr = 0x4000 | (((id & 0x03) ^ 0x03) << 2);
	uint8_t value = ((id & 0x03) == 0x01) ? 0 : 0x30;
	A_ = value;
	Y_ = adr & 0xFF;
	WriteCallback(adr, value);
}

void CEngine::Write2A03() {
	// $80EC - $80FD
	mem_[0xC4] = Y_;
	Y_ |= ((X_ & 0x03) ^ 0x03) << 2;
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
		for (auto x : sfx_)
			x->Reset();
		A_ = 0;
	}
	else {
		// $816F - $81AD
		chain(mem_[0xC9], mem_[0xCA]) = 0x0199;
		mem_[0xC8] = var_globalTrsp = mem_[0xCC] = mem_[0xCD] = 0;
		for (auto x : mus_)
			x->Reset();
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
	if (!mem_[0xCF])
		return;
	mem_[0xCF] ^= 0x0F;
	Func81F1();
	mem_[0xCF] = 0;
	A_ = 0;
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
	for (X_ = 3; X_ < 0x80u; --X_) {
		if (!(mem_[0xCF] & (1 << (3 - X_)))) {
			SilenceChannel(X_);
			if (mem_[0x754 + X_] | mem_[0x750 + X_])
				A_ = mem_[0x77C + X_] = 0xFF;
		}
	}
	WriteCallback(0x4001, 0x08);
	WriteCallback(0x4005, 0x08);
	WriteCallback(0x4015, 0x0F);
	A_ = 0x0F;
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
	mem_[0xC0] &= 0x0F;
	Y_ = mem_[0xCC] = mem_[0xC3];
	if (mem_[0xCC]) {
		Y_ = 0xFF;
		if (mem_[0xCD] == 0xFF)
			Y_ = mem_[0xCD] = 0;
	}
	else
		mem_[0xCD] = 0;
	A_ = mem_[0xC0];
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
		A_ = mem_[0xC4] = GetSFXData();
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
		A_ = GetSFXData() << 1;
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
		X_ = GetSFXData();
		A_ = GetSFXData();
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
	if (lsr(mem_[0xC4]))
		mem_[0xD4] = GetSFXData();
	if (lsr(mem_[0xC4]))
		mem_[0xD2] = GetSFXData();
	mem_[0xD3] = GetSFXData();
	Y_ = Multiply(mem_[0xD3], mem_[0xD4]) >> 8;
	mem_[0xD5] = Y_ + 1;
	++mem_[0xC0];
	auto A1 = GetSFXData();
	A_ = A1 ^ mem_[0xCF];
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
	mem_[0xC4] = 0;
	A_ = GetSFXData();
	while (true) {
		if (lsr(A_)) {
			auto A1 = A_;
			mem_[0xC3] = GetSFXData();
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
	Y_ = A_ = GetSFXData();
	if (!A_) {
		mem_[0x710 + X_] = A_;
		mem_[0x704 + X_] &= 0xF8;
		mem_[0x704 + X_] |= 0x04;
		SilenceChannel(X_ & 0x03);
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
	case 0x00: CmdEnvelope(X_); break;
	case 0x01: CmdDuty(X_); break;
	case 0x02: CmdVolume(X_); break;
	case 0x03: CmdPortamento(X_); break;
	case 0x04: CmdDetune(X_); break;
	default: throw std::runtime_error {"Unknown SFX command"};
	}
}

uint8_t CEngine::GetSFXData() {
	// $8386 - $8392
	return ReadROM(sfx_currentPtr++);
}

void CEngine::ProcessChannel(uint8_t id) {
	// $8393 - 83CC
	CMusicTrack *Chan = GetMusicTrack(id);
	if (!Chan->patternAdr)
		return;
	A_ = Chan->noteWait;
	if (Chan->noteWait > 0) {
		Y_ = Chan->envNumber;
		if (Chan->envNumber) {
			Func8684();
			Func86BA();
		}
		if (Chan->sustainWait <= mem_[0xC7])
			Func85A3();
		Chan->sustainWait -= mem_[0xC7];
		if (Chan->noteWait > mem_[0xC7]) {
			A_ = Chan->noteWait -= mem_[0xC7];
			return;
		}
		Chan->noteWait -= mem_[0xC7];
	}

	// $83CD - $83D9
	while (true) {
		A_ = GetTrackData(Chan->index);
		const uint8_t cmd = A_;
		if (cmd >= 0x20u)
			break;
		CommandDispatch(Chan->index);
		if (cmd == 0x17)
			return;
	}

	// $83DA - $840D
	const auto A1 = A_;
	Y_ = (A1 >> 5) - 1;
	A_ = Chan->octaveFlag << 2;
	if (A_ >= 0x80u)
		A_ = NOTE_LENGTH[Y_];
	else {
		bool C = (A_ & 0x40) != 0;
		A_ = NOTE_LENGTH_DOTTED[Y_];
		if (C) {
			mem_[0xC3] = A_;
			Chan->octaveFlag &= 0xEF;
			A_ += A_ >> 1;
		}
	}
	Chan->noteWait += A_;
	Y_ = Chan->noteWait;

	// $840E - $842F
	A_ = A1 & 0x1F;
	if (!A_) {
		Func85A3();
		Chan->sustainWait = A_ = 0xFF;
		return;
	}
	Chan->sustainWait = Multiply(Chan->gateTime, Y_) >> 8;
	if (!Chan->sustainWait)
		Chan->sustainWait = 1;
	Y_ = A_ - 1;

	// $8430 - $847D
	if (!(Chan->octaveFlag & 0x80)) {
		Func85AE();
		A_ = mem_[0xCF];
		if (mem_[0xCF] < 0x80u) {
			mem_[0xC3] = Y_;
			Chan->periodCache = A_ = 0xFF;
		}
	}
	if (!(Chan->octaveFlag & 0x80) || Chan->portamento)
		if (Chan->channelID == 0) {
			mem_[0xC3] = 0;
			A_ = (Y_ & 0x0F) ^ 0x0F;
			Func8636();
		}
		else {
			mem_[0xC3] = Y_;
			Y_ = Chan->octaveFlag & 0x0F;
			A_ = OCTAVE_TABLE[Y_] + mem_[0xC3] + var_globalTrsp + Chan->transpose;
			Func85DE();
		}
	else
		Func8644();

	// $847E - $8496
	Y_ = Chan->octaveFlag;
	mem_[0xC4] = (Chan->octaveFlag & 0x40) << 1;
	A_ = mem_[0xC4] | (Chan->octaveFlag & 0x7F);
	Chan->octaveFlag = A_;
	if (A_ >= 0x80u)
		Chan->sustainWait = A_ = 0xFF;
}

void CEngine::CommandDispatch(uint8_t id) {
	// $8497 - $84D8
	CMusicTrack *Chan = GetMusicTrack(id);
	if (A_ >= 0x04u) {
		mem_[0xC4] = A_;
		mem_[0xC3] = GetTrackData(Chan->index);
		A_ = mem_[0xC4];
	}
	switch (A_) {
	case 0x00: CmdTriplet(id); break;
	case 0x01: CmdTie(id); break;
	case 0x02: CmdDot(id); break;
	case 0x03: Cmd15va(id); break;
	case 0x04: CmdFlags(id); break;
	case 0x05: CmdTempo(id); break;
	case 0x06: CmdGate(id); break;
	case 0x07: CmdVolume(id); break;
	case 0x08: CmdEnvelope(id); break;
	case 0x09: CmdOctave(id); break;
	case 0x0A: CmdGlobalTrsp(id); break;
	case 0x0B: CmdTranspose(id); break;
	case 0x0C: CmdDetune(id); break;
	case 0x0D: CmdPortamento(id); break;
	case 0x0E: case 0x12: CmdLoop(id, 0); break; // $851B - $851E
	case 0x0F: case 0x13: CmdLoop(id, 1); break; // $851F - $8522
	case 0x10: case 0x14: CmdLoop(id, 2); break; // $8523 - $8526
	case 0x11: case 0x15: CmdLoop(id, 3); break;
	case 0x16: CmdGoto(id); break;
	case 0x17: CmdHalt(id); break;
	case 0x18: CmdDuty(id); break;
	default: throw std::runtime_error {"Unknown command"};
	}
}

void CEngine::CmdTriplet(uint8_t id) {
	// $84D9 - $84DC
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = Chan->octaveFlag ^= 0x20;
}

void CEngine::CmdTie(uint8_t id) {
	// $84DD - $84E0
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = Chan->octaveFlag ^= 0x40;
}

void CEngine::CmdDot(uint8_t id) {
	// $84E1 - $84E7
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = Chan->octaveFlag |= 0x10;
}

void CEngine::Cmd15va(uint8_t id) {
	// $84E8 - $84F0
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = Chan->octaveFlag ^= 0x08;
}

void CEngine::CmdFlags(uint8_t id) {
	// $8575 - $857F
	CMusicTrack *Chan = GetMusicTrack(id);
	Chan->octaveFlag &= 0x97;
	Chan->octaveFlag |= mem_[0xC3];
	A_ = Chan->octaveFlag;
}

void CEngine::CmdTempo(uint8_t id) {
	// $84F1 - $84FE
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = mem_[0xC8] = 0;
	A_ = GetTrackData(X_);
	chain(mem_[0xC9], mem_[0xCA]) = chain(mem_[0xC3], A_);
	Y_ = mem_[0xC9];
}

void CEngine::CmdGate(uint8_t id) {
	// $84FF - $8504
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = Chan->gateTime = mem_[0xC3];
}

void CEngine::CmdVolume(uint8_t id) {
	// $865A - $866E
	CSFXTrack *Chan = GetSFXTrack(id);
	if (X_ != 0x01 || !mem_[0xC3])
		Chan->volumeDuty = ((Chan->volumeDuty & 0xC0) | mem_[0xC3] | 0x30);
	else
		Chan->volumeDuty = mem_[0xC3];
	A_ = Chan->volumeDuty;
}

void CEngine::CmdEnvelope(uint8_t id) {
	// $866F - $8683
	CSFXTrack *Chan = GetSFXTrack(id);
	A_ = ++mem_[0xC3];
	if (A_ != Chan->envNumber) {
		Chan->envNumber = A_;
		Y_ = A_;
		Chan->envState |= 0x08;
		Func8684();
	}
}

void CEngine::CmdOctave(uint8_t id) {
	// $8505 - $850F
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = Chan->octaveFlag = ((Chan->octaveFlag & 0xF8) | mem_[0xC3]);
}

void CEngine::CmdGlobalTrsp(uint8_t id) {
	// $8510 - $8514
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = var_globalTrsp = mem_[0xC3];
}

void CEngine::CmdTranspose(uint8_t id) {
	// $8515 - $851A
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = Chan->transpose = mem_[0xC3];
}

void CEngine::CmdDetune(uint8_t id) {
	// $86A1 - $86A6
	CSFXTrack *Chan = GetSFXTrack(id);
	A_ = Chan->detune = mem_[0xC3];
}

void CEngine::CmdPortamento(uint8_t id) {
	// $86A7 - $86AC
	CSFXTrack *Chan = GetSFXTrack(id);
	A_ = Chan->portamento = mem_[0xC3];
}

/*
void CEngine::CmdLoop1(uint8_t id) {
	// $851B - $851E
	CmdLoopImpl(id, 0);
}

void CEngine::CmdLoop2(uint8_t id) {
	// $851F - $8522
	CmdLoopImpl(id, 1);
}

void CEngine::CmdLoop3(uint8_t id) {
	// $8523 - $8526
	CmdLoopImpl(id, 2);
}

void CEngine::CmdLoop4(uint8_t id) {
	// $8527 - $8528
	CmdLoopImpl(id, 3);
}
*/

void CEngine::CmdLoop(uint8_t id, uint8_t level) {
	// $8527 - $8559
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = level << 2;
	mem_[0xC2] = A_;
	Y_ = A_ + X_;
	if (mem_[0xC4] < 0x12u) {
		A_ = mem_[0x744 + Y_];
		mem_[0x744 + Y_] = A_ ? (A_ - 1) : mem_[0xC3];
		if (!mem_[0x744 + Y_]) {
			// $8566 - $8574
			Chan->patternAdr += 2;
			A_ = Chan->patternAdr & 0xFF;
			return;
		}
	}
	else {
		A_ = mem_[0x744 + Y_] - 1;
		if (A_) {
			Chan->patternAdr += 2;
			A_ = Chan->patternAdr & 0xFF;
			return;
		}
		mem_[0x744 + Y_] = A_;
		CmdFlags(Chan->index);
	}

	A_ = GetTrackData(X_);
	mem_[0xC3] = A_;
	CmdGoto(Chan->index);
}

void CEngine::CmdGoto(uint8_t id) {
	// $855A - $8565
	CMusicTrack *Chan = GetMusicTrack(id);
	A_ = GetTrackData(X_);
	Chan->patternAdr = chain(mem_[0xC3], A_);
	A_ = mem_[0xC3];
}

void CEngine::CmdHalt(uint8_t id) {
	// $8580 - $8591
	CMusicTrack *Chan = GetMusicTrack(id);
	Chan->patternAdr = 0;
	A_ = mem_[0xCF];
	if (mem_[0xCF] < 0x80u)
		SilenceChannel(Chan->channelID);
}

void CEngine::CmdDuty(uint8_t id) {
	// $86AD - $86B9
	CSFXTrack *Chan = GetSFXTrack(id);
	A_ = Chan->volumeDuty = ((Chan->volumeDuty & 0x0F) | mem_[0xC3] | 0x30);
}

uint8_t CEngine::GetTrackData(uint8_t id) {
	// $8592 - $85A2
	CMusicTrack *Chan = GetMusicTrack(id);
	return ReadROM(Chan->patternAdr++);
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
		Y_ = (Multiply(mem_[0xD3], mem_[0x70C + X_]) >> 8) + 1;
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
	chain(mem_[0x724 + X_], mem_[0x720 + X_]) = chain(A_, mem_[0xC3]);
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
				A_ = Multiply(mem_[0x740 + X_], A_) >> 8;
				if (A_)
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
	const auto tremoloLv = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 6);
	if (tremoloLv >= 0x05) {
		mem_[0xC4] = tremoloLv;
		Y_ = mem_[0x708 + X_];
		A_ = mem_[0x708 + X_];
		if (mem_[0x704 + X_] & 0x40)
			A_ ^= 0xFF;
		if (A_) {
			A_ = Multiply(A_, tremoloLv) >> 10;
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
	mem_[0xC4] = 0;
	WriteCallback(0x4000 | (((X_ & 0x03) ^ 0x03) << 2), A_);
	Y_ = X_ & 0x03;
	A_ = mem_[0x77C + Y_];
	bool write = true;
	do {
		if (A_ >= 0x80u)
			break;
		Y_ = 0x05;
		const auto vibratoLv = ReadCallback(chain(mem_[0xC6], mem_[0xC5]) + 5);
		if (!vibratoLv)
			break;
		Y_ = mem_[0x708 + X_];
		A_ = mem_[0x708 + X_];
		if (mem_[0x704 + X_] & 0x40)
			A_ ^= 0xFF;
		if (!A_)
			break;
		auto offset = Multiply(A_, vibratoLv);
		chain(A_, mem_[0xC2]) = offset >> 4;
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
		mem_[0xC2] = mem_[0x720 + X_];
		chain(Y_, A_) = chain(mem_[0x724 + X_], mem_[0x720 + X_]);
	}
	if (X_ < 0x28u && mem_[0xD6] >= 0x80u && mem_[0xD8]) {
		auto A1 = mem_[0xC2];
		chain(mem_[0xC1], mem_[0xC2]) = Multiply(Y_, mem_[0xD8]);
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
	mem_[0xC4] = 2;
	A_ = mem_[0xC2];
	WriteCallback(0x4002 | (((X_ & 0x03) ^ 0x03) << 2), A_);
	Y_ = X_ & 0x03;
	A_ = mem_[0x77C + Y_];
	if (mem_[0xC1] != mem_[0x77C + Y_]) {
		mem_[0x77C + Y_] = mem_[0xC1];
		A_ = mem_[0xC1] | 0x08;
		mem_[0xC4] = 3;
		Y_ = 3;
		WriteCallback(0x4003 | (((X_ & 0x03) ^ 0x03) << 2), A_);
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
