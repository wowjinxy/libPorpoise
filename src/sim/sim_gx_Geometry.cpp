#include "simulator/sim_gx_Geometry.hpp"

#include <cmath>
#include <limits>

#include <dolphin.h>

#include "simulator/sim_gx_State.hpp"


template <typename VertexCompDataType>
static float ConvertToFloat(VertexCompDataType value, u8 frac)
{
    static_assert(std::is_integral_v<VertexCompDataType>, "ConvertToFloat requires an integer type");

    return (static_cast<float>(value) * std::pow(2, -1.0f * static_cast<float>(frac)));
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
        auto frac = format.mAttributes[GX_VA_POS].mFraction;
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
                            vtxOut.position.coords[coordIdx] = ConvertToFloat<u8>(value, frac);
                        }
                        break;
                    case GX_S8:
                        {
                            s8 value = *(s8*)byteStreamPointer;
                            byteStreamPointer++;
                            vtxOut.position.coords[coordIdx] = ConvertToFloat<s8>(value, frac);
                        }
                        break;
                    case GX_U16:
                        {
                            u16 value = *(u16*)byteStreamPointer;
                            byteStreamPointer+= sizeof(u16);
                            vtxOut.position.coords[coordIdx] = ConvertToFloat<u16>(value, frac);
                        }
                        break;
                    case GX_S16:
                        {
                            s16 value = *(s16*)byteStreamPointer;
                            byteStreamPointer+= sizeof(s16);
                            vtxOut.position.coords[coordIdx] = ConvertToFloat<s16>(value, frac);
                        }
                        break;
                    case GX_F32:
                        {
                            f32 value = *(f32*)byteStreamPointer;
                            byteStreamPointer+= sizeof(f32);
                            vtxOut.position.coords[coordIdx] = value * std::pow(2, -(frac));
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
                        vtxOut.position.coords[coordIdx] = ConvertToFloat<u8>(*(u8*)(arrayPtr + (arrayIdx * stride) + (sizeof(u8) * coordIdx)), frac);
                        break;
                    case GX_S8:
                        vtxOut.position.coords[coordIdx] = ConvertToFloat<s8>(*(s8*)(arrayPtr + (arrayIdx * stride) + (sizeof(s8) * coordIdx)), frac);
                        break;
                    case GX_U16:
                        vtxOut.position.coords[coordIdx] = ConvertToFloat<u16>(*(u16*)(arrayPtr + (arrayIdx * stride) + (sizeof(u16) * coordIdx)), frac);
                        break;
                    case GX_S16:
                        vtxOut.position.coords[coordIdx] = ConvertToFloat<s16>(*(s16*)(arrayPtr + (arrayIdx * stride) + (sizeof(s16) * coordIdx)), frac);
                        break;
                    case GX_F32:
                        vtxOut.position.coords[coordIdx] = *(f32*)(arrayPtr + (arrayIdx * stride) + (sizeof(f32) * coordIdx)) * std::pow(2, -(frac));
                        break;
                    default:
                        // Bad data type!!!
                        OSReport("SIM::GX::Geometry bad data type!\n");
                        break;
                }
            }
        }
    }

    // Handle color0
    //TODO: for now I just advance bytestreampointer by the amount we want
    byteStreamPointer++;

    // Submit vertex
    SubmitVertex(vtxOut);
  }

  OSReport("Here. Send to the GL Renderer\n");
}

void GeometryProcessor::SubmitVertex(RenderVertex& vtx) {
    auto& gxState = GetGlobalState();

    GXPrimitive primitiveType = gxState.GetCurrentPrimitive();
    
}

}