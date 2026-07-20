#ifndef FILLER_H
#define FILLER_H

#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include "Graphics.h"
#include "Vector.h"
#include "Texture.h"
#include "Constants.h"
#include "FrameBuffer.h"
using namespace std;

/// <summary>
/// 实现基于扫描线的多边形填充框架，支持带透视校正的深度测试与纹理映射，
/// 通过活性边表（AET）与边表（ET）对Mesh3D的Face3D进行光栅化着色，并处理半透明面的深度排序。
/// </summary>

struct Edge {
	double curX;	//当前扫描线与当前边 交点的x
	double rK;	//斜率的倒数
	int maxY;	//边最大的y
	// 为透视校正插值存储的量（在交点处的值）
	double curZS;   // z * ratio 在当前交点处的值（zs = z * ratio）
	double curR;    // ratio 在当前交点处的值
	// 每向下移动 1 scanline 时的增量
	double dZS_dy;
	double dR_dy;

	Edge(double cx, double k, int my, double zs = 1, double cr = 0, double dz = 0, double dr = 0)
		:curX(cx), rK(k), maxY(my), curZS(zs), curR(cr), dZS_dy(dz), dR_dy(dr) {
	}
	bool operator==(const Edge& e) {
		return abs(this->curX - e.curX) < EPSILON && abs(this->rK - e.rK) < EPSILON && this->maxY == e.maxY;
	}
};

bool cmp(const Edge& a, const Edge& b) {
	if (fabs(a.curX - b.curX) > EPSILON) return a.curX < b.curX;
	if (fabs(a.rK - b.rK) > EPSILON) return a.rK < b.rK;
	return a.maxY < b.maxY;
}

struct EdgeWithTexture {
	double curX;    // 当前扫描线与边交点 X
	double rK;      // 斜率倒数
	int maxY;       // 边最大 Y

	// 透视修正插值用量
	double curZS;       // z * ratio
	double curR;        // ratio = 1/w
	Vector2 curUV;      // uv * ratio

	// 每向下移动 1 scanline 的增量
	double dZS_dy;
	double dR_dy;
	Vector2 dUV_dy;

	EdgeWithTexture(double cx, double k, int my,
		double zs = 1, double cr = 0, Vector2 uv = { 0,0 },
		double dz = 0, double dr = 0, Vector2 duv = { 0,0 })
		: curX(cx), rK(k), maxY(my), curZS(zs), curR(cr), curUV(uv),
		dZS_dy(dz), dR_dy(dr), dUV_dy(duv) {
	}

	bool operator==(const EdgeWithTexture& e) {
		return fabs(curX - e.curX) < EPSILON &&
			fabs(rK - e.rK) < EPSILON &&
			maxY == e.maxY;
	}
};

// 排序
inline bool cmpTexEdge(const EdgeWithTexture& a, const EdgeWithTexture& b) {
	if (fabs(a.curX - b.curX) > EPSILON) return a.curX < b.curX;
	if (fabs(a.rK - b.rK) > EPSILON) return a.rK < b.rK;
	return a.maxY < b.maxY;
}

class IFill2D {
protected:
	FrameBuffer* buffer = nullptr;
public:
	void SetFrameBuffer(FrameBuffer* b) {
		buffer = b;
	}
	virtual void FillMesh(const Mesh3D& mesh) = 0;
	virtual void FillMeshWithTexture(const Mesh3D& mesh) = 0;
};

class Scanline :public IFill2D {
private:
	map<int, vector<Edge>> ET;
	vector<Edge> AET;
	int endY = 0;
	map<int, vector<EdgeWithTexture>> ETTex;
	vector<EdgeWithTexture> AETTex;

