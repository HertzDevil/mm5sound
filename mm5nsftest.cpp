#include "mm5sound.h"
#include <cstdio>
#include <cstdlib>

using namespace MM5Sound;

class CEngineNSFLog : public CEngine {
	uint8_t ReadCallback(uint16_t adr) const override {
//		printf("READ(%04X)\n", adr);
		return CEngine::ReadCallback(adr);
	}
	void WriteCallback(uint16_t adr, uint8_t value) override {
		printf("WRITE(%04X,%02X)\n", adr, value);
		CEngine::WriteCallback(adr, value);
	}
};



template <class T>
void PlayNSF(int track, int region, int ticks) {
	ISongPlayer *mm5 = new T;
	printf("INIT(%02X,%02X)\n", track, region);
	mm5->CallINIT(track, region);
	for (int t = 0; t < ticks; ++t) {
		printf("PLAY(%d)\n", t);
		mm5->CallPLAY();
	}
	delete mm5;
}

int main(int argc, char **argv) {
	PlayNSF<CEngineNSFLog>(0, 0, 1800);
	return 0;
}
