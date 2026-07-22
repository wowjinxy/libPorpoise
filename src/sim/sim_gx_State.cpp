#include <simulator/sim_gx_State.hpp>

static SIM::GX::GlobalState sGXGlobalState ={};

namespace SIM::GX {

GlobalState::GlobalState() {}

size_t GlobalState::GetNumBytesPerVertex() {
    //TODO
    return 2;
}

void GlobalState::SetCurrentPrimitive(GXPrimitive primitive) {
    mCurrentPrimitive = primitive;
}

void GlobalState::SetCurrentVertexFormat(GXVtxFmt format) {
    mCurrentVertexFormat = format;
}

void GlobalState::SetVertexArray(GXAttr attr, VertexArray array) {
    mVertexArrays[attr] = array;
}

void GlobalState::SetVertexDescriptor(GXAttr attr, GXAttrType descType) {
    mVertexDescriptors[attr] = descType;
}

void GlobalState::SetVertexFormatComponents(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompCnt component) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mComponents = component;
}

void GlobalState::SetVertexFormatDataType(GXVtxFmt formatIndex, GXAttr attrIndex, GXCompType dataType) {
    mVertexFormats[formatIndex].mAttributes[attrIndex].mDataType = dataType;
}



void InitGlobalState() {

}

GlobalState& GetGlobalState() {
    return sGXGlobalState;
}
}