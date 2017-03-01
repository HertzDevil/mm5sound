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
	void BREAK() const override {
		const auto fn = [&] (unsigned adr) {
			printf("%04X:", adr);
			for (unsigned n = adr + 0x10; adr < n; ++adr) {
				switch (adr) {
				case 0xC5: case 0xC6: case 0xC7: case 0xC8: case 0xC9: case 0xCA: case 0xCB:
				case 0xD0: case 0xD1:
					printf(" --"); break;
				default:
					printf(" %02X", mem_[adr]);
				}
			}
			putchar('\n');
		};
		fn(0x700);
		fn(0x710);
		fn(0x720);
		fn(0xC0);
		fn(0xD0);
		printf("A: %02X    X: %02X    Y: %02X\n", A_, X_, Y_);
		getchar();
	}
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
//	mm5->BREAK();
	for (int t = 0; t < ticks; ++t) {
		printf("PLAY(%d)\n", t);
		mm5->CallPLAY();
//		mm5->BREAK();
	}
}

int main(int argc, char **argv) {
	PlayNSF<CEngineNSFLog>(
		argc >= 2 ? atoi(argv[1]) : 0,
		0,
		argc >= 3 ? atoi(argv[2]) : 1800
	);
	return 0;
}
