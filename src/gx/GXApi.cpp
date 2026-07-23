#include <dolphin/types.h>
#include <dolphin/gx.h>

// GXBump.h
void GXSetNumIndStages(u8 nIndStages) {

}

void GXSetIndTexOrder(GXIndTexStageID ind_stage, GXTexCoordID tex_coord, GXTexMapID tex_map) {

}

void GXSetIndTexCoordScale(GXIndTexStageID ind_state, GXIndTexScale scale_s, GXIndTexScale scale_t) {

}

void GXSetIndTexMtx(GXIndTexMtxID mtx_sel, const f32 offset[2][3], s8 scale_exp) {

}

void GXSetTevIndirect(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexFormat format, 
                      GXIndTexBiasSel bias_sel, GXIndTexMtxID matrix_sel,GXIndTexWrap wrap_s, 
                      GXIndTexWrap wrap_t, GXBool add_prev, GXBool ind_lod, GXIndTexAlphaSel alpha_sel) {

}

void GXSetTevDirect(GXTevStageID tev_stage) {

}

void GXSetTevIndWarp(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXBool signed_offset, 
                     GXBool replace_mode, GXIndTexMtxID matrix_sel) {

}

void GXSetTevIndTile(GXTevStageID tev_stage, GXIndTexStageID ind_stage, u16 tilesize_s, 
                     u16 tilesize_t, u16 tilespacing_s, u16 tilespacing_t, GXIndTexFormat format, 
                     GXIndTexMtxID matrix_sel, GXIndTexBiasSel bias_sel, GXIndTexAlphaSel alpha_sel) {

}

void GXSetTevIndBumpST(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexMtxID matrix_sel) {

}

void GXSetTevIndBumpXYZ(GXTevStageID tev_stage, GXIndTexStageID ind_stage, GXIndTexMtxID matrix_sel) {

}

void GXSetTevIndRepeat(GXTevStageID tev_stage) {

}

void __GXSetIndirectMask(u32 mask) {

}

// GXCpu2Efb.h
void GXPokeAlphaMode(GXCompare func, u8 threshold) {

}

void GXPokeAlphaRead(GXAlphaReadMode mode) {

}

void GXPokeAlphaUpdate(GXBool update_enable) {

}

void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src_factor,
                     GXBlendFactor dst_factor, GXLogicOp op) {

}

void GXPokeColorUpdate(GXBool update_enable) {

}

void GXPokeDstAlpha(GXBool enable, u8 alpha) {

}

void GXPokeDither(GXBool dither) {

}

void GXPokeZMode(GXBool compare_enable, GXCompare func,
                 GXBool update_enable) {

}

void GXPokeZ(u16 x, u16 y, u32 z) {

}

void GXPeekZ(u16 x, u16 y, u32* z) {

}

void GXPokeARGB(u16 x, u16 y, u32 color) {

}

void GXPeekARGB(u16 x, u16 y, u32* color) {

}

u32 GXCompressZ16(u32 z24, GXZFmt16 zfmt) {

}

u32 GXDecompressZ16(u32 z16, GXZFmt16 zfmt) {

}

// GXCull.h
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {

}

void GXGetScissor(u32* xOrig, u32* yOrig, u32* wd, u32* ht) {

}

void GXSetCullMode(GXCullMode mode) {

}

void GXGetCullMode(GXCullMode* mode) {

}

void GXSetCoPlanar(GXBool enable) {

}

// GXDispList.h
void GXBeginDisplayList(void* list, u32 size) {

}

u32 GXEndDisplayList(void) {

}

void GXCallDisplayListLE(const void* list, u32 nbytes) {

}

void GXCallDisplayList(const void* list, u32 nbytes) {

}

// GXDraw.h
void GXDrawSphere(u8 numMajor, u8 numMinor) {

}

void GXDrawSphere1(u8 depth) {

}

void GXDrawCylinder(u8 numEdges) {

}

void GXDrawTorus(f32 rc, u8 numc, u8 numt) {

}

void GXDrawCube(void) {

}

void GXDrawDodeca(void) {
    
}

void GXDrawOctahedron(void) {

}

void GXDrawIcosahedron(void) {

}

// GXFifo.h
void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {

}

void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr) {

}

void GXGetFifoPtrs(GXFifoObj* fifo, void** readPtr, void** writePtr) {

}

GXFifoObj* GXGetCPUFifo(void) {

}

GXFifoObj* GXGetGPFifo(void) {

}
/* Compatibility alias for typo occasionally seen in external scans. */
GXFifoObj* GXGetGPFIfo(void) {

}

void GXSetCPUFifo(GXFifoObj* fifo) {

}

void GXSetGPFifo(GXFifoObj* fifo) {

}

void GXSaveCPUFifo(GXFifoObj* fifo) {

}

