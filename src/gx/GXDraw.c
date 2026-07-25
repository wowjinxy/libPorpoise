#include <dolphin/gx.h>

#include <math.h>

#define GX_DRAW_PI 3.14159265358979323846f
#define GX_DRAW_SQRT3 1.732050808f

static GXVtxDescList s_draw_vcd[GX_MAX_VTXDESCLIST_SZ];
static GXVtxAttrFmtList s_draw_vat[GX_MAX_VTXATTRFMTLIST_SZ];

static void SaveAndSetVertexState(void)
{
	GXGetVtxDescv(s_draw_vcd);
	GXGetVtxAttrFmtv(GX_VTXFMT3, s_draw_vat);

	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_NRM, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
}

static void RestoreVertexState(void)
{
	GXSetVtxDescv(s_draw_vcd);
	GXSetVtxAttrFmtv(GX_VTXFMT3, s_draw_vat);
}

static void SubtractVector(const f32* first, const f32* second, f32* result)
{
	result[0] = second[0] - first[0];
	result[1] = second[1] - first[1];
	result[2] = second[2] - first[2];
}

static void CrossVector(const f32* first, const f32* second, f32* result)
{
	f32 x = first[1] * second[2] - first[2] * second[1];
	f32 y = first[2] * second[0] - first[0] * second[2];
	f32 z = first[0] * second[1] - first[1] * second[0];

	result[0] = x;
	result[1] = y;
	result[2] = z;
}

static void NormalizeVector(f32* vector)
{
	f32 length = sqrtf(
		vector[0] * vector[0] +
		vector[1] * vector[1] +
		vector[2] * vector[2]);

	if (length <= 1.0e-12f) {
		vector[0] = 0.0f;
		vector[1] = 0.0f;
		vector[2] = 1.0f;
		return;
	}

	vector[0] /= length;
	vector[1] /= length;
	vector[2] /= length;
}

static void EmitVertex(const f32* position, const f32* normal)
{
	GXPosition3f32(position[0], position[1], position[2]);
	GXNormal3f32(normal[0], normal[1], normal[2]);
}

static void DrawTriangle(f32* first, f32* second, f32* third)
{
	GXBegin(GX_TRIANGLES, GX_VTXFMT3, 3);
	EmitVertex(first, first);
	EmitVertex(second, second);
	EmitVertex(third, third);
	GXEnd();
}

static void SubdivideTriangle(u8 depth, f32* first, f32* second, f32* third)
{
	f32 first_second[3];
	f32 second_third[3];
	f32 third_first[3];
	u32 component;

	if (depth == 0) {
		DrawTriangle(first, second, third);
		return;
	}

	for (component = 0; component < 3; ++component) {
		first_second[component] = first[component] + second[component];
		second_third[component] = second[component] + third[component];
		third_first[component] = third[component] + first[component];
	}

	NormalizeVector(first_second);
	NormalizeVector(second_third);
	NormalizeVector(third_first);

	SubdivideTriangle((u8)(depth - 1), first, first_second, third_first);
	SubdivideTriangle((u8)(depth - 1), second, second_third, first_second);
	SubdivideTriangle((u8)(depth - 1), third, third_first, second_third);
	SubdivideTriangle((u8)(depth - 1), first_second, second_third, third_first);
}

static void DrawIndexedTriangle(
	u8 depth,
	u8 triangle,
	f32 vertices[][3],
	const u8 indices[][3])
{
	SubdivideTriangle(
		depth,
		vertices[indices[triangle][0]],
		vertices[indices[triangle][1]],
		vertices[indices[triangle][2]]);
}

