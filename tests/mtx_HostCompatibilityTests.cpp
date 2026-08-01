#include <dolphin/mtx.h>

#include <cmath>
#include <cstddef>
#include <iostream>

namespace {

template <size_t Rows>
bool MatricesEqual(
    const f32 (&left)[Rows][4],
    const f32 (&right)[Rows][4],
    f32 tolerance = 0.00001f) {
    for (size_t row = 0; row < Rows; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            if (std::fabs(left[row][column] - right[row][column]) >
                tolerance) {
                return false;
            }
        }
    }
    return true;
}

bool VectorsEqual(const Vec& left, const Vec& right,
                  f32 tolerance = 0.00001f) {
    return std::fabs(left.x - right.x) <= tolerance &&
           std::fabs(left.y - right.y) <= tolerance &&
           std::fabs(left.z - right.z) <= tolerance;
}

bool ScalarsEqual(f32 left, f32 right, f32 tolerance = 0.00001f) {
    return std::isfinite(left) && std::isfinite(right) &&
           std::fabs(left - right) <= tolerance;
}

template <size_t Rows>
void FillMatrix(f32 (&matrix)[Rows][4], f32 first = 1.0f) {
    f32 value = first;
    for (size_t row = 0; row < Rows; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            matrix[row][column] = value;
            value += 1.0f;
        }
    }
}

bool TestMtxIdentityCopyAndTranspose() {
    Mtx expected;
    Mtx actual;
    FillMatrix(expected, -20.0f);
    FillMatrix(actual, 20.0f);
    C_MTXIdentity(expected);
    PSMTXIdentity(actual);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    Mtx source;
    FillMatrix(source, -5.0f);
    C_MTXCopy(source, expected);
    PSMTXCopy(source, actual);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTXTranspose(source, expected);
    PSMTXTranspose(source, actual);
    return MatricesEqual(expected, actual);
}

bool TestVectorOperations() {
    const Vec left = {3.0f, -4.0f, 12.0f};
    const Vec right = {-2.0f, 5.0f, 7.0f};
    Vec expected;
    Vec actual;

    C_VECAdd(&left, &right, &expected);
    actual = {101.0f, 102.0f, 103.0f};
    PSVECAdd(&left, &right, &actual);
    if (!VectorsEqual(expected, actual)) {
        return false;
    }

    C_VECSubtract(&left, &right, &expected);
    actual = {101.0f, 102.0f, 103.0f};
    PSVECSubtract(&left, &right, &actual);
    if (!VectorsEqual(expected, actual)) {
        return false;
    }

    C_VECScale(&left, &expected, -0.5f);
    actual = {101.0f, 102.0f, 103.0f};
    PSVECScale(&left, &actual, -0.5f);
    if (!VectorsEqual(expected, actual)) {
        return false;
    }

    C_VECNormalize(&left, &expected);
    actual = left;
    PSVECNormalize(&actual, &actual);
    if (!VectorsEqual(expected, actual)) {
        return false;
    }

    if (!ScalarsEqual(C_VECSquareMag(&left), PSVECSquareMag(&left)) ||
        !ScalarsEqual(C_VECMag(&left), PSVECMag(&left)) ||
        !ScalarsEqual(C_VECDotProduct(&left, &right),
                      PSVECDotProduct(&left, &right))) {
        return false;
    }

    C_VECCrossProduct(&left, &right, &expected);
    actual = left;
    PSVECCrossProduct(&actual, &right, &actual);
    if (!VectorsEqual(expected, actual)) {
        return false;
    }

    return ScalarsEqual(C_VECSquareDistance(&left, &right),
                        PSVECSquareDistance(&left, &right)) &&
           ScalarsEqual(C_VECDistance(&left, &right),
                        PSVECDistance(&left, &right));
}