void GXGetFifoStatus(GXFifoObj* fifo, GXBool* overhi, GXBool* underlow, u32* fifoCount, GXBool* cpu_write,
                     GXBool* gp_read, GXBool* fifowrap) {

}
void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle, GXBool* cmdIdle, GXBool* brkpt) {

}

void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWaterMark, u32 loWaterMark) {

}

GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback cb) {

}

void GXEnableBreakPt(void* break_pt) {

}

void GXDisableBreakPt(void) {

}

OSThread* GXSetCurrentGXThread(void) {

}

OSThread* GXGetCurrentGXThread(void) {

}

// GXFrameBuffer.h
void GXSetCopyClear(GXColor clear_clr, u32 clear_z) {

}

void GXAdjustForOverscan(GXRenderModeObj* rmin, GXRenderModeObj* rmout, u16 hor, u16 ver) {

}

void GXCopyDisp(void* dest, GXBool clear) {

}

void GXSetDispCopyGamma(GXGamma gamma) {

}

void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {

}

void GXSetDispCopyDst(u16 wd, u16 ht) {

}

void GXSetDispCopyFrame2Field(GXCopyMode mode) {

}

u32 GXSetDispCopyYScale(f32 vscale) {

}

f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {

}

u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) {

}

void GXSetCopyFilter(GXBool aa, u8 sample_pattern[12][2], GXBool vf, u8 vfilter[7]) {

}

void GXSetCopyClamp(GXFBClamp clamp) {

}

void GXSetPixelFmt(GXPixelFmt pix_fmt, GXZFmt16 z_fmt) {

}

void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {

}

void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap) {

}

void GXCopyTex(void* dest, GXBool clear) {

}

void GXClearBoundingBox(void) {

}

void GXReadBoundingBox(u16* left, u16* top, u16* right, u16* bottom) {

}

// GXGeometry.h
void GXSetVtxDesc(GXAttr attr, GXAttrType type) {

}

void GXSetVtxDescv(GXVtxDescList* list) {

}

void GXClearVtxDesc(void) {

}

void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac) {

}

void GXSetNumTexGens(u8 nTexGens) {

}

void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts) {

}

void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx, GXBool normalize,
                       u32 postmtx) {

}

void GXSetLineWidth(u8 width, GXTexOffset texOffsets) {

}

void GXSetPointSize(u8 pointSize, GXTexOffset texOffsets) {

}

void GXEnableTexOffsets(GXTexCoordID coord, GXBool line_enable, GXBool point_enable) {

}

void GXSetArray(GXAttr attr, const void* data, u8 stride) {

}

void GXInvalidateVtxCache(void) {

}

void GXSetVtxAttrFmtv(GXVtxFmt vtxfmt, const GXVtxAttrFmtList* list) {

}

// GXGet.h
GXBool GXGetTexObjMipMap(GXTexObj* tex_obj) {

}

GXTexFmt GXGetTexObjFmt(GXTexObj* tex_obj) {

}

u16 GXGetTexObjHeight(GXTexObj* tex_obj) {

}

u16 GXGetTexObjWidth(GXTexObj* tex_obj) {

}

GXTexWrapMode GXGetTexObjWrapS(GXTexObj* tex_obj) {

}

GXTexWrapMode GXGetTexObjWrapT(GXTexObj* tex_obj) {

}

void* GXGetTexObjData(GXTexObj* tex_obj) {

}

void* GXGetTexObjUserData(GXTexObj* tex_obj) {

}

void GXGetTexObjAll(GXTexObj* obj, void** image_ptr, u16* width, u16* height, GXTexFmt* format, GXTexWrapMode* wrap_s,
                    GXTexWrapMode* wrap_t, GXBool* mipmap) {

}

void* GXGetTlutObjData(GXTlutObj* tlut_obj) {

}

GXTlutFmt GXGetTlutObjFmt(GXTlutObj* tlut_obj) {

}

u16 GXGetTlutObjNumEntries(GXTlutObj* tlut_obj) {

}

void GXGetTlutObjAll(GXTlutObj* tlut_obj, void** lut, GXTlutFmt* fmt, u16* n_entries) {

}

void GXGetProjectionv(f32* p) {

}

void GXGetViewportv(f32* vp) {

}

void GXGetLightAttnA(GXLightObj* lt_obj, f32* a0, f32* a1, f32* a2) {

}

void GXGetLightAttnK(GXLightObj* lt_obj, f32* k0, f32* k1, f32* k2) {

}

void GXGetLightPos(GXLightObj* lt_obj, f32* x, f32* y, f32* z) {

}

void GXGetLightDir(GXLightObj* lt_obj, f32* nx, f32* ny, f32* nz) {

}

void GXGetLightColor(GXLightObj* lt_obj, GXColor* color) {

}

void GXGetVtxDesc(GXAttr attr, GXAttrType* type) {

}