void GXDrawCylinder(u8 numEdges)
{
	s32 edge;
	f32 x[100];
	f32 y[100];
	const f32 top = 1.0f;
	const f32 bottom = -1.0f;

	if (numEdges < 3)
		numEdges = 3;
	if (numEdges > 99)
		numEdges = 99;

	SaveAndSetVertexState();

	for (edge = 0; edge <= numEdges; ++edge) {
		f32 angle = edge * 2.0f * GX_DRAW_PI / numEdges;
		x[edge] = cosf(angle);
		y[edge] = sinf(angle);
	}

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (u16)((numEdges + 1) * 2));
	for (edge = 0; edge <= numEdges; ++edge) {
		GXPosition3f32(x[edge], y[edge], bottom);
		GXNormal3f32(x[edge], y[edge], 0.0f);
		GXPosition3f32(x[edge], y[edge], top);
		GXNormal3f32(x[edge], y[edge], 0.0f);
	}
	GXEnd();

	GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, (u16)(numEdges + 2));
	GXPosition3f32(0.0f, 0.0f, top);
	GXNormal3f32(0.0f, 0.0f, 1.0f);
	for (edge = 0; edge <= numEdges; ++edge) {
		GXPosition3f32(x[edge], -y[edge], top);
		GXNormal3f32(0.0f, 0.0f, 1.0f);
	}
	GXEnd();

	GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, (u16)(numEdges + 2));
	GXPosition3f32(0.0f, 0.0f, bottom);
	GXNormal3f32(0.0f, 0.0f, -1.0f);
	for (edge = 0; edge <= numEdges; ++edge) {
		GXPosition3f32(x[edge], y[edge], bottom);
		GXNormal3f32(0.0f, 0.0f, -1.0f);
	}
	GXEnd();

	RestoreVertexState();
}

void GXDrawTorus(f32 rc, u8 numc, u8 numt)
{
	GXAttrType textureType;
	f32 torusRadius;
	s32 circle;
	s32 section;
	s32 side;

	if (rc <= 0.0f)
		rc = 0.25f;
	if (rc >= 1.0f)
		rc = 0.999f;
	if (numc < 3)
		numc = 3;
	if (numt < 3)
		numt = 3;

	torusRadius = 1.0f - rc;
	GXGetVtxDesc(GX_VA_TEX0, &textureType);
	SaveAndSetVertexState();
	if (textureType != GX_NONE) {
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GXSetVtxAttrFmt(
			GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	}

	for (circle = 0; circle < numc; ++circle) {
		GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (u16)((numt + 1) * 2));
		for (section = 0; section <= numt; ++section) {
			for (side = 1; side >= 0; --side) {
				f32 s = (f32)((circle + side) % numc);
				f32 t = (f32)(section % numt);
				f32 sectionAngle = s * 2.0f * GX_DRAW_PI / numc;
				f32 circleAngle = t * 2.0f * GX_DRAW_PI / numt;
				f32 sectionCos = cosf(sectionAngle);
				f32 circleCos = cosf(circleAngle);
				f32 circleSin = sinf(circleAngle);
				f32 x = (torusRadius - rc * sectionCos) * circleCos;
				f32 y = (torusRadius - rc * sectionCos) * circleSin;
				f32 z = rc * sinf(sectionAngle);

				GXPosition3f32(x, y, z);
				GXNormal3f32(
					-circleCos * sectionCos,
					-circleSin * sectionCos,
					sinf(sectionAngle));
				if (textureType != GX_NONE) {
					GXTexCoord2f32(
						(f32)(circle + side) / numc,
						(f32)section / numt);
				}
			}
		}
		GXEnd();
	}

	RestoreVertexState();
}

