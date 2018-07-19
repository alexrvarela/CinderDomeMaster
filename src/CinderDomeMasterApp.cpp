#include "Resources.h"

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/VboMesh.h"
#include "cinder/Camera.h"
#include "cinder/Sphere.h"
#include "cinder/Arcball.h"
#include "cinder/ObjLoader.h"
#include "cinder/Utilities.h"

using namespace ci;
using namespace ci::app;
using namespace std;

//Constants
#define VEC3_ZERO vec3( 0.0f )
#define VEC3_XAXIS vec3(1.0f, 0.0f, 0.0f)
#define VEC3_YAXIS vec3(0.0f, 1.0f, 0.0f)
#define VEC3_ZAXIS vec3(0.0f, 0.0f, 1.0f)

//3600 //2400 //1200
#define DOME_MAP_TEXTURE_SIZE 2400
#define DOME_MAP_FBO_SIZE DOME_MAP_TEXTURE_SIZE
#define CUBE_FBO_SIZE DOME_MAP_TEXTURE_SIZE / 3

#define CAMERA_NEAR 0.001f
#define CAMERA_FAR 1000.0f
#define CAMERA_FOV 40.0f
#define CUBE_FOV 90.0f

#define DOME_MASTER_SCALE 0.20f
#define CUBE_MAP_SCALE 0.15f

//SCENE PARAMS
#define SPHERE_SUBDIV 50

class CinderDomeMasterApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
    void mouseDrag( MouseEvent event ) override;
	void update() override;
	void draw() override;

    void prepareSettings(Settings *settings);
    void setupCameras();
    void setupFBO();

    //Cubemap cameras
    vector<gl::FboRef> cubeFBOs;
    vector<CameraPersp> cameraList;
    vector<vec3> cameraAxis;
    vector<float> cameraRotations;

    //dome master
    void drawDomeMaster();
    gl::FboRef domeFbo;
    vector<gl::VboMeshRef> domeMeshes;
    vector<gl::BatchRef> domeBatches;
    vector<ObjLoader> domeMeshLoaders;
    vector<DataSourceRef> domeResources;
    float domeDistance;
    CameraPersp domeCamera;
    
    //Scene
    gl::FboRef sceneFBO;
    void setupScene();
    void drawScene();
    void drawCubeMap();
    void renderCube();
    void drawMain();
    void restoreViewport();
    
    Arcball mArcball;
    CameraPersp mCamera;
    Sphere mSphere;
    gl::BatchRef mBatch;
    gl::Texture2dRef mTexture;//deprecate

};

//Setup

void CinderDomeMasterApp::setupCameras()
{
    //**** CUBE CAMERAS
    for(int i = 0; i < 6; i++)
    {
        CameraPersp camera  = CameraPersp();
        camera.setPerspective(CUBE_FOV,
                              1.0,
                              CAMERA_NEAR,
                              CAMERA_FAR);
        
        camera.lookAt(vec3(1.0, 1.25, 1.0),
                      vec3(1.0));
        
        cameraList.push_back(camera);
    }
    
    cameraAxis.push_back( - VEC3_YAXIS );//top
    cameraAxis.push_back( VEC3_XAXIS );//left
    cameraAxis.push_back( VEC3_ZAXIS );//front
    cameraAxis.push_back( - VEC3_XAXIS );//rigth
    cameraAxis.push_back( - VEC3_ZAXIS );//back
    cameraAxis.push_back( VEC3_YAXIS );//bottom
    
    cameraRotations.push_back(180.0f);//top
    cameraRotations.push_back(270.0f);//left
    cameraRotations.push_back(0);//front
    cameraRotations.push_back(-270.0f);//rigth
    cameraRotations.push_back(180.0f);//back
    cameraRotations.push_back(0);//bottom
    
    for(int i = 0; i < cameraList.size(); i++)
    {
        //set camera location and orientation
        cameraList[i].lookAt(
                             vec3(0.0),
                             cameraAxis[i]
                             );
        
        //rotate camera z axis
        quat rotation = angleAxis(
                                  toRadians( cameraRotations[i] ),
                                  VEC3_ZAXIS
                                  );
        
        cameraList[i].setOrientation(cameraList[i].getOrientation() * rotation);
    }
    
    //**** DOME MAP
    domeDistance =  3.0;
    domeCamera  = CameraPersp();
    domeCamera.setPerspective(CAMERA_FOV, 1.0, CAMERA_NEAR, CAMERA_FAR);
    domeCamera.lookAt( vec3(0.0f, domeDistance, 0.0f), vec3( 0 ), VEC3_YAXIS);
    
    //**** SCENE
    mCamera = CameraPersp();
    mCamera.setPerspective( CAMERA_FOV, 1.0, CAMERA_NEAR, CAMERA_FAR );
    mCamera.lookAt( vec3( 0, 0, 3 ), vec3( 0 ) );
//    mCamera.lookAt(vec3( 0 ), VEC3_ZERO, VEC3_YAXIS * -1.0f);
}


