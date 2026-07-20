#ifndef CAMERA_H
#define CAMERA_H

#include "Vector.h"
#include "Matrix.h"
#include "Graphics.h"
#include "Projection.h"

/// <summary>
/// 定义相机基类及正交/透视相机子类，封装视点、注视点、投影参数与视锥体，提供视图矩阵、投影矩阵及视图-投影矩阵的构建与更新，并支持相机的整体变换。
/// </summary>

class Camera {
public:
    Vector3 eyePos;      // 视点位置
    Vector3 lookAt;   // 注视点
	Vector3 forward;  //视线方向
    Vector3 up;       // 上方向
	Vector3 worldUp = { 0,1,0 };

	double nearZ;     // 近平面
	double farZ;      // 远平面

	Matrix_my viewMatrix;
	Matrix_my projMatrix;
	Matrix_my viewProjMatrix;

	Polygon2D window; // 裁剪窗口

	Camera(const Vector3 e, const Vector3 la, const double n, const double f, const Polygon2D& w)
		:eyePos(e), lookAt(la), nearZ(n), farZ(f), window(w){
		forward = (eyePos - lookAt).normalized();
		Vector3 right = (worldUp.cross(forward)).normalized();
		up = forward.cross(right);
	}

	void InitMatrix() {
		viewMatrix = GetViewMatrix();
		projMatrix = GetProjectionMatrix();
		viewProjMatrix = projMatrix * viewMatrix;
	}
	Matrix_my GetViewMatrix() {
		return Transform3D::LookAt(eyePos, lookAt, up);
	}
	virtual Matrix_my GetProjectionMatrix() const = 0;

	void Transform(Matrix_my M) {
		// 位置：完整变换
		eyePos = ApplyTransformToVec3(M, eyePos);
		lookAt = ApplyTransformToVec3(M, lookAt);

		// 方向：只用旋转部分
		Matrix_my R = M;
		R(0,3) = R(1,3) = R(2,3) = 0;

		forward = ApplyTransformToVec3(R, forward).normalized();
		up = ApplyTransformToVec3(R, up).normalized();

		// 保证正交
		Vector3 right = (up.cross(forward)).normalized();
		up = forward.cross(right);

		// 更新矩阵
		InitMatrix(); 
	}
};

class OrhtoCamera : public Camera {
public:
	OrhtoCamera(const Vector3& e, const Vector3& la, const double n, const double f,
		const Polygon2D& w) :Camera(e, la, n, f, w) {
		InitMatrix();
	}

	virtual Matrix_my GetProjectionMatrix() const override {
		OrthographicProjection proj;
		return proj.ProjectMatrix();
	}
};

class PerspectiveCamera : public Camera {
public:
	double aspect;    // 宽高比
	double fovY;      // 视野角度（度数）

	PerspectiveCamera(const Vector3& e, const Vector3& la, const double n, const double f,
		const double a, const double fovYdeg, const Polygon2D& w) :Camera(e, la, n, f, w) {
		aspect = a;
		fovY = fovYdeg;

		InitMatrix();
	}
	virtual Matrix_my GetProjectionMatrix() const override {
		PerspectiveProjection proj(nearZ);
		return proj.ProjectMatrix();
	}
};

#endif