void GXDrawSphere(u8 numMajor, u8 numMinor)
{
	GXAttrType textureType;
	f32 majorStep;
	f32 minorStep;
	s32 major;
	s32 minor;

	if (numMajor < 2)
		numMajor = 2;
	if (numMinor < 3)
		numMinor = 3;

	majorStep = GX_DRAW_PI / numMajor;
	minorStep = 2.0f * GX_DRAW_PI / numMinor;
	GXGetVtxDesc(GX_VA_TEX0, &textureType);
	SaveAndSetVertexState();
	if (textureType != GX_NONE) {
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GXSetVtxAttrFmt(
			GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	}

	for (major = 0; major < numMajor; ++major) {
		f32 firstAngle = major * majorStep;
		f32 secondAngle = firstAngle + majorStep;
		f32 firstRadius = sinf(firstAngle);
		f32 secondRadius = sinf(secondAngle);
		f32 firstZ = cosf(firstAngle);
		f32 secondZ = cosf(secondAngle);

		GXBegin(
			GX_TRIANGLESTRIP,
			GX_VTXFMT3,
			(u16)((numMinor + 1) * 2));
		for (minor = 0; minor <= numMinor; ++minor) {
			f32 angle = minor * minorStep;
			f32 x = cosf(angle);
			f32 y = sinf(angle);

			GXPosition3f32(
				x * secondRadius, y * secondRadius, secondZ);
			GXNormal3f32(
				x * secondRadius, y * secondRadius, secondZ);
			if (textureType != GX_NONE) {
				GXTexCoord2f32(
					(f32)minor / numMinor,
					(f32)(major + 1) / numMajor);
			}

			GXPosition3f32(x * firstRadius, y * firstRadius, firstZ);
			GXNormal3f32(x * firstRadius, y * firstRadius, firstZ);
			if (textureType != GX_NONE) {
				GXTexCoord2f32(
					(f32)minor / numMinor,
					(f32)major / numMajor);
			}
		}
		GXEnd();
	}

	RestoreVertexState();
}

static void DrawCubeFace(
	f32 nx, f32 ny, f32 nz,
	f32 tx, f32 ty, f32 tz,
	f32 bx, f32 by, f32 bz,
	GXAttrType binormal,
	GXAttrType texture)
{
	const f32 size = 1.0f / GX_DRAW_SQRT3;
	const f32 signs[4][2] = {
		{ 1.0f,  1.0f},
		{-1.0f,  1.0f},
		{-1.0f, -1.0f},
		{ 1.0f, -1.0f},
	};
	s32 vertex;

	for (vertex = 0; vertex < 4; ++vertex) {
		f32 tangentSign = signs[vertex][0];
		f32 binormalSign = signs[vertex][1];

		GXPosition3f32(
			(nx + tangentSign * tx + binormalSign * bx) * size,
			(ny + tangentSign * ty + binormalSign * by) * size,
			(nz + tangentSign * tz + binormalSign * bz) * size);
		GXNormal3f32(nx, ny, nz);
		if (binormal != GX_NONE) {
			GXNormal3f32(tx, ty, tz);
			GXNormal3f32(bx, by, bz);
		}
		if (texture != GX_NONE) {
			GXTexCoord2s8(
				tangentSign > 0.0f ? 1 : 0,
				binormalSign > 0.0f ? 1 : 0);
		}
	}
}

void GXDrawCube(void)
{
	GXAttrType normalType;
	GXAttrType textureType;

	GXGetVtxDesc(GX_VA_NBT, &normalType);
	GXGetVtxDesc(GX_VA_TEX0, &textureType);
	SaveAndSetVertexState();

	if (normalType != GX_NONE) {
		GXSetVtxDesc(GX_VA_NBT, GX_DIRECT);
		GXSetVtxAttrFmt(
			GX_VTXFMT3, GX_VA_NBT, GX_NRM_NBT, GX_F32, 0);
	}
	if (textureType != GX_NONE) {
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GXSetVtxAttrFmt(
			GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_S8, 0);
	}

	GXBegin(GX_QUADS, GX_VTXFMT3, 24);
	DrawCubeFace(
		-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 1.0f, 0.0f, normalType, textureType);
	DrawCubeFace(
		1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, -1.0f, normalType, textureType);
	DrawCubeFace(
		0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, normalType, textureType);
	DrawCubeFace(
		0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, 0.0f, 0.0f, normalType, textureType);
	DrawCubeFace(
		0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f, normalType, textureType);
	DrawCubeFace(
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, normalType, textureType);
	GXEnd();

	RestoreVertexState();
}

static const u32 s_dodecahedron_polygons[12][5] = {
	{0, 12, 10, 11, 16},
	{1, 17, 8, 9, 13},
	{2, 14, 9, 8, 18},
	{3, 19, 11, 10, 15},
	{4, 14, 2, 3, 15},
	{5, 12, 0, 1, 13},
	{6, 17, 1, 0, 16},
	{7, 19, 3, 2, 18},
	{8, 17, 6, 7, 18},
	{9, 14, 4, 5, 13},
	{10, 12, 5, 4, 15},
	{11, 19, 7, 6, 16},
};

static f32 s_dodecahedron_vertices[20][3] = {
	{-0.809015f, 0.0f, 0.309015f},
	{-0.809015f, 0.0f, -0.309015f},
	{0.809015f, 0.0f, -0.309015f},
	{0.809015f, 0.0f, 0.309015f},
	{0.309015f, -0.809015f, 0.0f},
	{-0.309015f, -0.809015f, 0.0f},
	{-0.309015f, 0.809015f, 0.0f},
	{0.309015f, 0.809015f, 0.0f},
	{0.0f, 0.309015f, -0.809015f},
	{0.0f, -0.309015f, -0.809015f},
	{0.0f, -0.309015f, 0.809015f},
	{0.0f, 0.309015f, 0.809015f},
	{-0.5f, -0.5f, 0.5f},
	{-0.5f, -0.5f, -0.5f},
	{0.5f, -0.5f, -0.5f},
	{0.5f, -0.5f, 0.5f},
	{-0.5f, 0.5f, 0.5f},
	{-0.5f, 0.5f, -0.5f},
	{0.5f, 0.5f, -0.5f},
	{0.5f, 0.5f, 0.5f},
};

void GXDrawDodeca(void)
{
	u32 polygon;

	SaveAndSetVertexState();

	for (polygon = 0; polygon < 12; ++polygon) {
		f32* first =
			s_dodecahedron_vertices[s_dodecahedron_polygons[polygon][0]];
		f32* second =
			s_dodecahedron_vertices[s_dodecahedron_polygons[polygon][1]];
		f32* third =
			s_dodecahedron_vertices[s_dodecahedron_polygons[polygon][2]];
		f32 firstEdge[3];
		f32 secondEdge[3];
		f32 normal[3];

		SubtractVector(second, third, firstEdge);
		SubtractVector(second, first, secondEdge);
		CrossVector(firstEdge, secondEdge, normal);
		NormalizeVector(normal);

		GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, 5);
		EmitVertex(
			s_dodecahedron_vertices[
				s_dodecahedron_polygons[polygon][4]],
			normal);
		EmitVertex(
			s_dodecahedron_vertices[
				s_dodecahedron_polygons[polygon][3]],
			normal);
		EmitVertex(third, normal);
		EmitVertex(second, normal);
		EmitVertex(first, normal);
		GXEnd();
	}

	RestoreVertexState();
}

