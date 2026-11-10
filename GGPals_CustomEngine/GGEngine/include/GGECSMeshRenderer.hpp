/*************************************************************************
  \file       GGECSMeshRenderer.hpp
   \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       03 October 2025
  \brief
    ECS system for instanced rendering of meshes/sprites, with optional
    debug visuals and dynamic transform effects.

  \details
    GGMeshRendererSystem collects entities having both a transform and a mesh
    (or sprite), builds per-instance data (position, scale, rotation, UVs,
    texture handles), then uploads and draws them via GPU instancing
    (glDrawElementsInstanced). It also supports:

    - Bindless texture usage (ARB_bindless_texture) for per-instance texture binding.
    - Global dynamic scaling (sinusoidal) and rotation toggles for visual effects.
    - Debug rendering of AABBs, velocity vectors, squares, circles, and center points
      through 2D render helpers.

    This design intends to minimize draw calls by batching instance data each frame
    and issuing a single instanced draw per mesh.
*************************************************************************/

#pragma once

#include <cstdint>
#include <GGSystemInterface.hpp>
#include <GL/glew.h>  
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

//#include <GGEditorCamera2D.hpp>

/*************************************************************************
  \struct InstanceData
  \brief Stores per-instance data for GPU instanced rendering.
  \details
    This structure contains all necessary information for rendering
    a single instance of a mesh or sprite:
    - Translation, rotation, and scale
    - Texture usage flags, UV bounds, and bindless texture handle
    - Padding to maintain 16-byte alignment for SSBOs
*************************************************************************/
struct InstanceData {
    glm::vec3 translation{};       // 12 bytes
    float rotation{};              // 4 bytes  completes first 16 bytes

    glm::vec3 scale{};             // 12 bytes
    uint32_t useTexture{};         // 4 bytes

    glm::vec4 uvBounds{};        // 16 bytes

    uint32_t textureID{};          // 4 bytes — OpenGL texture ID (CPU-side grouping)
    uint32_t _pad_texID{};         // 4 bytes — padding to maintain 8-byte alignment

    int32_t layer{};                // 4 bytes
    int32_t ySort{};                // 4 bytes

    glm::vec4 colorAdd{};
    glm::vec4 colorMultiply{};
    uint32_t isFontTexture = 0;  // 1 for font, 0 for sprite
    

    //object effects
    uint32_t effectType  {};    //  4 bytes  FOG/FADE/BLOOM/OUTLINE/DISSOLVE/SHINE
    uint32_t loop{};
    uint32_t useEffect{};
    uint32_t padding{};  //
    float    intensity = {};  //  4 bytes
    float    effectStartTime{};  //  4 bytes
    float    effectDuration{};  //  4 bytes

    glm::vec4 effectColor{}; // 16 bytes

    
};

/*************************************************************************
  \struct PickingInstanceData
  \brief Per-instance data for object picking (color-coded selection).
  \details
    Similar to InstanceData but replaces color modifiers with pickingColor
    used for GPU-based object selection.
*************************************************************************/
struct PickingInstanceData {
    glm::vec3 translation{};       // 12 bytes
    float rotation{};              // 4 bytes  completes first 16 bytes

    glm::vec3 scale{};             // 12 bytes
    uint32_t useTexture{};         // 4 bytes

    glm::vec4 uvBounds{};        // 16 bytes

    uint32_t textureID{};          // 4 bytes — OpenGL texture ID (CPU-side grouping)
    uint32_t _pad_texID{};         // 4 bytes — padding to maintain 8-byte alignment

    int32_t layer{};                // 4 bytes
    int32_t ySort{};                // 4 bytes

    glm::vec4 pickingColor{};

