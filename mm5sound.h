#pragma once

#include <cstdint>
#include "chain_int.h"

namespace MM5Sound {

struct CSFXTrack {
	CSFXTrack(uint8_t *memory, uint8_t id);
	virtual ~CSFXTrack() = default;

	virtual void Reset();

	const uint8_t index;
	const uint8_t channelID;

	uint8_t &envNumber;	// $0700
	uint8_t &envState;	// $0704
	uint8_t &oscPhase;	// $0708
	uint8_t &volumeDuty;	// $070C
	uint8_t &envLevel;	// $0710
	uint8_t &detune;	// $0714
	uint8_t &portamento;	// $0718
	uint8_t &note;		// $071C
	ChainInt<2> pitch;	// $0720
	uint8_t &periodCache;	// $077C
};

struct CMusicTrack : public CSFXTrack {
	using CSFXTrack::CSFXTrack;

	void Reset() override;

	uint16_t patternAdr;	// $0728
	uint8_t octaveFlag;	// $0730
	uint8_t transpose;	// $0734
	uint8_t noteWait;	// $0738
	uint8_t gateTime;	// $073C
	uint8_t sustainWait;	// $0740
	uint8_t loopCount[4];	// $0744
};

struct ISongPlayer {
	virtual ~ISongPlayer() = default;
	virtual void CallINIT(uint8_t track, uint8_t region) = 0;
	virtual void CallPLAY() = 0;
	virtual void BREAK() const { }
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
	void CommandDispatch(uint8_t id, uint8_t fx);
	uint8_t GetTrackData(uint8_t id);
	void ReleaseNote(uint8_t id);
	void Func85AE();
	void Func85DE(uint8_t id);
	void Func8636(uint8_t id, uint16_t pitch);
	void Func8644(uint8_t id);
	void LoadEnvelope(uint8_t index);
	void Func86BA(uint8_t id);

	void L81C5();
	void L81C8();
//	void L81E4();
	void L821E();
	void L8226();
	void L822D();
	void L8234();
	void L824A();

	void CmdTriplet(uint8_t id);
	void CmdTie(uint8_t id);
	void CmdDot(uint8_t id);
	void Cmd15va(uint8_t id);
	void CmdFlags(uint8_t id);
	void CmdTempo(uint8_t id);
	void CmdGate(uint8_t id);
	void CmdVolume(uint8_t id);
	void CmdEnvelope(uint8_t id);
	void CmdOctave(uint8_t id);
	void CmdGlobalTrsp();
	void CmdTranspose(uint8_t id);
	void CmdDetune(uint8_t id);
	void CmdPortamento(uint8_t id);
	void CmdLoopEnd(uint8_t id, uint8_t level);
	void CmdLoopBreak(uint8_t id, uint8_t level);
	void CmdGoto(uint8_t id);
	void CmdHalt(uint8_t id);
	void CmdDuty(uint8_t id);

	void EnvelopeAttack(uint8_t id);
	void EnvelopeDecay(uint8_t id);
	void EnvelopeRelease(uint8_t id);
	void L8720(uint8_t id);
	void WriteVolumeReg(uint8_t id);
//	void WritePitchReg(uint8_t id);
	void L88A0(uint8_t id);

	CSFXTrack *GetSFXTrack(uint8_t id) const;
	CMusicTrack *GetMusicTrack(uint8_t id) const;

protected:
	uint16_t var_envelopePtr; // $C5 - $C6
	uint8_t var_tickElapsed; // $C7
	uint8_t var_tickCounter; // $C8
	uint16_t var_tempo; // $C9 - $CA
	uint8_t var_globalTrsp; // $CB
	uint16_t sfx_currentPtr; // $D0 - $D1

	static const uint8_t NOTE_LENGTH_TRIPLET[];
	static const uint8_t NOTE_LENGTH[];
	static const uint8_t OCTAVE_TABLE[];
	static const uint8_t ENV_RATE_TABLE[];
	static const uint16_t PITCH_TABLE[];
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
	0:2	envelope part
	3	
	4
	5	portamento enabled
	6:7	lfo phase hi
708	lfo phase lo
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
	0:2	octave
	3	15va
	4	dot
	5	triplet
	6	tie
	7	
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
