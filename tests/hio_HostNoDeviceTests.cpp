#include <dolphin/hio.h>

#include <array>
#include <type_traits>

using HIOWriteSignature = BOOL (*)(u32, const void*, s32);
using HIOWriteAsyncSignature =
    BOOL (*)(u32, const void*, s32, HIOCallback);
using HIOInitExSignature = BOOL (*)(s32, u32, HIOCallback);
using HIOInit2Signature =
    BOOL (*)(s32, HIOCallback, HIONotify, void*);
using HIOInitEx2Signature =
    BOOL (*)(s32, s32, HIOCallback, HIONotify, void*);

static_assert(std::is_same_v<decltype(&HIOWrite), HIOWriteSignature>);
static_assert(
    std::is_same_v<decltype(&HIOWriteAsync), HIOWriteAsyncSignature>);
static_assert(std::is_same_v<decltype(&HIOInitEx), HIOInitExSignature>);
static_assert(std::is_same_v<decltype(&HIOInit2), HIOInit2Signature>);
static_assert(std::is_same_v<decltype(&HIOInitEx2), HIOInitEx2Signature>);
static_assert(HIO_STATUS_TX == 0x00000001);
static_assert(HIO_STATUS_RX == 0x00000002);
static_assert(HIO_STATUS_ID == 0x00000004);
static_assert(HIO_NOTIFY_INIT_REQUEST == 0x0001);
static_assert(HIO_NOTIFY_EXIT_DONE == 0x0002);

namespace {

int sCallbackCount;
int sEnumCallbackCount;
int sNotifyCount;

void CompletionCallback()
{
	++sCallbackCount;
}

BOOL EnumCallback(s32)
{
	++sEnumCallbackCount;
	return TRUE;
}

s32 NotifyCallback(s32, void*)
{
	++sNotifyCount;
	return 0;
}

bool TestDiscoveryAndInitialization()
{
	sCallbackCount = 0;
	sEnumCallbackCount = 0;
	sNotifyCount = 0;

	return HIOEnumDevices(EnumCallback) == TRUE &&
	       HIOInit(0, CompletionCallback) == FALSE &&
	       HIOInitEx(0, 2, CompletionCallback) == FALSE &&
	       HIOInit2(0, CompletionCallback, NotifyCallback, nullptr) == FALSE &&
	       HIOInitEx2(0, 2, CompletionCallback, NotifyCallback, nullptr) ==
	           FALSE &&
	       sEnumCallbackCount == 0 && sCallbackCount == 0 &&
	       sNotifyCount == 0;
}

bool TestMailboxAndStatusOutputsRemainUntouched()
{
	u32 mailbox = 0x12345678U;
	u32 status = 0x89ABCDEFU;

	return HIOReadMailbox(&mailbox) == FALSE &&
	       HIOWriteMailbox(0xDEADBEEFU) == FALSE &&
	       HIOReadStatus(&status) == FALSE && mailbox == 0x12345678U &&
	       status == 0x89ABCDEFU;
}

bool TestTransferBuffersAndCallbacksRemainUntouched()
{
	std::array<u8, 8> buffer = { 0x10, 0x21, 0x32, 0x43,
	                             0x54, 0x65, 0x76, 0x87 };
	const std::array<u8, 8> expected = buffer;
	const std::array<u8, 8> writeBuffer = expected;
	sCallbackCount = 0;

	return HIORead(0x1000U, buffer.data(), static_cast<s32>(buffer.size())) ==
	           FALSE &&
	       buffer == expected &&
	       HIOWrite(0x1000U, writeBuffer.data(),
	                static_cast<s32>(writeBuffer.size())) ==
	           FALSE &&
	       buffer == expected &&
	       HIOReadAsync(0x2000U, buffer.data(),
	                    static_cast<s32>(buffer.size()), CompletionCallback) ==
	           FALSE &&
	       buffer == expected && sCallbackCount == 0 &&
	       HIOWriteAsync(0x2000U, writeBuffer.data(),
	                     static_cast<s32>(writeBuffer.size()),
	                     CompletionCallback) ==
	           FALSE &&
	       buffer == expected && sCallbackCount == 0;
}

bool TestInvalidAndEmptyRequestsFailSafely()
{
	return HIOEnumDevices(nullptr) == FALSE &&
	       HIOInit(-1, nullptr) == FALSE && HIOReadMailbox(nullptr) == FALSE &&
	       HIORead(0, nullptr, 0) == FALSE &&
	       HIOWrite(0, nullptr, -1) == FALSE &&
	       HIOReadAsync(0, nullptr, 0, nullptr) == FALSE &&
	       HIOWriteAsync(0, nullptr, -1, nullptr) == FALSE &&
	       HIOReadStatus(nullptr) == FALSE;
}

}  // namespace

int main()
{
	if (!TestDiscoveryAndInitialization()) {
		return 1;
	}
	if (!TestMailboxAndStatusOutputsRemainUntouched()) {
		return 2;
	}
	if (!TestTransferBuffersAndCallbacksRemainUntouched()) {
		return 3;
	}
	if (!TestInvalidAndEmptyRequestsFailSafely()) {
		return 4;
	}
#if defined(_WIN32) || defined(WIN32)
	HIOExit();
#endif
	return 0;
}
