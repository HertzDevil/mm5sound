#include "mm5sound.h"
#include <cstdio>
#include <cstdlib>
#include <memory>

using namespace MM5Sound;

namespace {

// using std::make_unique;
template <class T, class... Arg>
auto make_unique(Arg&&... args) {
	return std::unique_ptr<T>(new T(std::forward<Arg>(args)...));
}

} // namespace

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
	std::unique_ptr<ISongPlayer> mm5 = make_unique<T>();
	printf("INIT(%02X,%02X)\n", track, region);
	mm5->CallINIT(track, region);
	for (int t = 0; t < ticks; ++t) {
		printf("PLAY(%d)\n", t);
		mm5->CallPLAY();
	}
}

int main(int argc, char **argv) {
	PlayNSF<CEngineNSFLog>(argc >= 2 ? atoi(argv[1]) : 0, 0, argc >= 3 ? atoi(argv[2]) : 1800);
	return 0;
}