static f32 s_octahedron_vertices[6][3] = {
	{1.0f, 0.0f, 0.0f},
	{-1.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f},
	{0.0f, -1.0f, 0.0f},
	{0.0f, 0.0f, 1.0f},
	{0.0f, 0.0f, -1.0f},
};

static const u8 s_octahedron_indices[8][3] = {
	{0, 4, 2},
	{1, 2, 4},
	{0, 3, 4},
	{1, 4, 3},
	{0, 2, 5},
	{1, 5, 2},
	{0, 5, 3},
	{1, 3, 5},
};

static f32 s_icosahedron_vertices[12][3] = {
	{-0.525731112119133606f, 0.0f, 0.850650808352039932f},
	{0.525731112119133606f, 0.0f, 0.850650808352039932f},
	{-0.525731112119133606f, 0.0f, -0.850650808352039932f},
	{0.525731112119133606f, 0.0f, -0.850650808352039932f},
	{0.0f, 0.850650808352039932f, 0.525731112119133606f},
	{0.0f, 0.850650808352039932f, -0.525731112119133606f},
	{0.0f, -0.850650808352039932f, 0.525731112119133606f},
	{0.0f, -0.850650808352039932f, -0.525731112119133606f},
	{0.850650808352039932f, 0.525731112119133606f, 0.0f},
	{-0.850650808352039932f, 0.525731112119133606f, 0.0f},
	{0.850650808352039932f, -0.525731112119133606f, 0.0f},
	{-0.850650808352039932f, -0.525731112119133606f, 0.0f},
};

