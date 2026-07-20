#define GLUT_DISABLE_ATEXIT_HACK
#include <iostream>
#include <vector>
#include <GL/freeglut.h>
#include "Vector.h"
#include "Texture.h"
#include "Matrix.h"
#include "Graphics.h"
#include "Projection.h"
#include "Clipper.h"
#include "Filler.h"
#include "FrameBuffer.h"
#include "Scene.h"
#include "Renderer.h"
#include "ObjLoader.h"
#include "Curve.h"
#include "Lighting.h"
#include "ShadowMap.h"
using namespace std;

#pragma region 场景定义
//绕某条线旋转
Vector3 rotatePoint = { 0,0,0 };
Vector3 rotateLineDirection = { 0,1,0 };

//图形
Mesh3D shape;
Mesh3D shape1;
//加载器
ObjLoader objLoader;
//填充
Scanline filler;
//裁剪
SutherlandHodgman_PolygonClip2D clipper;
//裁剪窗口（长方形）
vector<Vector2> cp = { { -400,-300},{400,-300},{400,300},{-400,300 } };
Polygon2D clipRect(cp);
//帧缓存 范围为裁剪窗口
FrameBuffer buffer(clipRect);
//光源
PointLight pointLight({ 100,200,-300 }, { -100,-200,300 }, { 1.0f,1.0f,1.0f }, true);
PointLight pointLight1({ -50,50,400 }, { 50,-50,-400 }, { 0.5f,0.25f,0.0f }, true);
PointLight pointLight2({ 0,0,500 }, { 0,0,-500 }, { 0.3f,0.3f,0.3f }, false);
vector<PointLight*> lights = { &pointLight, &pointLight1, &pointLight2 };
//正交摄像机
OrhtoCamera cameraO({ 0,200,1000 }, { 0,0,0 }, 1000, 2000, clipRect);
//透视摄像机
PerspectiveCamera cameraP({ 0,50,400 }, { 0,0,0 }, 1500, 2000, 4 / 3, 0, clipRect);
//PerspectiveCamera cameraP({ 0,200,1000 }, { 0,0,0 }, 1000, 2000, 4 / 3, 0, clipRect);
//场景
Scene scene(cameraO, cameraP, lights);
//创建管线
Renderer renderer(scene, buffer, &clipper, filler, &cameraO);
#pragma endregion

int MODE = 1;
//1 填充无纹理
//2 填充有纹理
//3 光照无纹理
//4 光照有纹理
//5 离线

void Init() {
	//读入房间
	shape = objLoader.LoadMesh3D("D:\\room","room2");
	shape.Transform(Transform3D::RotateY(-PI/12));
	//添加进场景
	scene.AddShape3D(&shape);
}

//绘制坐标系
void draw_coordinate()
{
	glBegin(GL_LINES);
	glColor3f(0.0f, 1.0f, 0.0f);
	glVertex2f(-500, 0.0);
	glVertex2f(500, 0.0);

	glColor3f(1.0f, 0.0f, 0.0f);
	glVertex2f(0.0, -500);
	glVertex2f(0.0, 500);
	glEnd();
}