bool TestMtxOperations() {
    const Mtx source = {
        {2.0f, 0.1f, 0.2f, 4.0f},
        {0.3f, 3.0f, 0.4f, 5.0f},
        {0.5f, 0.6f, 4.0f, 6.0f},
    };
    Mtx expected;
    Mtx actual;

    if (C_MTXInverse(source, expected) !=
        PSMTXInverse(source, actual)) {
        return false;
    }
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    if (C_MTXInvXpose(source, expected) !=
        PSMTXInvXpose(source, actual)) {
        return false;
    }
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTXRotRad(expected, 'y', 0.37f);
    PSMTXRotRad(actual, 'y', 0.37f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    const Vec axis = {1.0f, 2.0f, 3.0f};
    C_MTXRotAxisRad(expected, &axis, 0.61f);
    PSMTXRotAxisRad(actual, &axis, 0.61f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTXTransApply(source, expected, 7.0f, -2.0f, 0.5f);
    PSMTXTransApply(source, actual, 7.0f, -2.0f, 0.5f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTXScale(expected, 2.0f, 3.0f, 4.0f);
    PSMTXScale(actual, 2.0f, 3.0f, 4.0f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTXScaleApply(source, expected, 2.0f, 3.0f, 4.0f);
    PSMTXScaleApply(source, actual, 2.0f, 3.0f, 4.0f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    const Quaternion quaternion = {0.2f, -0.3f, 0.4f, 0.8f};
    C_MTXQuat(expected, &quaternion);
    PSMTXQuat(actual, &quaternion);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    const Vec point = {1.0f, 2.0f, 3.0f};
    const Vec normal = {0.0f, 1.0f, 0.0f};
    C_MTXReflect(expected, &point, &normal);
    PSMTXReflect(actual, &point, &normal);
    return MatricesEqual(expected, actual);
}

bool TestMtx44IdentityCopyAndTranspose() {
    Mtx44 expected;
    Mtx44 actual;
    FillMatrix(expected, -20.0f);
    FillMatrix(actual, 20.0f);
    C_MTX44Identity(expected);
    PSMTX44Identity(actual);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    Mtx44 source;
    FillMatrix(source, -5.0f);
    C_MTX44Copy(source, expected);
    PSMTX44Copy(source, actual);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTX44Transpose(source, expected);
    PSMTX44Transpose(source, actual);
    return MatricesEqual(expected, actual);
}

bool TestMtx44Operations() {
    const Mtx44 source = {
        {2.0f, 0.1f, 0.2f, 4.0f},
        {0.3f, 3.0f, 0.4f, 5.0f},
        {0.5f, 0.6f, 4.0f, 6.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    const Mtx44 right = {
        {1.0f, 2.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 3.0f, 2.0f},
        {4.0f, 0.0f, 1.0f, 3.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    Mtx44 expected;
    Mtx44 actual;

    C_MTX44Concat(source, right, expected);
    PSMTX44Concat(source, right, actual);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTX44Trans(expected, 7.0f, -2.0f, 0.5f);
    PSMTX44Trans(actual, 7.0f, -2.0f, 0.5f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTX44TransApply(source, expected, 7.0f, -2.0f, 0.5f);
    PSMTX44TransApply(source, actual, 7.0f, -2.0f, 0.5f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTX44Scale(expected, 2.0f, 3.0f, 4.0f);
    PSMTX44Scale(actual, 2.0f, 3.0f, 4.0f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTX44ScaleApply(source, expected, 2.0f, 3.0f, 4.0f);
    PSMTX44ScaleApply(source, actual, 2.0f, 3.0f, 4.0f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    C_MTX44RotRad(expected, 'z', 0.37f);
    PSMTX44RotRad(actual, 'z', 0.37f);
    if (!MatricesEqual(expected, actual)) {
        return false;
    }

    const Vec axis = {1.0f, 2.0f, 3.0f};
    C_MTX44RotAxisRad(expected, &axis, 0.61f);
    PSMTX44RotAxisRad(actual, &axis, 0.61f);
    return MatricesEqual(expected, actual);
}

}  // namespace

int main() {
    if (!TestVectorOperations()) {
        std::cerr << "paired-single vector fallback compatibility failed\n";
        return 1;
    }
    if (!TestMtxIdentityCopyAndTranspose()) {
        std::cerr << "3x4 identity/copy/transpose compatibility failed\n";
        return 1;
    }
    if (!TestMtxOperations()) {
        std::cerr << "3x4 paired-single fallback compatibility failed\n";
        return 1;
    }
    if (!TestMtx44IdentityCopyAndTranspose()) {
        std::cerr << "4x4 identity/copy/transpose compatibility failed\n";
        return 1;
    }
    if (!TestMtx44Operations()) {
        std::cerr << "4x4 paired-single fallback compatibility failed\n";
        return 1;
    }
    return 0;
}
