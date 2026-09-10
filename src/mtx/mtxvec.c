#include <dolphin.h>
#include <dolphin/mtx.h>

#define qr0 0

#ifndef LIBPORPOISE_PORT
asm 
#endif
void PSMTXMultVec(const register Mtx44 m, const register Vec* src, register Vec* dst)
{
#ifdef __MWERKS__ // clang-format off
	nofralloc
	psq_l f0, Vec.x(src), 0, qr0
	psq_l f2, 0(m), 0, qr0
	psq_l f1, Vec.z(src), 1, qr0
	ps_mul f4, f2, f0
	psq_l f3, 8(m), 0, qr0
	ps_madd f5, f3, f1, f4
	psq_l f8, 16(m), 0, qr0
	ps_sum0 f6, f5, f6, f5
	psq_l f9, 24(m), 0, qr0
	ps_mul f10, f8, f0
	psq_st f6, Vec.x(dst), 1, qr0
	ps_madd f11, f9, f1, f10
	psq_l f2, 32(m), 0, qr0
	ps_sum0 f12, f11, f12, f11
	psq_l f3, 40(m), 0, qr0
	ps_mul f4, f2, f0
	psq_st f12, Vec.y(dst), 1, qr0
	ps_madd f5, f3, f1, f4
	ps_sum0 f6, f5, f6, f5
	psq_st f6, Vec.z(dst), 1, qr0
	blr
#endif // clang-format on
#ifdef LIBPORPOISE_PORT
	C_MTXMultVec(m, src, dst);
#endif
}

#ifndef LIBPORPOISE_PORT
asm 
#endif
void PSMTXMultVecArray(const register Mtx m, const register Vec* srcBase,
                           register Vec* dstBase, register u32 count)
{
#ifdef __MWERKS__ // clang-format off
	psq_l      f13, 0x0(m), 0, qr0
	psq_l      f12, 0x10(m), 0, qr0
	subi       count, count, 0x1
	psq_l      f11, 0x8(m), 0, qr0
	ps_merge00 f0, f13, f12
	subi       dstBase, dstBase, 0x4
	psq_l      f10, 0x18(m), 0, qr0
	ps_merge11 f1, f13, f12

	mtctr count
	psq_l      f4, 0x20(m), 0, qr0
	ps_merge00 f2, f11, f10
	psq_l      f5, 0x28(m), 0, qr0
	ps_merge11 f3, f11, f10
	psq_l      f6, Vec.x(srcBase), 0, qr0
	psq_lu     f7, Vec.z(srcBase), 1, qr0
	ps_madds0  f8, f0, f6, f3
	ps_mul     f9, f4, f6
	ps_madds1  f8, f1, f6, f8
	ps_madd    f10, f5, f7, f9

loop:
	psq_lu    f6, Vec.y(srcBase), 0, qr0
	ps_madds0 f12, f2, f7, f8
	psq_lu    f7, Vec.z(srcBase), 1, qr0
	ps_sum0   f13, f10, f9, f10
	ps_madds0 f8, f0, f6, f3
	ps_mul    f9, f4, f6
	psq_stu   f12, 0x4(dstBase), 0, qr0
	ps_madds1 f8, f1, f6, f8
	psq_stu   f13, 0x8(dstBase), 1, qr0
	ps_madd   f10, f5, f7, f9
	bdnz loop

	ps_madds0 f12, f2, f7, f8
	ps_sum0   f13, f10, f9, f10
	psq_stu   f12, 0x4(dstBase), 0, qr0
	psq_stu   f13, 0x8(dstBase), 1, qr0
#endif // clang-format on
#ifdef LIBPORPOISE_PORT
	C_MTXMultVecArray(m, srcBase, dstBase, count);
#endif
}

#ifndef LIBPORPOISE_PORT
asm 
#endif
void PSMTXMultVecSR(const register Mtx44 m, const register Vec* src, register Vec* dst)
{
#ifdef __MWERKS__ // clang-format off
	psq_l   f0, 0x0(m), 0, qr0
	psq_l   f6, Vec.x(src), 0, qr0
	psq_l   f2, 0x10(m), 0, qr0
	ps_mul  f8, f0, f6
	psq_l   f4, 0x20(m), 0, qr0
	ps_mul  f10, f2, f6
	psq_l   f7, Vec.z(src), 1, qr0
	ps_mul  f12, f4, f6
	psq_l   f3, 0x18(m), 0, qr0
	ps_sum0 f8, f8, f8, f8
	psq_l   f5, 0x28(m), 0, qr0
	ps_sum0 f10, f10, f10, f10
	psq_l   f1, 0x8(m), 0, qr0
	ps_sum0 f12, f12, f12, f12
	ps_madd f9, f1, f7, f8
	psq_st  f9, Vec.x(dst), 1, qr0
	ps_madd f11, f3, f7, f10
	psq_st  f11, Vec.y(dst), 1, qr0
	ps_madd f13, f5, f7, f12
	psq_st  f13, Vec.z(dst), 1, qr0
#endif // clang-format on
#ifdef LIBPORPOISE_PORT
	C_MTXMultVecSR(m, src, dst);
#endif
}


void C_MTXMultVec(const register Mtx44 m, const register Vec* src, register Vec* dst)
{
    f32 x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z + m[0][3];
    f32 y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z + m[1][3];
    f32 z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z + m[2][3];
    dst->x = x; dst->y = y; dst->z = z;
}

void C_MTXMultVecArray(const register Mtx m, const register Vec* srcBase,
                           register Vec* dstBase, register u32 count)
{
    for (u32 i = 0; i < count; i++) {
        PSMTXMultVec(m, &srcBase[i], &dstBase[i]);
    }
}

void C_MTXMultVecSR(const register Mtx44 m, const register Vec* src, register Vec* dst)
{
    f32 x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z;
    f32 y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z;
    f32 z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z;
    dst->x = x; dst->y = y; dst->z = z;
}