void GXGetVtxDescv(GXVtxDescList* list) {

}

void GXGetVtxAttrFmt(GXVtxFmt idx, GXAttr attr, GXCompCnt* compCnt, GXCompType* compType, u8* shift) {

}

void GXGetVtxAttrFmtv(GXVtxFmt idx, GXVtxAttrFmtList* list) {

}

void GXGetArray(GXAttr attr, void** base_ptr, u8* stride) {

}

void GXGetLineWidth(u8* width, GXTexOffset* tex_offsets) {

}

void GXGetPointSize(u8* size, GXTexOffset* tex_offsets) {

}

// GXLighting.h
void GXSetNumChans(u8 nChans) {

}

void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src, u32 light_mask,
                   GXDiffuseFn diff_fn, GXAttnFn attn_fn) {

}

void GXSetChanAmbColor(GXChannelID chan, GXColor amb_color) {

}

void GXSetChanMatColor(GXChannelID chan, GXColor mat_color) {

}

void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, GXSpotFn spot_func) {

}

void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_distance, f32 ref_brightness, GXDistAttnFn dist_func) {

}
void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z) {

}

void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {

}

void GXInitSpecularDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {

}

void GXInitSpecularDirHA(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz, f32 hx, f32 hy, f32 hz) {

}

void GXInitLightColor(GXLightObj* lt_obj, GXColor color) {

}

void GXInitLightAttn(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {

}

void GXInitLightAttnA(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2) {

}

void GXInitLightAttnK(GXLightObj* lt_obj, f32 k0, f32 k1, f32 k2) {

}

void GXLoadLightObjImm(const GXLightObj* lt_obj, GXLightID light) {

}

void GXLoadLightObjIndx(u32 lt_obj_indx, GXLightID light) {

}

// GXManage.h
GXFifoObj* GXInit(void* base, u32 size) {

}

void GXAbortFrame(void) {

}

void GXSetDrawSync(u16 token) {

}

u16 GXReadDrawSync(void) {

}

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) {

}

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb) {

}

void GXDrawDone(void) {

}

void GXSetDrawDone(void) {

}

void GXWaitDrawDone(void) {

}

void GXFlush(void) {

}

void GXResetWriteGatherPipe(void) {

}

void GXPixModeSync(void) {

}

void GXTexModeSync(void) {

}

void GXSetVerifyLevel(GXVerifyLevel level) {

}

GXVerifyCallback GXSetVerifyCallback(GXVerifyCallback cb) {

}

volatile void* GXRedirectWriteGatherPipe(void* ptr) {

}

void GXRestoreWriteGatherPipe(void) {

}

BOOL IsWriteGatherBufferEmpty(void) {

}

void GXSetMisc(u32 token, u32 val) {

}

// GXPerf.h
void GXSetGPMetric(GXPerf0 perf0, GXPerf1 perf1) {

}

void GXClearGPMetric(void) {

}

void GXReadGPMetric(u32* cnt0, u32* cnt1) {

}

u32 GXReadGP0Metric(void) {

}

u32 GXReadGP1Metric(void) {

}

void GXReadMemMetric(
    u32* cp_req, u32* tc_req, u32* cpu_rd_req, u32* cpu_wr_req, u32* dsp_req,
    u32* io_req, u32* vi_req, u32* pe_req, u32* rf_req, u32* fi_req) {

}
void GXClearMemMetric(void) {

}

void GXReadPixMetric(
    u32* top_pixels_in, u32* top_pixels_out, u32* bot_pixels_in, u32* bot_pixels_out,
    u32* clr_pixels_in, u32* copy_clks) {

}

void GXClearPixMetric(void) {

}

void GXSetVCacheMetric(GXVCachePerf attr) {

}

void GXReadVCacheMetric(u32* check, u32* miss, u32* stall) {

}

void GXClearVCacheMetric(void) {

}

void GXReadXfRasMetric(u32* xf_wait_in, u32* xf_wait_out, u32* ras_busy, u32* clocks) {

}

// GXPixel.h
void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {

}

void GXSetFogColor(GXColor color) {

}

void GXInitFogAdjTable(GXFogAdjTable* table, u16 width, const f32 projMtx[4][4]) {

}

void GXSetFogRangeAdj(GXBool doEnable, u16 center, GXFogAdjTable* table) {

}

void GXSetBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor, GXLogicOp op) {

}

void GXSetColorUpdate(GXBool update_enable) {

}

void GXSetAlphaUpdate(GXBool update_enable) {

}

void GXSetZMode(GXBool compare_enable, GXCompare func, GXBool update_enable) {

}

void GXSetZCompLoc(GXBool before_tex) {

}

void GXSetDither(GXBool dither) {

}

void GXSetDstAlpha(GXBool enable, u8 alpha) {

}

void GXSetFieldMask(GXBool odd_mask, GXBool even_mask) {

}