	void GenerateET(const Mesh3D& mesh, const Face3D& f) {
		ET.clear();
		endY = INT_MIN;
		const auto& pts = f.indices;
		int n = (int)pts.size();

		if (n < 3) return;

		for (int i = 0; i < n; ++i) {
			const Vector3& v1 = mesh.clippedPoints[pts[i]].projectedPoint;
			const Vector3& v2 = mesh.clippedPoints[pts[(i + 1) % n]].projectedPoint;

			// 忽略水平边
			if (fabs(v1.y - v2.y) < EPSILON) {
				//cout <<" | "<< v1.x << " | " << v1.y << " | " << v2.x << " | " << v2.y << " | " << endl;
				//cout <<i<< " fabs(v1.y - v2.y) < EPSILON" << endl;
				continue;
			}

			// 我们把 edge 从 yLower -> yUpper (向下扫描时的方向)
			const Vector3* top = &v1, * bottom = &v2;
			if (v1.y > v2.y) { top = &v2; bottom = &v1; } // top: smaller y, bottom: larger y

			double yTop = top->y;
			double yBottom = bottom->y;

			// scanline 起始
			int yStart = (int)(yTop + 0.5);
			int yMax = (int)(yBottom + 0.5);

			// 当 curY == yMax 时认为边已失效（符合你之前的逻辑：if (e.maxY > curY) 保留）
			if (yStart >= yMax) {
				//cout << "yStart >= yMax" << endl;
				continue; // 不跨越任何扫描线
			}

			// 边上的 x 在 yStart 的值, 用线性插值得到
			double dyFull = (bottom->y - top->y); // >0
			double dxFull = (bottom->x - top->x);
			double drFull = (bottom->ratio - top->ratio);
			double dzsFull = ((bottom->z * bottom->ratio) - (top->z * top->ratio));

			// 计算从顶点 yTop 到 yStart 的 t（以 y 为参数）
			double tScan = 0.0;
			if (fabs(dyFull) > EPSILON) tScan = ((double)yStart - yTop) / dyFull;
			/*
			double curX = top->x + tScan * dxFull;
			double curR = top->ratio + tScan * drFull;
			double curZS = (top->z * top->ratio) + tScan * dzsFull;
			*/
			double curX = top->x;
			double curR = top->ratio;
			double curZS = (top->z * top->ratio);
			// 每向下移动 1 scanline，curX 增量:
			double rK = dxFull / dyFull;
			double dRdy = drFull / dyFull;
			double dZSdy = dzsFull / dyFull;

			Edge e(curX, rK, yMax, curZS, curR, dZSdy, dRdy);

			ET[yStart].push_back(e);
			if (yMax > endY) endY = yMax;
		}
	}
	void GenerateETWithTexture(const Mesh3D& mesh, const Face3D& f) {
		ETTex.clear();
		endY = INT_MIN;
		const auto& pts = f.indices;
		int n = (int)pts.size();
		if (n < 3) return;

		for (int i = 0; i < n; ++i) {
			const Vertex& v1 = mesh.clippedPoints[pts[i]];
			const Vertex& v2 = mesh.clippedPoints[pts[(i + 1) % n]];

			if (fabs(v1.projectedPoint.y - v2.projectedPoint.y) < EPSILON) continue;

			const Vertex* top = &v1;
			const Vertex* bottom = &v2;
			if (v1.projectedPoint.y > v2.projectedPoint.y) swap(top, bottom);

			double yTop = top->projectedPoint.y;
			double yBottom = bottom->projectedPoint.y;
			int yStart = (int)(yTop + 0.5);
			int yMax = (int)(yBottom + 0.5);
			if (yStart >= yMax) continue;

			double dyFull = bottom->projectedPoint.y - top->projectedPoint.y;
			double dxFull = bottom->projectedPoint.x - top->projectedPoint.x;
			double dzsFull = (bottom->point.z * bottom->projectedPoint.z - top->point.z * top->projectedPoint.z);
			double drFull = bottom->projectedPoint.z - top->projectedPoint.z; // 或 ratio

			double rK = dxFull / dyFull;
			double dZS_dy = dzsFull / dyFull;
			double dR_dy = drFull / dyFull;

			Vector2 uvTopPre = top->uvPos * top->projectedPoint.z;
			Vector2 uvBottomPre = bottom->uvPos * bottom->projectedPoint.z;
			Vector2 duvFull = uvBottomPre - uvTopPre;
			Vector2 dUV_dy = duvFull / dyFull;

			EdgeWithTexture e(top->projectedPoint.x, rK, yMax,
				top->point.z * top->projectedPoint.z, top->projectedPoint.z, uvTopPre,
				dZS_dy, dR_dy, dUV_dy);

			ETTex[yStart].push_back(e);
			if (yMax > endY) endY = yMax;
		}
	}

