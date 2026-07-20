#ifndef OBJLOADER_H
#define OBJLOADER_H

#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <locale>
#include <codecvt>
#include "Vector.h"
#include "Texture.h"
#include "Graphics.h"
using namespace std;

/// <summary>
/// 实现从 Wavefront OBJ/MTL 文件加载三维网格数据与材质，解析顶点、法线、纹理坐标和面信息，构建带材质的 Mesh3D 对象供渲染使用。
/// </summary>

class ObjLoader {
public:
	map<string, Material> LoadMTL(const string& filePath, const string& mtlFile) {
		map<string, Material> mats;
		ifstream f(mtlFile);
		if (!f.is_open()) { cout << "Failed to open MTL file: " << mtlFile << endl; return mats; }

		string line, tag, currentName;
		while (getline(f, line)) {
			stringstream ss(line);
			ss >> tag;
			if (tag == "newmtl") { ss >> currentName; mats.emplace(currentName, Material()); }
			else if (tag == "Ka") { float r, g, b; ss >> r >> g >> b; mats.at(currentName).ka = Color_my(r, g, b); }
			else if (tag == "Kd") { float r, g, b; ss >> r >> g >> b; mats.at(currentName).kd = Color_my(r, g, b); }
			else if (tag == "Ks") { float r, g, b; ss >> r >> g >> b; 
				mats.at(currentName).ks = Color_my(r, g, b);
				float kr = 0.2126f * r + 0.7152f * g + 0.0722f * b;
				mats.at(currentName).reflectivity = kr; }
			else if (tag == "Ns") { float ns; ss >> ns; mats.at(currentName).shininess = ns; }
			else if (tag == "Ni") { float ni; ss >> ni; mats.at(currentName).ior = ni; }
			else if (tag == "d") { float d; ss >> d; mats.at(currentName).t = d; }
			else if (tag == "Ke") { float r, g, b; ss >> r >> g >> b; mats.at(currentName).ke = Color_my(r, g, b);}
			else if (tag == "map_Kd")
			{
				string token, last;
				while (ss >> token) {
					last = token;
				}

				// last 才是真正的贴图路径
				string texPath = last;

				size_t pos = texPath.find_last_of("/\\");
				string texName = (pos == string::npos) ? texPath : texPath.substr(pos + 1);

				string finalTexPath = filePath + "\\" + texName;
				wstring wtex(finalTexPath.begin(), finalTexPath.end());

				auto& m = mats.at(currentName);
				auto tex = make_shared<Texture>(wtex);

				if (tex->loaded) {
					m.diffuseMap = tex;   //只在成功时覆盖
				}
			}
		}
		f.close();
		return mats;
	}

	Mesh3D LoadMesh3D(const string& filepath, const string& filename)
	{
		string objPath = filepath + "\\" + filename + ".obj";
		string mtlPath = filepath + "\\" + filename + ".mtl";

		// ---------- 读取 MTL ----------
		map<string, Material> mtlLib = LoadMTL(filepath, mtlPath);
		
		// ---------- 读取 OBJ ----------
		ifstream f(objPath);
		if (!f.is_open()) {
			cout << "Failed to open OBJ file: " << objPath << endl;
			return Mesh3D();
		}

		vector<Vector3> vList;
		vector<Vector3> vnList;
		vector<Vector2> vtList;

		vector<Vector3> finalVerts;
		vector<Vector3> finalNormals;
		vector<Vector2> finalUVs;
		vector<Face3D> faces;

		map<tuple<int, int, int>, int> vertMap;

		string line, tag;
		string currentUseMtl = "";

		while (getline(f, line)) {
			stringstream ss(line);
			ss >> tag;

			if (tag == "v") {
				double x, y, z;
				ss >> x >> y >> z;
				vList.push_back({ x, y, z });
			}
			else if (tag == "vn") {
				double x, y, z;
				ss >> x >> y >> z;
				vnList.push_back({ x, y, z });
			}
			else if (tag == "vt") {
				double u, v;
				ss >> u >> v;
				vtList.push_back({ u, v });
			}
			else if (tag == "usemtl") {
				ss >> currentUseMtl;
			}
			else if (tag == "f") {

				Face3D face;

				// 绑定材质到 face
				if (mtlLib.count(currentUseMtl)) {
					face.mat = mtlLib.at(currentUseMtl);
				}
				else
					face.mat = Material();

				string token;
				while (ss >> token) {
					int vIdx = -1, vtIdx = -1, vnIdx = -1;

					size_t p1 = token.find('/');
					if (p1 == string::npos) {
						vIdx = stoi(token) - 1;
					}
					else {
						vIdx = stoi(token.substr(0, p1)) - 1;
						size_t p2 = token.find('/', p1 + 1);

						if (p2 > p1 + 1)
							vtIdx = stoi(token.substr(p1 + 1, p2 - p1 - 1)) - 1;
						if (p2 != string::npos && p2 + 1 < token.size())
							vnIdx = stoi(token.substr(p2 + 1)) - 1;
					}

					tuple<int, int, int> key = { vIdx, vtIdx, vnIdx };
					int newIndex;

					if (vertMap.count(key)) {
						newIndex = vertMap[key];
					}
					else {
						finalVerts.push_back(vList[vIdx]);
						finalNormals.push_back(vnIdx >= 0 ? vnList[vnIdx] : Vector3{ 0,0,0 });
						finalUVs.push_back(vtIdx >= 0 ? vtList[vtIdx] : Vector2{ 0,0 });

						newIndex = (int)finalVerts.size() - 1;
						vertMap[key] = newIndex;
					}

					face.indices.push_back(newIndex);
				}

				faces.push_back(face);
			}
		}

		f.close();

		cout << "OBJ Loaded: "
			<< finalVerts.size() << " verts, "
			<< faces.size() << " faces, "
			<< finalNormals.size() << " normals\n";

		Mesh3D mesh(finalVerts, faces, finalNormals, finalUVs);

		return mesh;
	}
};
#endif