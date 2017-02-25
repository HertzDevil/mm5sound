#pragma once

#include <cstdint>
#include "chain_int.h"

namespace MM5Sound {

struct CSFXTrack {
	CSFXTrack(uint8_t *memory, uint8_t id);
	uint8_t &envNumber;	// $0700
	uint8_t &envState;	// $0704
	uint8_t &oscPhase;	// $0708
	uint8_t &volumeDuty;	// $070C
	uint8_t &envLevel;	// $0710
	uint8_t &detune;	// $0714
	uint8_t &portamento;	// $0718
	uint8_t &note;		// $071C
	ChainInt<2> pitch;	// $0720

	const uint8_t index;
	const uint8_t channelID;
};

struct CMusicTrack : public CSFXTrack {
	CMusicTrack(uint8_t *memory, uint8_t id);
	ChainInt<2> patternAdr;	// $0728
	uint8_t &octaveFlag;	// $0730
	uint8_t &transpose;	// $0734
	uint8_t &noteWait;	// $0738
	uint8_t &gateTime;	// $073C
	uint8_t &sustainWait;	// $0740
	uint8_t &loopCount1;	// $0744
	uint8_t &loopCount2;	// $0748
	uint8_t &loopCount3;	// $074C
	uint8_t &loopCount4;	// $0750
	uint8_t &periodCache;	// $0754
};

struct ISongPlayer {
	virtual ~ISongPlayer() = default;
	virtual void CallINIT(uint8_t track, uint8_t region) = 0;
	virtual void CallPLAY() = 0;
	virtual uint8_t ReadCallback(uint16_t adr) const = 0;
	virtual void WriteCallback(uint16_t adr, uint8_t value) = 0;
};

class CEngine : public ISongPlayer {
public:
	CEngine();
	~CEngine() override;

protected:
	void CallINIT(uint8_t track, uint8_t region) override;
	void CallPLAY() override;
	uint8_t ReadCallback(uint16_t adr) const override;
	void WriteCallback(uint16_t adr, uint8_t value) override;

private:
	uint16_t Multiply(uint8_t a, uint8_t b);
	uint8_t ReadROM(uint16_t adr);
	void StepDriver();
	void SilenceChannel(uint8_t id);
	void Write2A03();
	void InitDriver();
	void Func8106();
	void Func8118();
	void Func81D4();
	void Func81E4();
	void Func81F1();
	void Func8252();
	void Func82DE();
	void Func8326();
	uint8_t GetSFXData();
	void ProcessChannel(uint8_t id);
	void CommandDispatch();
	uint8_t GetTrackData(uint8_t id);
	void Func85A3();
	void Func85AE();
	void Func85DE();
	void Func8636();
	void Func8644();
	void Func8684();
	void Func86BA();

	void L81C5();
	void L81C8();
//	void L81E4();
	void L821E();
	void L8226();
	void L822D();
	void L8234();
	void L824A();

	void CmdTriplet();
	void CmdTie();
	void CmdDot();
	void Cmd15va();
	void CmdFlags();
	void CmdTempo();
	void CmdGate();
	void CmdVolume();
	void CmdEnvelope();
	void CmdOctave();
	void CmdGlobalTrsp();
	void CmdTranspose();
	void CmdDetune();
	void CmdPortamento();
	void CmdLoop1();
	void CmdLoop2();
	void CmdLoop3();
	void CmdLoop4();
	void CmdGoto();
	void CmdHalt();
	void CmdDuty();

	void L86D1();
	void L86E6();
	void L8702();
	void L8720();
	void L88A0();

	void CmdLoopImpl();
	void WriteVolumeReg();
	void WritePitchReg();

	CSFXTrack *GetSFXTrack(uint8_t id) const;
	CMusicTrack *GetMusicTrack(uint8_t id) const;

	uint8_t var_globalTrsp; // $CB
	uint16_t sfx_currentPtr; // $D0 - $D1

	static const uint8_t NOTE_LENGTH[];
	static const uint8_t NOTE_LENGTH_DOTTED[];
	static const uint8_t OCTAVE_TABLE[];
	static const uint8_t ENV_RATE_TABLE[];
	static const uint8_t TRACK_COUNT;
	static const uint16_t SONG_TABLE;
	static const uint16_t INSTRUMENT_TABLE;

	uint8_t mem_[0x800] = { };
	uint8_t A_ = 0u, X_ = 0u, Y_ = 0u;

	CSFXTrack *sfx_[4] = { };
	CMusicTrack *mus_[4] = { };

/*
700	envelope index
704	envelope state
708	lfo phase
70C	volume/duty byte
710	instrument level
714	detune
718	portamento
71C	note index
720	internal pitch, lo
724	internal pitch, hi
728	pattern adr, lo
72C	pattern adr, hi
730	octave, flags
734	transpose
738	note duration
73C	gate time
740	sustain duration
744	loop count 1
748	loop count 2
74C	loop count 3
750	loop count 4
754	period high byte cache
*/
};

} // namespace MM5Sound
