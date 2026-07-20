#ifndef MATRIX_H
#define MATRIX_H

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include "Constants.h"
#include "Vector.h"

using namespace std;

/// <summary>
/// 提供可自定义尺寸的方阵类及常用几何变换矩阵工厂，
/// 支持矩阵乘法、求逆、转置、向量变换，以及2D/3D的平移、旋转、缩放、观察矩阵构建与统一应用接口。
/// </summary>

class Matrix_my {
private:
    size_t size;
    vector<vector<double>> data;
public:
    Matrix_my(size_t s = 3) : size(s), data(s, vector<double>(s, 0.0)) {}

    void Initialize() {
        for (size_t i = 0; i < size; i++) { for (size_t j = 0; j < size; j++) data[i][j] = (i == j) ? 1.0 : 0.0; }
    }

    size_t GetSize() const { return size; }

    double& operator()(size_t r, size_t c) { return data.at(r).at(c); }
    const double& operator()(size_t r, size_t c) const { return data.at(r).at(c); }

    Matrix_my operator*(const Matrix_my& other) const {
        if (size != other.size) throw invalid_argument("Matrix sizes mismatch");
        Matrix_my result(size);
        for (size_t i = 0; i < size; i++)
            for (size_t j = 0; j < size; j++) {
                double s = 0;
                for (size_t k = 0; k < size; k++) s += (*this)(i, k) * other(k, j);
                result(i, j) = (abs(s) < EPSILON ? 0.0 : s);
            }
        return result;
    }
    Vector3 operator*(const Vector3& v) const {
        if (size != 3) throw invalid_argument("3x3 required");
        return { (*this)(0,0) * v.x + (*this)(0,1) * v.y + (*this)(0,2) * v.z,
                 (*this)(1,0) * v.x + (*this)(1,1) * v.y + (*this)(1,2) * v.z,
                 (*this)(2,0) * v.x + (*this)(2,1) * v.y + (*this)(2,2) * v.z };
    }
    Vector4 operator*(const Vector4& v) const {
        if (size != 4) throw invalid_argument("4x4 required");
        return { (*this)(0,0) * v.x + (*this)(0,1) * v.y + (*this)(0,2) * v.z + (*this)(0,3) * v.w,
                 (*this)(1,0) * v.x + (*this)(1,1) * v.y + (*this)(1,2) * v.z + (*this)(1,3) * v.w,
                 (*this)(2,0) * v.x + (*this)(2,1) * v.y + (*this)(2,2) * v.z + (*this)(2,3) * v.w,
                 (*this)(3,0) * v.x + (*this)(3,1) * v.y + (*this)(3,2) * v.z + (*this)(3,3) * v.w };
    }
    //逆矩阵
    Matrix_my Inverse() const {
        size_t n = size;
        Matrix_my A(*this);       // 拷贝原矩阵
        Matrix_my inv(n);
        inv.Initialize();         // 初始为单位矩阵

        for (size_t i = 0; i < n; i++) {
            // 寻找主元
            double pivot = A(i, i);
            size_t pivotRow = i;
            for (size_t j = i + 1; j < n; j++) {
                if (abs(A(j, i)) > abs(pivot)) {
                    pivot = A(j, i);
                    pivotRow = j;
                }
            }

            if (abs(pivot) < 1e-10)
                throw runtime_error("Matrix is singular and cannot be inverted.");

            // 交换行
            if (pivotRow != i) {
                swap(A.data[i], A.data[pivotRow]);
                swap(inv.data[i], inv.data[pivotRow]);
            }

            // 将主元归一
            double factor = A(i, i);
            for (size_t j = 0; j < n; j++) {
                A(i, j) /= factor;
                inv(i, j) /= factor;
            }

            // 消去其他行
            for (size_t j = 0; j < n; j++) {
                if (j == i) continue;
                double f = A(j, i);
                for (size_t k = 0; k < n; k++) {
                    A(j, k) -= f * A(i, k);
                    inv(j, k) -= f * inv(i, k);
                }
            }
        }

        return inv;
    }
    //转置
    Matrix_my Transpose() const {
        Matrix_my result(size);
        for (size_t i = 0; i < size; i++) {
            for (size_t j = 0; j < size; j++) {
                result(j, i) = (*this)(i, j);
            }
        }
        return result;
    }
};
// Vector3 * Matrix
Vector3 operator*(const Vector3& vec, const Matrix_my& mat) {
    if (mat.GetSize() != 3)
        throw invalid_argument("Vector3 * Matrix is only supported for 3x3 matrix.");
    return Vector3{
        vec.x * mat(0, 0) + vec.y * mat(1, 0) + vec.z * mat(2, 0),
        vec.x * mat(0, 1) + vec.y * mat(1, 1) + vec.z * mat(2, 1),
        vec.x * mat(0, 2) + vec.y * mat(1, 2) + vec.z * mat(2, 2)
    };
}
// Vector4 * Matrix
Vector4 operator*(const Vector4& vec, const Matrix_my& mat) {
    if (mat.GetSize() != 4)
        throw invalid_argument("Vector4 * Matrix is only supported for 4x4 matrix.");
    return Vector4{
        vec.x * mat(0, 0) + vec.y * mat(1, 0) + vec.z * mat(2, 0) + vec.w * mat(3, 0),
        vec.x * mat(0, 1) + vec.y * mat(1, 1) + vec.z * mat(2, 1) + vec.w * mat(3, 1),
        vec.x * mat(0, 2) + vec.y * mat(1, 2) + vec.z * mat(2, 2) + vec.w * mat(3, 2),
        vec.x * mat(0, 3) + vec.y * mat(1, 3) + vec.z * mat(2, 3) + vec.w * mat(3, 3)
    };
}

