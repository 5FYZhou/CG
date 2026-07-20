#ifndef PROJECTION_H
#define PROJECTION_H

#include <vector>
#include "Constants.h"
#include "Vector.h"
#include "Graphics.h"

/// <summary>
/// 定义3D到2D投影接口及正交/透视投影实现，提供线、面、网格的投影转换方法
/// </summary>

//投影策略接口
class IProjection3DTo2D {
public:
    virtual ~IProjection3DTo2D() {}
	virtual Matrix_my ProjectMatrix() const = 0;
};

//正交投影
class OrthographicProjection : public IProjection3DTo2D {
public:
    Matrix_my ProjectMatrix() const override  {
        Matrix_my m(4);
        m.Initialize();
        return m;
    }

    Matrix_my ProjectMatrix(double left, double right, double down, double up, double nearZ, double farZ) const {
        Matrix_my m(4);
        m.Initialize();
        m(0,0) = 2.0 / (right - left);
        m(1,1) = 2.0 / (up - down);
        m(2,2) = -2.0 / (farZ - nearZ);

        m(0,3) = -(right + left) / (right - left);
        m(1,3) = -(up + down) / (up - down);
        m(2,3) = -(farZ + nearZ) / (farZ - nearZ);
        return m;
    }
};

//透视投影
class PerspectiveProjection : public IProjection3DTo2D {
public:
    double d;
    PerspectiveProjection(double dd = 0) : d(dd) {}

    Matrix_my ProjectMatrix() const override {
        Matrix_my m(4);
        m.Initialize();
        m(3, 2) = -1.0 / d;
        return m;
    }
};

//图形投影
inline void ProjectMeshFaces(Mesh3D& mesh, const Matrix_my& M) {
    mesh.projectedFaces.clear();
    mesh.projectedPoints.clear();

    const int N = mesh.points.size();
    vector<bool> used(N, false);

    // Step 1: 选取可见面
    for (auto& f : mesh.faces) {
        if (f.canBeSee) {
            mesh.projectedFaces.push_back(f);
            for (int idx : f.indices) {
                used[idx] = true;
            }
        }
    }

    // Step 2: 构建 oldIndex -> newIndex 映射
    vector<int> mapOldToNew(N, -1);
    mesh.projectedPoints.reserve(N);

    int newIndex = 0;
    for (int i = 0; i < N; i++) {
        if (used[i]) {
            mapOldToNew[i] = newIndex++;
            Vertex newV = mesh.points[i];
            Vector4 v = ApplyTransformToVec4(M, mesh.points[i].point);
            if (fabs(v.w) < EPSILON)v.w = EPSILON * (v.w > 0 ? 1 : -1);
            v /= v.w;
            newV.projectedPoint = { v.x,v.y,v.z,v.w };
            mesh.projectedPoints.push_back(newV);
        }
    }

    // Step 3: 更新 projectedFaces 的索引
    for (auto& f : mesh.projectedFaces) {
        for (auto& idx : f.indices) {
            idx = mapOldToNew[idx];
        }
    }
}
#endif