	void DrawLine(Edge& L, Edge& R, const int curY, const Material& mat) {
		double xL = L.curX;
		double xR = R.curX;

		// 对应的透视校正值（在交点处）
		double zsL = L.curZS;
		double zsR = R.curZS;
		double rL = L.curR;
		double rR = R.curR;

		if (xR <= xL) return; // 防御性处理

		// 像素区间
		int xPixelStart = (int)(xL + 0.5);
		int xPixelEnd = (int)(xR + 0.5);

		if (xPixelEnd < xPixelStart) return;

		double dx = xR - xL;
		// 在 x 方向上线性插值 zs 和 r（因为我们在交点处已经有透视预乘值）
		double dZS_dx = (zsR - zsL) / dx;
		double dR_dx = (rR - rL) / dx;

		// 计算第一个像素中心对应的 zs 和 r
		double curZS = zsL + ((double)xPixelStart - xL) * dZS_dx;
		double curR = rL + ((double)xPixelStart - xL) * dR_dx;

		for (int x = xPixelStart; x <= xPixelEnd; ++x) {
			// 恢复真实 z（用于深度测试）
			double realZ = (fabs(curR) > EPSILON) ? (curZS / curR) : 1e9;

			buffer->SetPixel(x, curY, realZ, mat.kd, mat.t, mat.ke);

			// advance
			curZS += dZS_dx;
			curR += dR_dx;
		}
	}
	void DrawLineWithTexture(EdgeWithTexture& L, EdgeWithTexture& R, int curY, const Material& mat) {
		double xL = L.curX, xR = R.curX;
		double zsL = L.curZS, zsR = R.curZS;
		double rL = L.curR, rR = R.curR;
		Vector2 uvL = L.curUV, uvR = R.curUV;

		if (xR <= xL) return;

		int xPixelStart = (int)(xL + 0.5);
		int xPixelEnd = (int)(xR + 0.5);
		if (xPixelEnd < xPixelStart) return;

		double dx = xR - xL;

		// x方向插值增量
		double dZS_dx = (zsR - zsL) / dx;
		double dR_dx = (rR - rL) / dx;
		Vector2 dUV_dx = (uvR - uvL) / dx;

		double curZS = zsL + ((double)xPixelStart - xL) * dZS_dx;
		double curR = rL + ((double)xPixelStart - xL) * dR_dx;
		Vector2 curUV = uvL + dUV_dx * ((double)xPixelStart - xL);

		for (int x = xPixelStart; x <= xPixelEnd; ++x) {
			double realZ = (fabs(curR) > EPSILON) ? (curZS / curR) : 1e9;

			// 恢复透视校正后的 uv
			Vector2 uv = (fabs(curR) > EPSILON) ? (curUV / curR) : Vector2{ 0,0 };

			Color_my kd = mat.diffuseMap ?
				(mat.diffuseMap->loaded ? mat.diffuseMap->Sample(uv.x, uv.y) : mat.kd)
				: mat.kd;

			buffer->SetPixel(x, curY, realZ, kd, mat.t, mat.ke);

			curZS += dZS_dx;
			curR += dR_dx;
			curUV += dUV_dx;
		}
	}
public:
	void FillMesh(const Mesh3D& mesh) override {
		if (buffer == nullptr) {
			throw invalid_argument("Filler buffer hasn't set");
			return;
		}
		vector<Face3D> tFace;
		for (auto& face : mesh.clippedFaces) {
			if (face.mat.t < 1) {
				tFace.push_back(face);
				continue;
			}
			GenerateET(mesh, face);
			if (ET.size() < 1) {
				continue;
			}

			AET.clear();

			int curY = ET.begin()->first;

			for (; curY < endY; ++curY) {
				//加入新边
				if (ET.count(curY)) {
					for (auto& e : ET[curY])
						AET.push_back(e);
				}
				// 删除失效边
				vector<Edge> newAET;
				for (auto& e : AET)
					if (e.maxY > curY)
						newAET.push_back(e);
				AET.swap(newAET);

				if (AET.empty()) continue;
				sort(AET.begin(), AET.end(), cmp);

				for (unsigned int i = 0; i < AET.size() - 1; i += 2) {
					DrawLine(AET[i], AET[i + 1], curY, face.mat);
				}

				for (auto& e : AET) {
					e.curX += e.rK;
					e.curZS += e.dZS_dy;
					e.curR += e.dR_dy;
				}
			}
		}
		// 计算半透明面
		if (tFace.size() <= 0)return;
		sort(tFace.begin(), tFace.end(), [](Face3D a, Face3D b) {
			return a.center.z > b.center.z; // 从远到近
			});
		for (auto& face : tFace) {
			GenerateET(mesh, face);
			if (ET.size() < 1) {
				continue;
			}

			AET.clear();

			int curY = ET.begin()->first;

			for (; curY < endY; ++curY) {
				//加入新边
				if (ET.count(curY)) {
					for (auto& e : ET[curY])
						AET.push_back(e);
				}
				// 删除失效边
				vector<Edge> newAET;
				for (auto& e : AET)
					if (e.maxY > curY)
						newAET.push_back(e);
				AET.swap(newAET);

				if (AET.empty()) continue;
				sort(AET.begin(), AET.end(), cmp);

				for (unsigned int i = 0; i < AET.size() - 1; i += 2) {
					DrawLine(AET[i], AET[i + 1], curY, face.mat);
				}

				for (auto& e : AET) {
					e.curX += e.rK;
					e.curZS += e.dZS_dy;
					e.curR += e.dR_dy;
				}
			}
		}
	}