inline ostream& operator<<(ostream& os, const Matrix_my& mat) {
    size_t n = mat.GetSize();

    os << "Matrix_" << n << "x" << n << ": [" << endl;
    for (size_t i = 0; i < n; i++) {
        os << "  [";
        for (size_t j = 0; j < n; j++) {
            double val = mat(i, j);
            // 处理接近零的值
            if (abs(val) < EPSILON) val = 0.0;

            os << val;
            if (j < n - 1) os << ", ";
        }
        os << "]";
        if (i < n - 1) os << ",";
        os << endl;
    }
    os << "]" << endl;

    return os;
}

//变换工厂
namespace Transform2D {
    inline Matrix_my Translate(double tx, double ty) {
        Matrix_my M(3);
        M.Initialize();
        M(0, 2) = tx;
        M(1, 2) = ty;
        return M;
    }
    inline Matrix_my Rotate(double rad) {
        Matrix_my M(3);
        M.Initialize();
        double c = cos(rad), s = sin(rad);
        M(0, 0) = c;
        M(0, 1) = -s;
        M(1, 0) = s;
        M(1, 1) = c;
        return M;
    }
    inline Matrix_my Scale(double sx, double sy) {
        Matrix_my M(3);
        M.Initialize();
        M(0, 0) = sx;
        M(1, 1) = sy;
        return M;
    }
}
namespace Transform3D {
    inline Matrix_my Translate(double tx, double ty, double tz) {
        Matrix_my M(4);
        M.Initialize();
        M(0, 3) = tx;
        M(1, 3) = ty;
        M(2, 3) = tz;
        return M;
    }
    inline Matrix_my Scale(double sx, double sy, double sz) {
        Matrix_my M(4);
        M.Initialize();
        M(0, 0) = sx;
        M(1, 1) = sy;
        M(2, 2) = sz;
        return M;
    }
    inline Matrix_my RotateX(double rad) {
        Matrix_my M(4);
        M.Initialize();
        double c = cos(rad), s = sin(rad);
        M(1, 1) = c;
        M(1, 2) = -s;
        M(2, 1) = s;
        M(2, 2) = c;
        return M;
    }
    inline Matrix_my RotateY(double rad) {
        Matrix_my M(4);
        M.Initialize();
        double c = cos(rad), s = sin(rad);
        M(0, 0) = c;
        M(0, 2) = s;
        M(2, 0) = -s;
        M(2, 2) = c;
        return M;
    }
    inline Matrix_my RotateZ(double rad) {
        Matrix_my M(4);
        M.Initialize();
        double c = cos(rad), s = sin(rad);
        M(0, 0) = c;
        M(0, 1) = -s;
        M(1, 0) = s;
        M(1, 1) = c;
        return M;
    }
    inline Matrix_my LookAt(const Vector3& eye, const Vector3& lookAt, const Vector3& up)
    {
        Vector3 zaxis = (eye - lookAt).normalized();        // camera forward
        Vector3 xaxis = up.cross(zaxis).normalized();       // camera right
        Vector3 yaxis = zaxis.cross(xaxis);                // camera up

        Matrix_my view(4);

        view(0,0) = xaxis.x;
        view(0,1) = xaxis.y;
        view(0,2) = xaxis.z;
        view(0,3) = -xaxis.dot(eye);

        view(1,0) = yaxis.x;
        view(1,1) = yaxis.y;
        view(1,2) = yaxis.z;
        view(1,3) = -yaxis.dot(eye);

        view(2,0) = zaxis.x;
        view(2,1) = zaxis.y;
        view(2, 2) = zaxis.z;
        view(2, 3) = -zaxis.dot(eye);

        view(3, 0) = 0;
        view(3, 1) = 0;
        view(3, 2) = 0;
        view(3, 3) = 1;

        return view;
    }
}

//统一调用
inline Vector2 ApplyTransformToVec2(const Matrix_my& M, const Vector2& v) {
    if (M.GetSize() == 3) {
        Vector3 hv = M * Vector3(v.x, v.y, 1);
        return { hv.x, hv.y };
    }
    throw invalid_argument("Unsupported matrix size for Vec2 transform");
}
inline Vector3 ApplyTransformToVec3(const Matrix_my& M, const Vector3& v) {
    if (M.GetSize() == 4) {
        Vector4 hv = M * Vector4(v.x, v.y, v.z, 1);
        return { hv.x, hv.y, hv.z };
    }
    throw invalid_argument("Unsupported matrix size for Vec3 transform");
}
inline Vector4 ApplyTransformToVec4(const Matrix_my& M, const Vector3& v) {
    if (M.GetSize() == 4) {
        Vector4 hv = M * Vector4(v.x, v.y, v.z, 1);
        return hv;
    }
    throw invalid_argument("Unsupported matrix size for Vec3 transform");
}
#endif