//绘制内容
void display(void)
{
	glClearColor(1.f, 1.f, 1.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	draw_coordinate(); //绘制坐标系
	glColor3f(0, 0, 0);
	glBegin(GL_POINTS);
	{
		switch (MODE)
		{
		case 1:
			renderer.Render1();
			break;
		case 2:
			renderer.Render2();
			break;
		case 3:
			renderer.Render3();
			break;
		case 4:
			renderer.Render4();
			break;
		case 5:
			renderer.Render5();
			break;
		default:
			break;
		}

		//绘制裁剪框
		glColor3f(0, 0, 0);
		clipRect.DrawWireframe();
	}
	glEnd();
	glutSwapBuffers();
}
// 摄像机朝原点移动的函数
void MoveCameraTowardsOrigin(double step)
{
	Camera* camera = renderer.curCamera;

	// 获取摄像机位置
	Vector3 cameraPos = camera->eyePos;

	// 计算从摄像机指向原点的方向向量
	Vector3 directionToOrigin = Vector3(0, 0, 0) - cameraPos;
	double distanceToOrigin = directionToOrigin.length();

	// 如果已经在原点附近，就不移动了
	if (distanceToOrigin < step) {
		camera->eyePos=(Vector3(0, 0, 0));
		return;
	}

	// 归一化方向向量
	Vector3 moveDir = directionToOrigin.normalized();

	// 计算移动向量
	Vector3 moveVector = moveDir * step;

	// 移动摄像机
	camera->Transform(Transform3D::Translate(moveVector.x, moveVector.y, moveVector.z));
}
//键盘交互事件
void keyboard(unsigned char key, int x, int y)
{
	double moveStep = 10;
	switch (key)
	{
	case 'w': case 'W':
		renderer.curCamera->Transform(Transform3D::Translate(0, moveStep, 0));
		glutPostRedisplay();
		break;
	case 's': case 'S':
		renderer.curCamera->Transform(Transform3D::Translate(0, -moveStep, 0));
		glutPostRedisplay();
		break;
	case 'a': case 'A':
		renderer.curCamera->Transform(Transform3D::Translate(-moveStep, 0, 0));
		glutPostRedisplay();
		break;
	case 'd': case 'D':
		renderer.curCamera->Transform(Transform3D::Translate(moveStep, 0, 0));
		glutPostRedisplay();
		break;
		/*
		case 'q': case 'Q':
			clipRect.Transform(Transform2D::Rotate(PI / 8));
			glutPostRedisplay();
			break;
		case 'e': case 'E':
			clipRect.Transform(Transform2D::Rotate(- PI / 8));
			glutPostRedisplay();
			break;
		*/
	case 'z': case 'Z':
		renderer.curCamera->Transform(Transform3D::Translate(0, 0, moveStep));
		glutPostRedisplay();
		break;
	case 'x': case 'X':
		renderer.curCamera->Transform(Transform3D::Translate(0, 0, -moveStep));
		glutPostRedisplay();
		break;
	case 'o':case 'O':
		renderer.curCamera = &cameraO;
		glutPostRedisplay();
		break;
	case 'p':case 'P':
		renderer.curCamera = &cameraP;
		glutPostRedisplay();
		break;
	case 'i': case 'I':
		shape.Transform(Transform3D::Translate(0, moveStep, 0));
		glutPostRedisplay();
		break;
	case 'k': case 'K':
		shape.Transform(Transform3D::Translate(0, -moveStep, 0));
		glutPostRedisplay();
		break;
	case 'j': case 'J':
		shape.Transform(Transform3D::Translate(-moveStep, 0, 0));
		glutPostRedisplay();
		break;
	case 'l': case 'L':
		shape.Transform(Transform3D::Translate(moveStep, 0, 0));
		glutPostRedisplay();
		break;
	case 'n': case 'N':
		shape.Transform(Transform3D::Translate(0, 0, moveStep));
		glutPostRedisplay();
		break;
	case 'm': case 'M':
		shape.Transform(Transform3D::Translate(0, 0, -moveStep));
		glutPostRedisplay();
		break;
	case 't': case 'T':
		//MoveCameraTowardsOrigin(500);
		renderer.curCamera->Transform(Transform3D::Translate(0, -150, -1000));
		glutPostRedisplay();
		break;
	case 'g': case 'G':
		//shape.Transform(Transform3D::Translate(0, -moveStep, 0));
		glutPostRedisplay();
		break;
	case 'f':case 'F':
		//shape.Transform(Transform3D::Translate(-moveStep, 0, 0));
		glutPostRedisplay();
		break;
	case 'h':case 'H':
		//shape.Transform(Transform3D::Translate(moveStep, 0, 0));
		glutPostRedisplay();
		break;
	case '1':
		MODE = 1;
		glutPostRedisplay();
		break;
	case '2':
		MODE = 2;
		glutPostRedisplay();
		break;
	case '3':
		MODE = 3;
		glutPostRedisplay();
		break;
	case '4':
		MODE = 4;
		glutPostRedisplay();
		break;
	case '5':
		MODE = 5;
		glutPostRedisplay();
		break;
	case 27:
		exit(0);
		break;
	}
}

//鼠标交互事件
void mouse(int button, int state, int x, int y)
{
	double angle = PI / 8;
	Vector3 u = rotateLineDirection.normalized();
	double phi = atan2(u.y, u.x);
	double r = sqrt(u.x * u.x + u.y * u.y);
	double theta = atan2(r, u.z);
	switch (button) {
	case GLUT_LEFT_BUTTON:
		if (state == GLUT_DOWN)
		{
			shape.Transform(Transform3D::RotateY(-angle));
			//glutPostRedisplay();
		}
		break;
	case GLUT_RIGHT_BUTTON:
		if (state == GLUT_DOWN)
		{
			shape.Transform(Transform3D::RotateY(angle));
			//glutPostRedisplay();
		}
		break;
	default:
		break;
	}
}

//投影方式、modelview方式等设置
void reshape(GLsizei w, GLsizei h)
{
	glViewport(0, 0, w, h); //它负责把视景体截取的场景按照怎样的高和宽显示到屏幕上。
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(-0.5 * w, 0.5 * w, -0.5 * h, 0.5 * h);
	glMatrixMode(GL_MODELVIEW);

}

//主调函数
int main(int argc, char** argv)
{
	// 初始化 GDI+
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR token;
	GdiplusStartup(&token, &gdiplusStartupInput, nullptr);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
	glutInitWindowSize(900, 900);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("HelloWorld");

	Init();

	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutMouseFunc(mouse);
	glutMainLoop();

	// 关闭 GDI+
	GdiplusShutdown(token);
	return 0;
}