	void FillMeshWithTexture(const Mesh3D& mesh) override {
		if (buffer == nullptr) {
			throw invalid_argument("Filler buffer hasn't set");
			return;
		}
		vector<Face3D> tFace;
		for (auto& face : mesh.clippedFaces) {
			if (face.mat.t < 1) {
				tFace.push_back(face);
				continue;
			}
			GenerateETWithTexture(mesh, face);
			if (ETTex.size() < 1)continue;
			AETTex.clear();
			int curY = ETTex.begin()->first;

			for (; curY < endY; ++curY) {
				if (ETTex.count(curY)) for (auto& e : ETTex[curY]) AETTex.push_back(e);
				vector<EdgeWithTexture> newAET;
				for (auto& e : AETTex) if (e.maxY > curY) newAET.push_back(e);
				AETTex.swap(newAET);

				if (AETTex.empty()) continue;
				sort(AETTex.begin(), AETTex.end(), cmpTexEdge);

				for (size_t i = 0; i < AETTex.size() - 1; i += 2) {
					DrawLineWithTexture(AETTex[i], AETTex[i + 1], curY, face.mat);
				}

				for (auto& e : AETTex) {
					e.curX += e.rK;
					e.curZS += e.dZS_dy;
					e.curR += e.dR_dy;
					e.curUV += e.dUV_dy;
				}
			}
		}
		// 计算半透明面
		if (tFace.size() <= 0)return;
		sort(tFace.begin(), tFace.end(), [](Face3D a, Face3D b) {
			return a.center.z > b.center.z; // 从远到近
			});
		for (auto& face : tFace) {
			GenerateETWithTexture(mesh, face);
			if (ETTex.size() < 1)continue;
			AETTex.clear();
			int curY = ETTex.begin()->first;

			for (; curY < endY; ++curY) {
				if (ETTex.count(curY)) for (auto& e : ETTex[curY]) AETTex.push_back(e);
				vector<EdgeWithTexture> newAET;
				for (auto& e : AETTex) if (e.maxY > curY) newAET.push_back(e);
				AETTex.swap(newAET);

				if (AETTex.empty()) continue;
				sort(AETTex.begin(), AETTex.end(), cmpTexEdge);

				for (size_t i = 0; i < AETTex.size() - 1; i += 2) {
					DrawLineWithTexture(AETTex[i], AETTex[i + 1], curY, face.mat);
				}

				for (auto& e : AETTex) {
					e.curX += e.rK;
					e.curZS += e.dZS_dy;
					e.curR += e.dR_dy;
					e.curUV += e.dUV_dy;
				}
			}
		}
	}
};

inline void FillMesh(const Mesh3D& mesh, IFill2D& filler) {
	filler.FillMesh(mesh);
}
inline void FillMeshWithTexture(const Mesh3D& mesh, IFill2D& filler) {
	filler.FillMeshWithTexture(mesh);
}

#endif