    uint32_t pickableObject{1};
    uint32_t pixelPerfectPicking{};
    uint32_t padding[2];
};
/*************************************************************************
  \class GGMeshRendererSystem
  \brief ECS system for rendering meshes/sprites with GPU instancing.
  \details
    GGMeshRendererSystem inherits from SystemInterface and manages:
    - Collecting entities with Transform2DFloat + MeshRenderer/Sprite
    - Building per-instance data
    - Uploading to GPU and issuing instanced draws
    - Optional dynamic scaling and rotation for visual effects
    - Debug rendering for colliders, velocity, squares, circles, and points

    The system maintains static toggles for debug visuals and dynamic transforms.
*************************************************************************/

class GGMeshRendererSystem : public SystemInterface {
public:
    /*************************************************************************
      \brief Initializes system resources (GPU buffers, shaders, etc.)
      \return true if initialization succeeds, false otherwise
    *************************************************************************/
    bool Init() override;

    /*************************************************************************
      \brief Updates system per-frame (e.g., dynamic scaling/rotation)
      \param world Reference to ECS world
    *************************************************************************/
	void Update(World& world) override;

    /*************************************************************************
      \brief Renders objects with GPU-based picking colors
      \param world Reference to ECS world
    *************************************************************************/
    void RenderPicking(World& world);

    /*************************************************************************
      \brief Renders UI-specific picking objects
      \param world Reference to ECS world
    *************************************************************************/
    void RenderUIPicking(World& world);

    /*************************************************************************
      \brief Renders UI elements using instancing
      \param world Reference to ECS world
    *************************************************************************/
    void RenderUI(World& world);

    /*************************************************************************
      \brief Renders all standard game objects
      \param world Reference to ECS world
    *************************************************************************/
    void RenderObjects(World& world);

    void SampleTextTest(World& world);

  
    void PushInstance(InstanceData&& instance);


   
    // toggles for synamic scaling and rotating
    static bool enableDynamicScale;
    static float dynamicScaleAmplitude;
    static float dynamicScaleFrequency;
    static bool enableDynamicRotation;
    static float dynamicRotationSpeed;
    // Getter methods
    static bool ShowDebugPoints() { return showDebugPoints; }
    static bool ShowDebugLines() { return showDebugLines; }
    static bool ShowDebugAABBs() { return showDebugAABBs; }
    static bool ShowDebugCircles() { return showDebugCircles; }
    static bool ShowDebugSquares() { return showDebugSquares; }

    // Setter/toggle methods
    static void SetDebugPoints(bool val) { showDebugPoints = val; }
    static void SetDebugLines(bool val) { showDebugLines = val; }
    static void SetDebugAABBs(bool val) { showDebugAABBs = val; }
    static void SetDebugCircles(bool val) { showDebugCircles = val; }
    static void SetDebugSquares(bool val) { showDebugSquares = val; }

    
    //gizmo mode system
    enum class GizmoMode {
        TRANSLATE,
        ROTATE,
        SCALE
    };
    static GizmoMode currentGizmoMode;
    // --- Gizmo dragging state ---
    static bool draggingAxis;
    static enum class Axis { NONE, X, Y ,ROTATE}draggedAxis;
    static glm::vec2 dragStartMouse;
    static glm::vec2 dragStartEntityPos;
    //static GGEditorCamera2D m_editorCamera;


    static bool showMoveGizmo;
    static uint32_t selectedEntity;        // Currently selected entity
    static float gizmoHandleRadius;
    static float gizmoLineLength;

    // for rotaion gizmo state
    static float rotationGizmoRadius;
    static float rotationStartAngle;
    static float entityStartRotation;
    static glm::vec2 rotationDragStartPos;
    static float rotationCurrentAngle;

    // for scale gizmo state 
    static float scaleHandleSize;
   
    static glm::vec2 scaleStartEntityScale;
    static glm::vec2 scaleStartMouseWorld;
    enum class ScaleHandle {
        NONE,
        CENTER,
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM_LEFT,
        BOTTOM_RIGHT
    };
    static ScaleHandle draggedScaleHandle;
private:
    static bool showDebugPoints;
    static bool showDebugLines;
    static bool showDebugAABBs;
    static bool showDebugCircles;
    static bool showDebugSquares;

    std::vector<InstanceData> instances;

    

};