static const u8 s_icosahedron_indices[20][3] = {
	{0, 4, 1},
	{0, 9, 4},
	{9, 5, 4},
	{4, 5, 8},
	{4, 8, 1},
	{8, 10, 1},
	{8, 3, 10},
	{5, 3, 8},
	{5, 2, 3},
	{2, 7, 3},
	{7, 10, 3},
	{7, 6, 10},
	{7, 11, 6},
	{11, 0, 6},
	{0, 1, 6},
	{6, 1, 10},
	{9, 0, 11},
	{9, 11, 2},
	{9, 2, 5},
	{7, 2, 11},
};

void GXDrawOctahedron(void)
{
	s32 triangle;

	SaveAndSetVertexState();
	for (triangle = 7; triangle >= 0; --triangle) {
		DrawIndexedTriangle(
			0,
			(u8)triangle,
			s_octahedron_vertices,
			s_octahedron_indices);
	}
	RestoreVertexState();
}

void GXDrawIcosahedron(void)
{
	s32 triangle;

	SaveAndSetVertexState();
	for (triangle = 19; triangle >= 0; --triangle) {
		DrawIndexedTriangle(
			0,
			(u8)triangle,
			s_icosahedron_vertices,
			s_icosahedron_indices);
	}
	RestoreVertexState();
}

void GXDrawSphere1(u8 depth)
{
	s32 triangle;

	SaveAndSetVertexState();
	for (triangle = 19; triangle >= 0; --triangle) {
		DrawIndexedTriangle(
			depth,
			(u8)triangle,
			s_icosahedron_vertices,
			s_icosahedron_indices);
	}
	RestoreVertexState();
}

static u32 s_normal_count;
static f32* s_normal_table;

static GXBool NormalExists(const f32* normal)
{
	u32 entry;

	for (entry = 0; entry < s_normal_count; ++entry) {
		const f32* existing = &s_normal_table[entry * 3];
		if (normal[0] == existing[0] &&
		    normal[1] == existing[1] &&
		    normal[2] == existing[2])
			return GX_TRUE;
	}

	return GX_FALSE;
}

static void AddNormal(const f32* normal)
{
	u32 offset;

	if (NormalExists(normal))
		return;

	offset = s_normal_count * 3;
	s_normal_table[offset] = normal[0];
	s_normal_table[offset + 1] = normal[1];
	s_normal_table[offset + 2] = normal[2];
	++s_normal_count;
}

static void SubdivideNormals(
	u8 depth,
	f32* first,
	f32* second,
	f32* third)
{
	f32 first_second[3];
	f32 second_third[3];
	f32 third_first[3];
	u32 component;

	if (depth == 0) {
		AddNormal(first);
		AddNormal(second);
		AddNormal(third);
		return;
	}

	for (component = 0; component < 3; ++component) {
		first_second[component] = first[component] + second[component];
		second_third[component] = second[component] + third[component];
		third_first[component] = third[component] + first[component];
	}

	NormalizeVector(first_second);
	NormalizeVector(second_third);
	NormalizeVector(third_first);

	SubdivideNormals((u8)(depth - 1), first, first_second, third_first);
	SubdivideNormals((u8)(depth - 1), second, second_third, first_second);
	SubdivideNormals((u8)(depth - 1), third, third_first, second_third);
	SubdivideNormals(
		(u8)(depth - 1), first_second, second_third, third_first);
}

u32 GXGenNormalTable(u8 depth, f32* table)
{
	s32 triangle;

	if (table == NULL)
		return 0;

	s_normal_count = 0;
	s_normal_table = table;
	for (triangle = 7; triangle >= 0; --triangle) {
		const u8* indices = s_octahedron_indices[triangle];
		SubdivideNormals(
			depth,
			s_octahedron_vertices[indices[0]],
			s_octahedron_vertices[indices[1]],
			s_octahedron_vertices[indices[2]]);
	}

	return s_normal_count;
}
