#include "simulator/sim_gx_Geometry.hpp"

#include <limits>

#include <dolphin.h>

#include "simulator/sim_gx_State.hpp"


template <typename VertexCompDataType>
static float NormalizeToFloat(VertexCompDataType value)
{
    static_assert(std::is_integral_v<VertexCompDataType>, "NormalizeToFloat requires an integer type");

    if constexpr (std::is_signed_v<VertexCompDataType>)
    {
        // Signed: map [min, max] -> [-1, 1]
        // Note: max is used as the divisor (not min's abs value),
        // since |min| > max for two's complement types (e.g. int8_t: -128..127).
        // This means min maps to slightly less than -1, so we clamp.
        constexpr float maxVal = static_cast<float>(std::numeric_limits<VertexCompDataType>::max());
        float result = static_cast<float>(value) / maxVal;
        return std::max(result, -1.0f);
    }
    else
    {
        // Unsigned: map [0, max] -> [-1, 1]
        constexpr float maxVal = static_cast<float>(std::numeric_limits<VertexCompDataType>::max());
        return (static_cast<float>(value) / maxVal) * 2.0f - 1.0f;
    }
}


namespace SIM::GX {

GeometryProcessor::GeometryProcessor() {}

void GeometryProcessor::ProcessByteStream(std::vector<u8>& byteStream) {
  auto& gxState = GetGlobalState();
  mRenderVerts.clear();
  size_t bytesPerVertex = gxState.GetNumBytesPerVertex();
  if(bytesPerVertex == 0) {
    // Should probably not get here
    // No vertex attributes are enabled
    return;
  }
  size_t numVerts = byteStream.size() / bytesPerVertex;
  auto& format = gxState.GetCurrentVertexFormat();

  u8 * byteStreamPointer = byteStream.data();
  for(size_t i = 0; i < numVerts; i++) {
    RenderVertex vtxOut = {};

    // Handle coordinates
    auto positionDescriptor = gxState.GetVertexDescriptor(GX_VA_POS);
    if(positionDescriptor != GX_NONE) {
        auto coordDataType = format.mAttributes[GX_VA_POS].mDataType;
        auto descriptorSize = gxState.GetDescriptorSize(positionDescriptor, coordDataType);
        auto numComponents = gxState.GetNumPositionComponents(format.mAttributes[GX_VA_POS].mComponents);
        //for(auto coordIdx = 0; coordIdx < numComponents; coordIdx++) { // This for would only apply if we are in GX_DIRECT
        int stride = 0;
        u8 * arrayPtr = nullptr;
        size_t arrayIdx = 0;
        if(positionDescriptor == GX_DIRECT) {
            stride = bytesPerVertex;            
            // consume the values directly from the byte stream
            for(size_t coordIdx = 0; coordIdx < numComponents; coordIdx++) {
                switch(coordDataType) {
                    case GX_U8:
                        {
                            u8 value = *byteStreamPointer;
                            byteStreamPointer++;
                            vtxOut.position.coords[coordIdx] = NormalizeToFloat<u8>(value);
                        }
                        break;
                    case GX_S8:
                        {
                            s8 value = *(s8*)byteStreamPointer;
                            byteStreamPointer++;
                            vtxOut.position.coords[coordIdx] = NormalizeToFloat<s8>(value);
                        }
                        break;
                    case GX_U16:
                        {
                            u16 value = *(u16*)byteStreamPointer;
                            byteStreamPointer+= sizeof(u16);
                            vtxOut.position.coords[coordIdx] = NormalizeToFloat<u16>(value);
                        }
                        break;
                    case GX_S16:
                        {
                            s16 value = *(s16*)byteStreamPointer;
                            byteStreamPointer+= sizeof(s16);
                            vtxOut.position.coords[coordIdx] = NormalizeToFloat<s16>(value);
                        }
                        break;
                    case GX_F32:
                        {
                            f32 value = *(f32*)byteStreamPointer;
                            byteStreamPointer+= sizeof(f32);
                            vtxOut.position.coords[coordIdx] = value;
                        }
                        break;
                    default:
                        // Bad data type!!!
                        OSReport("SIM::GX::Geometry bad data type!\n");
                        break;
                }
            }

        } else {
            stride = gxState.GetVertexArray(GX_VA_POS).mStride;
            arrayPtr = static_cast<u8*>(gxState.GetVertexArray(GX_VA_POS).mArrayPtr);

            // Get the array index from the byte stream
            if(positionDescriptor == GX_INDEX16) {
                arrayIdx = *(u16*)byteStreamPointer;
                byteStreamPointer += 2;
            } else {
                arrayIdx = *(u8*)byteStreamPointer;
                byteStreamPointer++;
            }
            // Read the values from the array
            for(size_t coordIdx = 0; coordIdx < numComponents; coordIdx++) {
                switch(coordDataType) {
                    case GX_U8:
                        vtxOut.position.coords[coordIdx] = NormalizeToFloat<u8>(*(u8*)(arrayPtr + (arrayIdx * stride) + (sizeof(u8) * coordIdx)));
                        break;
                    case GX_S8:
                        vtxOut.position.coords[coordIdx] = NormalizeToFloat<s8>(*(s8*)(arrayPtr + (arrayIdx * stride) + (sizeof(s8) * coordIdx)));
                        break;
                    case GX_U16:
                        vtxOut.position.coords[coordIdx] = NormalizeToFloat<u16>(*(u16*)(arrayPtr + (arrayIdx * stride) + (sizeof(u16) * coordIdx)));
                        break;
                    case GX_S16:
                        vtxOut.position.coords[coordIdx] = NormalizeToFloat<s16>(*(s16*)(arrayPtr + (arrayIdx * stride) + (sizeof(s16) * coordIdx)));
                        break;
                    case GX_F32:
                        vtxOut.position.coords[coordIdx] = *(f32*)(arrayPtr + (arrayIdx * stride) + (sizeof(f32) * coordIdx));
                        break;
                    default:
                        // Bad data type!!!
                        OSReport("SIM::GX::Geometry bad data type!\n");
                        break;
                }
            }

        }
            //vtxOut.position.coords[coordIdx] =
        //}
    }

    // Handle color0
    //TODO: for now I just advance bytestreampointer by the amount we want
    byteStreamPointer++;


    mRenderVerts.push_back(vtxOut);
  }

  OSReport("Here\n");

}
}