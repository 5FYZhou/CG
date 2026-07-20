#ifndef CURVE_H
#define CURVE_H

#include <vector>
#include "Vector.h"
#include <GL/freeglut.h>

#pragma region 贝塞尔
// 计算组合数 C(n,k)
unsigned long long comb(int n, int k) {
	if (k < 0 || k > n) return 0;
	if (k == 0 || k == n) return 1;
	unsigned long long res = 1;
	for (int i = 1; i <= k; ++i) {
		res *= (n - (k - i));
		res /= i;
	}
	return res;
}

// 任意 n 次贝塞尔曲线点
Vector3 bezierPoint(const vector<Vector3>& ctrlPoints, double t) {
	int n = ctrlPoints.size() - 1;
	Vector3 point(0, 0, 0);
	for (int i = 0; i <= n; ++i) {
		double b = comb(n, i) * pow(1 - t, n - i) * pow(t, i);
		point = point + ctrlPoints[i] * b;
	}
	return point;
}

// 绘制任意 n 次贝塞尔曲线
void drawBezier(const vector<Vector3>& ctrlPoints, int segments = 100) {
	if (ctrlPoints.size() < 2) return; // 至少 2 个控制点

	for (int i = 0; i <= segments; ++i) {
		double t = (double)i / segments;
		Vector3 pt = bezierPoint(ctrlPoints, t);
		glVertex2i((int)(pt.x + 0.5), (int)(pt.y + 0.5));
	}
}

// 计算二维贝塞尔曲面点
Vector3 bezierSurfacePoint(const vector<vector<Vector3>>& ctrlPoints, double u, double v) {
	int n = ctrlPoints.size() - 1;
	int m = ctrlPoints[0].size() - 1;
	Vector3 point(0, 0, 0);

	for (int i = 0; i <= n; ++i) {
		double bu = comb(n, i) * pow(1 - u, n - i) * pow(u, i);
		for (int j = 0; j <= m; ++j) {
			double bv = comb(m, j) * pow(1 - v, m - j) * pow(v, j);
			point += ctrlPoints[i][j] * (bu * bv);
		}
	}
	return point;
}

// 计算颜色插值
Color_my bezierSurfaceColor(const vector<vector<Color_my>>& ctrlColors, double u, double v) {
	int n = ctrlColors.size() - 1;
	int m = ctrlColors[0].size() - 1;
	Color_my col(0, 0, 0);

	for (int i = 0; i <= n; ++i) {
		double bu = comb(n, i) * pow(1 - u, n - i) * pow(u, i);
		for (int j = 0; j <= m; ++j) {
			double bv = comb(m, j) * pow(1 - v, m - j) * pow(v, j);
			col += ctrlColors[i][j] * (float)(bu * bv);
		}
	}
	return col;
}

// 绘制带颜色插值的贝塞尔曲面平面
void drawBezierSurface2DColored(const vector<vector<Vector3>>& ctrlPoints,
	const vector<vector<Color_my>>& ctrlColors,
	int uSeg = 20, int vSeg = 20) {

	// u方向网格线
	for (int i = 0; i <= uSeg; ++i) {
		double u = (double)i / uSeg;
		for (int j = 0; j < vSeg; ++j) {
			double v1 = (double)j / vSeg;
			double v2 = (double)(j + 1) / vSeg;

			Vector3 p1 = bezierSurfacePoint(ctrlPoints, u, v1);
			Vector3 p2 = bezierSurfacePoint(ctrlPoints, u, v2);
			Color_my c1 = bezierSurfaceColor(ctrlColors, u, v1);
			Color_my c2 = bezierSurfaceColor(ctrlColors, u, v2);

			glColor3f(c1.r, c1.g, c1.b);
			glVertex2i((int)(p1.x + 0.5), (int)(p1.y + 0.5));
			glColor3f(c2.r, c2.g, c2.b);
			glVertex2i((int)(p2.x + 0.5), (int)(p2.y + 0.5));
		}
	}

	// v方向网格线
	for (int j = 0; j <= vSeg; ++j) {
		double v = (double)j / vSeg;
		for (int i = 0; i < uSeg; ++i) {
			double u1 = (double)i / uSeg;
			double u2 = (double)(i + 1) / uSeg;

			Vector3 p1 = bezierSurfacePoint(ctrlPoints, u1, v);
			Vector3 p2 = bezierSurfacePoint(ctrlPoints, u2, v);
			Color_my c1 = bezierSurfaceColor(ctrlColors, u1, v);
			Color_my c2 = bezierSurfaceColor(ctrlColors, u2, v);

			glColor3f(c1.r, c1.g, c1.b);
			glVertex2i((int)(p1.x + 0.5), (int)(p1.y + 0.5));
			glColor3f(c2.r, c2.g, c2.b);
			glVertex2i((int)(p2.x + 0.5), (int)(p2.y + 0.5));
		}
	}
}
#pragma endregion

#pragma region B样条
Vector3 cubicBSplineSegment(const Vector3& P0, const Vector3& P1, const Vector3& P2, const Vector3& P3, double t) {
	double t2 = t * t, t3 = t * t * t;
	double b0 = (-t3 + 3 * t2 - 3 * t + 1) / 6.0;
	double b1 = (3 * t3 - 6 * t2 + 4) / 6.0;
	double b2 = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6.0;
	double b3 = t3 / 6.0;
	return P0 * b0 + P1 * b1 + P2 * b2 + P3 * b3;
}

