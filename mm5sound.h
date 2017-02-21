#pragma once

#include <cstdint>
#include <initializer_list>

namespace MM5Sound {



struct ISongPlayer {
	virtual ~ISongPlayer() noexcept = default;
	virtual void CallINIT(uint8_t track, uint8_t region) = 0;
	virtual void CallPLAY() = 0;
	virtual uint8_t ReadCallback(uint16_t adr) const = 0;
	virtual void WriteCallback(uint16_t adr, uint8_t value) = 0;
};

class CEngine : public ISongPlayer {
	using FuncList_t = std::initializer_list<void (CEngine::*)()>;

public:
	CEngine() = default;
	void BREAK() const;

protected:
	void CallINIT(uint8_t track, uint8_t region) override;
	void CallPLAY() override;
	uint8_t ReadCallback(uint16_t adr) const override;
	void WriteCallback(uint16_t adr, uint8_t value) override;

private:
	void Multiply();
	void SwitchDispatch(FuncList_t funcs);
	uint8_t ReadROM(uint16_t adr);
	void StepDriver();
	void SilenceChannel();
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
	void GetSFXData();
	void ProcessChannel();
	void CommandDispatch();
	void GetTrackData();
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

/*
700	envelope index
704	envelope state
708	
70C	volume/duty byte
710	instrument level
714	detune
718	portamento
71C	
720	
724	
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



}