void CinderDomeMasterApp::setupFBO()
{
    for(int i = 0; i < 6; i++)
    {
        cubeFBOs.push_back(gl::Fbo::create(CUBE_FBO_SIZE, CUBE_FBO_SIZE));
    }
    
    domeFbo = gl::Fbo::create(DOME_MAP_TEXTURE_SIZE, DOME_MAP_TEXTURE_SIZE);
    
    domeResources.push_back(loadResource( DOME_MASTER_TOP ));//0
    domeResources.push_back(loadResource( DOME_MASTER_LEFT ));//1
    domeResources.push_back(loadResource( DOME_MASTER_FRONT ));//2
    domeResources.push_back(loadResource( DOME_MASTER_RIGHT ));//3
    domeResources.push_back(loadResource( DOME_MASTER_BACK ));//4
    domeResources.push_back(loadResource( DOME_MASTER_BOTTOM ));//5
    
    for(int i = 0; i < domeResources.size(); i++)
    {
        gl::VboMeshRef    mesh;
        ObjLoader loader( domeResources[i ]);
        mesh = gl::VboMesh::create( loader );
        
        auto def = gl::ShaderDef().texture();
        gl::BatchRef batch = gl::Batch::create(
                                            mesh,
                                            gl::getStockShader( def )
                                            );
        domeBatches.push_back(batch);
        domeMeshes.push_back(mesh);
        domeMeshLoaders.push_back(loader);//*/
    }
    
    sceneFBO = gl::Fbo::create(getWindowSize().y, getWindowSize().y);
    
}


void CinderDomeMasterApp::setupScene()
{
    //Make env sphere
    mTexture = gl::Texture::create( loadImage( loadResource( RES_TEXTURE ) ) );
    mSphere = Sphere( vec3( 0.0 ), 1.0);
    
    auto def = gl::ShaderDef().texture();
    mBatch = gl::Batch::create(geom::Sphere( mSphere ).subdivisions( SPHERE_SUBDIV ),
                               gl::getStockShader( def )
                               );
  
    mArcball = Arcball( &mCamera, mSphere );
}


void CinderDomeMasterApp::setup()
{
    //Cameras
    setupCameras();
    
    //Frame buffer objects
    setupFBO();
    
    //Scene
    setupScene();
}

void CinderDomeMasterApp::mouseDown( MouseEvent event )
{
    mArcball.mouseDown( event );
}

void CinderDomeMasterApp::mouseDrag( MouseEvent event )
{
    mArcball.mouseDrag( event );
}

//Update
void CinderDomeMasterApp::update()
{
    
}