// 绘制任意数量控制点的三次 B 样条
void drawBSpline(const vector<Vector3>& ctrlPoints, int segments = 20) {
	size_t n = ctrlPoints.size();
	if (n < 4) return; // 至少4个控制点

	for (size_t i = 0; i + 3 < n; ++i) { // 滑动窗口，每4个控制点计算一段曲线
		for (int j = 0; j <= segments; ++j) {
			double t = (double)j / segments;
			Vector3 pt = cubicBSplineSegment(ctrlPoints[i], ctrlPoints[i + 1], ctrlPoints[i + 2], ctrlPoints[i + 3], t);
			glVertex2i((int)pt.x, (int)pt.y);
		}
	}
}

// 三次均匀 B 样条基函数（对应矩阵法的系数）
inline double BSplineBasis(double t, int i) {
	switch (i) {
	case 0: return (-t * t * t + 3 * t * t - 3 * t + 1) / 6.0;
	case 1: return (3 * t * t * t - 6 * t * t + 4) / 6.0;
	case 2: return (-3 * t * t * t + 3 * t * t + 3 * t + 1) / 6.0;
	case 3: return (t * t * t) / 6.0;
	default: return 0;
	}
}

// 计算三次 B 样条曲面点
Vector3 BSplineSurfacePoint(const vector<vector<Vector3>>& ctrlPoints, double u, double v, size_t iu, size_t jv) {
	Vector3 p(0, 0, 0);

	for (int a = 0; a < 4; a++) {
		double bu = BSplineBasis(u, a);
		for (int b = 0; b < 4; b++) {
			double bv = BSplineBasis(v, b);
			p += ctrlPoints[iu + a][jv + b] * (bu * bv);
		}
	}
	return p;
}

// 计算 B 样条颜色插值
Color_my BSplineSurfaceColor(const vector<vector<Color_my>>& ctrlColors, double u, double v, size_t iu, size_t jv) {
	Color_my c(0, 0, 0);

	for (int a = 0; a < 4; a++) {
		double bu = BSplineBasis(u, a);
		for (int b = 0; b < 4; b++) {
			double bv = BSplineBasis(v, b);
			c += ctrlColors[iu + a][jv + b] * (float)(bu * bv);
		}
	}
	return c;
}

// 绘制三次 B 样条曲面（XY 平面线框 + 颜色插值）
void drawBSplineSurface2DColored(const vector<vector<Vector3>>& ctrlPoints,
	const vector<vector<Color_my>>& ctrlColors,
	int uSeg, int vSeg)
{
	size_t n = ctrlPoints.size();
	size_t m = ctrlPoints[0].size();
	if (n < 4 || m < 4) return;

	// 遍历每个 4x4 局部控制网格块
	for (size_t iu = 0; iu + 3 < n; iu++) {
		for (size_t jv = 0; jv + 3 < m; jv++) {

			// ---- u 方向曲线网格线 ----
			for (int i = 0; i <= uSeg; i++) {
				double u = (double)i / uSeg;
				for (int j = 0; j < vSeg; j++) {
					double v1 = (double)j / vSeg;
					double v2 = (double)(j + 1) / vSeg;

					Vector3 p1 = BSplineSurfacePoint(ctrlPoints, u, v1, iu, jv);
					Vector3 p2 = BSplineSurfacePoint(ctrlPoints, u, v2, iu, jv);

					Color_my c1 = BSplineSurfaceColor(ctrlColors, u, v1, iu, jv);
					Color_my c2 = BSplineSurfaceColor(ctrlColors, u, v2, iu, jv);

					glColor3f(c1.r, c1.g, c1.b);
					glVertex2i((int)(p1.x + 0.5), (int)(p1.y + 0.5));

					glColor3f(c2.r, c2.g, c2.b);
					glVertex2i((int)(p2.x + 0.5), (int)(p2.y + 0.5));
				}
			}

			// ---- v 方向曲线网格线 ----
			for (int j = 0; j <= vSeg; j++) {
				double v = (double)j / vSeg;
				for (int i = 0; i < uSeg; i++) {
					double u1 = (double)i / uSeg;
					double u2 = (double)(i + 1) / uSeg;

					Vector3 p1 = BSplineSurfacePoint(ctrlPoints, u1, v, iu, jv);
					Vector3 p2 = BSplineSurfacePoint(ctrlPoints, u2, v, iu, jv);

					Color_my c1 = BSplineSurfaceColor(ctrlColors, u1, v, iu, jv);
					Color_my c2 = BSplineSurfaceColor(ctrlColors, u2, v, iu, jv);

					glColor3f(c1.r, c1.g, c1.b);
					glVertex2i((int)(p1.x + 0.5), (int)(p1.y + 0.5));

					glColor3f(c2.r, c2.g, c2.b);
					glVertex2i((int)(p2.x + 0.5), (int)(p2.y + 0.5));
				}
			}
		}
	}
}
#pragma endregion

#endif