void GXSetFieldMode(GXBool field_mode, GXBool half_aspect_ratio) {

}

// GXTev.h
void GXSetTevOp(GXTevStageID id, GXTevMode mode) {

}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) {

}

void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) {

}

void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp,
                     GXTevRegID out_reg) {

}

void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp,
                     GXTevRegID out_reg) {

}

void GXSetTevColor(GXTevRegID id, GXColor color) {

}

void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) {

}

void GXSetTevClampMode(GXTevStageID stage, GXTevClampMode mode) {

}

void GXSetTevKColor(GXTevKColorID id, GXColor color) {

}

void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) {

}

void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) {

}

void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras_sel, GXTevSwapSel tex_sel) {

}

void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue,
                           GXTevColorChan alpha) {

}

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) {

}

void GXSetZTexture(GXZTexOp op, GXTexFmt fmt, u32 bias) {

}

void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color) {

}

void GXSetNumTevStages(u8 nStages) {

}

// GXTexture.h
void GXInitTexObj(GXTexObj* obj, const void* data, u16 width, u16 height, u32 format, GXTexWrapMode wrapS,
                  GXTexWrapMode wrapT, GXBool mipmap) {

}

void GXInitTexObjCI(GXTexObj* obj, const void* data, u16 width, u16 height, GXCITexFmt format, GXTexWrapMode wrapS,
                    GXTexWrapMode wrapT, GXBool mipmap, u32 tlut) {

}

void GXInitTexObjData(GXTexObj* obj, const void* data) {

}

void GXInitTexObjUserData(GXTexObj* obj, void* user_data) {

}

void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter min_filt, GXTexFilter mag_filt, f32 min_lod, f32 max_lod, f32 lod_bias,
                     GXBool bias_clamp, GXBool do_edge_lod, GXAnisotropy max_aniso) {

}

void GXLoadTexObj(GXTexObj* obj, GXTexMapID id) {

}

u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, GXBool mipmap, u8 max_lod) {

}

void GXInvalidateTexAll() {

}

void GXInitTexObjWrapMode(GXTexObj* obj, GXTexWrapMode s, GXTexWrapMode t) {

}

void GXInitTlutObj(GXTlutObj* obj, void* data, GXTlutFmt format, u16 entries) {

}

void GXLoadTlut(GXTlutObj* obj, GXTlut idx) {

}

void GXSetTexCoordScaleManually(GXTexCoordID coord, GXBool enable, u16 ss, u16 ts) {

}

void GXSetTexCoordCylWrap(GXTexCoordID coord, GXBool s_enable, GXBool t_enable) {

}

void GXInitTexCacheRegion(GXTexRegion* region, GXBool is_32b_mipmap, u32 tmem_even, GXTexCacheSize size_even,
                          u32 tmem_odd, GXTexCacheSize size_odd) {

}

void GXInitTexPreLoadRegion(GXTexRegion* region, u32 tmem_even, u32 size_even, u32 tmem_odd, u32 size_odd) {

}

void GXInitTlutRegion(GXTlutRegion* region, u32 tmem_addr, GXTlutSize tlut_size) {

}

GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback callback) {

}

GXTlutRegionCallback GXSetTlutRegionCallBack(GXTlutRegionCallback callback) {

}

void GXPreLoadEntireTexture(GXTexObj* tex_obj, GXTexRegion* region) {

}

void GXLoadTexObjPreLoaded(GXTexObj* obj, GXTexRegion* region, GXTexMapID id) {

}

void GXInvalidateTexRegion(const GXTexRegion* region) {

}

// GXTransform.h
void GXSetProjection(f32 mtx[4][4], GXProjectionType type) {

}

void GXSetProjectionv(const f32* ptr) {
    
}

void GXLoadPosMtxImm(f32 mtx[3][4], u32 id) {

}

void GXLoadNrmMtxImm(f32 mtx[3][4], u32 id) {

}

void GXLoadNrmMtxImm3x3(f32 mtx[3][3], u32 id) {

}

void GXLoadTexMtxImm(f32 mtx[][4], u32 id, GXTexMtxType type) {

}

void GXLoadPosMtxIndx(u16 index, u32 id) {

}

void GXLoadNrmMtxIndx3x3(u16 index, u32 id) {

}

void GXLoadTexMtxIndx(u16 index, u32 id, GXTexMtxType type) {

}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {

}

void GXSetViewportv(const f32* vp) {

}

void GXSetCurrentMtx(u32 id) {

}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {

}

void GXSetScissorBoxOffset(s32 x_off, s32 y_off) {

}

void GXSetClipMode(GXClipMode mode) {

}

void GXProject(f32 x, f32 y, f32 z, f32 mtx[3][4], f32* pm, f32* vp, f32* sx, f32* sy, f32* sz) {

}