//Draw
void CinderDomeMasterApp::renderCube()
{
    //Render cube map textures
    gl::viewport(ivec2(0, 0),
                 cubeFBOs[0]->getSize());
    
    for(int i = 0; i < cubeFBOs.size(); i++)
    {
        //draw inside frame buffer
        cubeFBOs[i]->bindFramebuffer();

        gl::pushMatrices();
        gl::setMatrices(cameraList[i]);
        
        //DRAW SCENE
        drawScene();
        
        gl::popMatrices();
        cubeFBOs[i]->unbindFramebuffer();
    }
}
void CinderDomeMasterApp::drawDomeMaster()
{
    //Draw cube map FBOs textures on dome master meshes
    //start draw inside frame buffer
    gl::viewport( ivec2(0, 0),
                 domeFbo->getSize() );
    domeFbo->bindFramebuffer();
    gl::pushMatrices();
    gl::setMatrices(domeCamera);
    gl::clear( Color( 0.5f, 0.5f, 0.5f ) );
    
    glEnable(GL_DEPTH_TEST);
    
    for(int i = 0; i < cubeFBOs.size(); i++)
    {
        gl::Texture2dRef texture = cubeFBOs[i]->getColorTexture();
        texture->bind();
        domeBatches[i]->draw();
        texture->unbind();
    }
    
    glDisable(GL_DEPTH_TEST);
    
    gl::popMatrices();
    domeFbo->unbindFramebuffer();
    
    //Finally Draw FBO texture
    restoreViewport();
    
    vec2 translate = vec2(sceneFBO->getSize().x, 0.0f);
    
    gl::pushMatrices();
    gl::scale(vec3( DOME_MASTER_SCALE ));
    gl::translate(translate * (1.0f / DOME_MASTER_SCALE) );
    gl::draw(domeFbo->getColorTexture(), vec2(0.0f, 0.0f));
    gl::popMatrices();
}

void CinderDomeMasterApp::restoreViewport()
{
    //restore viewport
    gl::viewport(
                 ivec2(0, 0),
                 ivec2(
                       getWindowBounds().getWidth(),
                       getWindowBounds().getHeight()
                       )
                 );
}

void CinderDomeMasterApp::drawMain()
{
    gl::viewport(
                 ivec2(0, 0),
                 sceneFBO->getSize()
                 );
    
    sceneFBO->bindFramebuffer();
    gl::pushMatrices();
    gl::setMatrices( mCamera );
    
    drawScene();
    
    gl::popMatrices();
    sceneFBO->unbindFramebuffer();

    //Draw FBO texture
    restoreViewport();
     gl::draw(sceneFBO->getColorTexture(), vec2(0.0f, 0.0f));
}

void CinderDomeMasterApp::drawCubeMap()
{
    vec2 translate = vec2(sceneFBO->getSize().x, domeFbo->getSize().y * DOME_MASTER_SCALE);
    float s = (float)(CUBE_FBO_SIZE);

    //draw cube map textures
    for(int i = 0; i < cubeFBOs.size(); i++)
    {
        gl::Texture2dRef texture = cubeFBOs[i]->getColorTexture();
        
        gl::pushMatrices();
        gl::scale(vec3( CUBE_MAP_SCALE ));
        gl::translate(translate * (1.0f / CUBE_MAP_SCALE) );
        
        float x;
        float y;
        
        if(i == 0){x = s * 2; y = s;}//top
        if(i == 1){x = s; y = s;}//left
        if(i == 2){x = s * 2; y = 0.0f;}//front
        if(i == 3){x = s * 3; y = s;}//rigth
        if(i == 4){x = s * 2; y = s * 2;}//back
        if(i == 5){x = 0.0f; y = s;}//bottom
        
        gl::draw(texture, vec2(x, y));
        gl::popMatrices();
    }
}

void CinderDomeMasterApp::drawScene()
{
    //DRAW CHECKER
    gl::clear();
    glEnable(GL_DEPTH_TEST);
    gl::pushMatrices();
    
    gl::rotate( mArcball.getQuat() );
    
    mTexture->bind();
    mBatch->draw();
    mTexture->unbind();
    
    gl::popMatrices();

    glDisable(GL_DEPTH_TEST);
    gl::color( Color(1.0f, 1.0f, 1.0f) );
}

void CinderDomeMasterApp::draw()
{
    gl::clear( Color( 0.5f, 0.5f, 0.5f ) );
    
    renderCube();
    drawDomeMaster();
    drawMain();
    drawCubeMap();
}

CINDER_APP( CinderDomeMasterApp, RendererGl, [&]( App::Settings *settings )
{
    settings->setWindowSize( 1920, 1200 );
    settings->setFrameRate( 30 );
    settings->setMultiTouchEnabled( false );
})
