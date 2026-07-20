#ifndef SCENE_H
#define SCENE_H

#include "Vector.h"
#include "Graphics.h"
#include "Camera.h"
#include "Lighting.h"

/// <summary>
/// 定义场景管理类，集中维护正交与透视相机引用、2D/3D图形对象集合、点光源列表及环境光，提供添加 3D 网格的方法，作为渲染系统的顶层数据容器。
/// </summary>

class Scene {
public:
	Camera& cameraO;
	Camera& cameraP;
	vector<Shape2D*> shape2Ds;
	vector<Mesh3D*> Mesh3Ds;
	vector<PointLight*> lights; 
	Color_my ambientLight = { 0.1f,0.1f,0.1f };

	Scene(Camera& cam, Camera& ca, vector<PointLight*> l)
		: cameraO(cam), cameraP(ca), lights(l){
		shape2Ds.clear();
		Mesh3Ds.clear();
	}
	
	void AddShape3D(Mesh3D* shape) {
		Mesh3Ds.push_back(shape);
	}
};

#endif