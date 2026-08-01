#include <dolphin/os/OSHostEndian.h>

#include <bit>
#include <cstdint>
#include <type_traits>

static_assert(sizeof(s32) == 4);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(vs32) == 4);
static_assert(sizeof(vu32) == 4);
static_assert(std::is_signed_v<s32>);
static_assert(std::is_unsigned_v<u32>);

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
	        -1985229329 ||
	    OSReadBigEndianS64(bytes + 7) !=
	        static_cast<s64>(0x0123456789ABCDEFULL)) {
		return 6;
	}

	const u8 arrayBytes[] = {0x12, 0x34, 0xab, 0xcd, 0xfe, 0xdc};
	u16 decodedArray[3] = {};
	OSReadBigEndian16Array(decodedArray, arrayBytes, 3);
	if (decodedArray[0] != 0x1234U || decodedArray[1] != 0xabcdU ||
	    decodedArray[2] != 0xfedcU) {
		return 7;
	}

	u16 inPlaceArray[2] = {};
	u8* inPlaceBytes = reinterpret_cast<u8*>(inPlaceArray);
	inPlaceBytes[0] = 0xfc;
	inPlaceBytes[1] = 0x00;
	inPlaceBytes[2] = 0x80;
	inPlaceBytes[3] = 0x1f;
	OSReadBigEndian16Array(
	    inPlaceArray,
	    inPlaceArray,
	    2);
	if (inPlaceArray[0] != 0xfc00U || inPlaceArray[1] != 0x801fU) {
		return 8;
	}

	u8 output[28] = {};
	OSWriteBigEndian16(output + 1, 0x1234U);
	OSWriteBigEndian32(output + 3, 0x89ABCDEFU);
	OSWriteBigEndian64(output + 7, 0x0123456789ABCDEFULL);
	OSWriteBigEndianF32(output + 15, 1.5f);
	OSWriteBigEndianF64(output + 19, -2.5);
	for (std::size_t index = 1; index < sizeof(bytes); ++index) {
		if (output[index] != bytes[index]) {
			return 9;
		}
	}

	return 0;
}
