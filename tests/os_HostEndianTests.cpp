#include <dolphin/os/OSHostEndian.h>

#include <bit>
#include <cstdint>

int main()
{
	const u8 bytes[] = {
	    0xFF,
	    0x12, 0x34,
	    0x89, 0xAB, 0xCD, 0xEF,
	    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
	    0x3F, 0xC0, 0x00, 0x00,
	    0xC0, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	if (OSReadBigEndian16(bytes + 1) != 0x1234U) {
		return 1;
	}
	if (OSReadBigEndian32(bytes + 3) != 0x89ABCDEFU) {
		return 2;
	}
	if (OSReadBigEndian64(bytes + 7) != 0x0123456789ABCDEFULL) {
		return 3;
	}
	if (std::bit_cast<std::uint32_t>(OSReadBigEndianF32(bytes + 15))
	    != 0x3FC00000U) {
		return 4;
	}
	if (std::bit_cast<std::uint64_t>(OSReadBigEndianF64(bytes + 19))
	    != 0xC004000000000000ULL) {
		return 5;
	}
	if (OSReadBigEndianS16(bytes + 1) != 0x1234 ||
	    OSReadBigEndianS32(bytes + 3) !=
	        static_cast<s32>(0x89ABCDEFU) ||
	    OSReadBigEndianS64(bytes + 7) !=
	        static_cast<s64>(0x0123456789ABCDEFULL)) {
		return 6;
	}

	u8 output[28] = {};
	OSWriteBigEndian16(output + 1, 0x1234U);
	OSWriteBigEndian32(output + 3, 0x89ABCDEFU);
	OSWriteBigEndian64(output + 7, 0x0123456789ABCDEFULL);
	OSWriteBigEndianF32(output + 15, 1.5f);
	OSWriteBigEndianF64(output + 19, -2.5);
	for (std::size_t index = 1; index < sizeof(bytes); ++index) {
		if (output[index] != bytes[index]) {
			return 7;
		}
	}

	return 0;
}
