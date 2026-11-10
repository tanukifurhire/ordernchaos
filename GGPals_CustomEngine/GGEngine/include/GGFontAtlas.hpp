/*************************************************************************
  \file       GGFontAtlas.hpp
  \author     Zafir Haqqym, Abhishek Akilandeswari Patchamuthu
  \par        Email: zafir.h@digipen.edu  , a.patchamuthu@digipen.edu
  \contribute Zafir(80%),Abhishek(%20 Generateintances for text comp/alignment, textlayout)
  \date       20 September 2025
  \version    1.0
  \brief
    Font atlas system text rendering with single texture.
  \details
    Replaces the inefficient one texture per character approach with a
    single atlas texture containing all characters and UV coordinates.
  \dependencies
    <string>, <unordered_map>, <vector>, <GL/glew.h>, <ft2build.h>
  \see
    GGAssetManager, GGEngineRender
  \history
    - 1.0 (20 September 2025): Initial font atlas implementation.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/


#pragma once

#include <GL/glew.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

/*************************************************************************
  \brief
    Character information with UV coordinates in atlas texture
*************************************************************************/
struct GGCharacterInfo
{
    // UV coordinates in atlas (0.0 to 1.0)
    float uvX, uvY;          // Top-left UV coordinate
    float uvWidth, uvHeight; // UV dimensions

    // Character metrics (in pixels)
    glm::ivec2 size;         // Character size
    glm::ivec2 bearing;      // Offset from baseline
    unsigned int advance;    // Distance to next character

    /*************************************************************************
      \brief
        Default constructor
    *************************************************************************/
    GGCharacterInfo()
        : uvX(0), uvY(0), uvWidth(0), uvHeight(0)
        , size(0, 0), bearing(0, 0), advance(0)
    {
    }
};
/*************************************************************************
  \struct TextLayoutInfo
  \brief
    Stores layout information for a block of text after alignment, wrapping,
    and scaling calculations.
*************************************************************************/
struct TextLayoutInfo {
    float totalWidth;      // Total width of the text
    float totalHeight;     // Total height (max character height)
    float baselineOffset;  // Offset from top to baseline
    glm::vec2 startPos;    // Adjusted start position after alignment
    glm::vec4 boundingBox; // x, y, width, height
};

/*************************************************************************
  \enum TextAlignment
  \brief
    Enum specifying horizontal alignment for text rendering.
*************************************************************************/
// Add this enum if not already present
enum class TextAlignment {
    LEFT = 0,
    CENTER = 1,
    RIGHT = 2
};

/*************************************************************************
  \enum VerticalAlignment
  \brief
    Enum specifying vertical alignment for text rendering.
*************************************************************************/
enum class VerticalAlignment {
    TOP = 0,
    MIDDLE = 1,
    BOTTOM = 2,
    BASELINE = 3
};


/*************************************************************************
  \struct TextInstanceData
  \brief
    Represents a single instance of a character for GPU text rendering.
    Stores position, rotation, scale, texture info, color, layer, and sorting.
*************************************************************************/
struct TextInstanceData {
    glm::vec3 translation{};      // World position (x, y, z-depth)
    float rotation{};             // Rotation in radians

    glm::vec3 scale{};            // Scale (width, height, 1.0)
    uint32_t useTexture{};             // Whether to use texture (always 1 for text)

    glm::vec4 uvBounds{};         // Character UV coordinates (x, y, w, h)
    GLuint textureID{};            // OpenGL texture ID (CPU-side grouping)
    uint32_t _pad_texID{};         // Padding to maintain alignment

    int32_t layer{};                  // Render layer
    int32_t ySort{};                  // Y-sorting flag

    glm::vec4 colorMultiply{};    // Text color
    glm::vec4 colorAdd{};         // Additive color (usually 0)
   
    
    unsigned int isFontTexture = 1;  // Mark as font texture
    unsigned int padding[3] = { 0,0,0 };
};

/*************************************************************************
  \brief
    Font atlas class that packs all characters into single texture
  \par Usage
  \code
    GGFontAtlas atlas;
    atlas.LoadFont("../../Assets/Fonts/SourceSans3-Bold.ttf", 48);

    // Render text using atlas
    atlas.RenderText(shader, "Hello World", x, y, scale, color);
  \endcode
*************************************************************************/
class GGFontAtlas
{
public:
    /*************************************************************************
      \brief
        Constructor
    *************************************************************************/
    GGFontAtlas();

    /*************************************************************************
      \brief
        Destructor with cleanup
    *************************************************************************/
    ~GGFontAtlas();

    /*************************************************************************
      \brief
        Load font and generate atlas texture
      \param[in] fontPath
        Path to font file
      \param[in] fontSize
        Font size in pixels
      \return
        True if font loaded successfully
    *************************************************************************/
    bool LoadFont(const std::string& fontPath, unsigned int fontSize);

    /*************************************************************************
      \brief
        Render text using the font atlas
      \param[in] shader
        Shader program for text rendering
      \param[in] text
        Text string to render
      \param[in] x
        X position
      \param[in] y
        Y position
      \param[in] scale
        Text scale factor
      \param[in] color
        Text color
    *************************************************************************/
    void RenderText(GLuint shaderProgram, const std::string& text,
        float x, float y, float scale, const glm::vec3& color);

    /*************************************************************************
      \brief
        Calculate text dimensions for layout
      \param[in] text
        Text to measure
      \param[in] scale
        Scale factor
      \return
        Text dimensions as vec2
    *************************************************************************/
    glm::vec2 GetTextSize(const std::string& text, float scale) const;

    void DebugPrintCharacterInfo(char c) const;

    /*************************************************************************
      \brief
        Get atlas texture ID for binding
      \return
        OpenGL texture ID
    *************************************************************************/
    GLuint GetAtlasTexture() const { return atlasTextureID; }

    /*************************************************************************
      \brief
        Check if atlas is ready for use
      \return
        True if atlas loaded successfully
    *************************************************************************/
    bool IsValid() const { return atlasTextureID != 0 && !characters.empty(); }

    /*************************************************************************
      \brief
        Get atlas dimensions for debugging
      \return
        Atlas dimensions as ivec2
    *************************************************************************/
    glm::ivec2 GetAtlasDimensions() const { return glm::ivec2(atlasWidth, atlasHeight); }

    /*TextLayoutInfo CalculateTextLayout(
        const std::string& text,
        float x,
        float y,
        float scale,
        TextAlignment alignment,
        VerticalAlignment verticalAlignment,
        float maxWidth = 0.0f
    ) const;*/
    // Generate instances with alignment support
    void GenerateTextInstancesAligned(
        const std::string& text,
        float x,
        float y,
        float scale,
        const glm::vec4& color,
        std::vector<TextInstanceData>& outInstances,
        int layer,
        float rotation,
        bool ySort,
        TextAlignment alignment = TextAlignment::LEFT,
        VerticalAlignment verticalAlignment = VerticalAlignment::BASELINE,
        float maxWidth = 0.0f,
        TextLayoutInfo* outLayout = nullptr,
        float lineSpacing = 1.2f
    );
    //void GenerateTextInstances(const std::string& text, float x, float y, float scale, const glm::vec3& color,std::vector<TextInstanceData>& outInstances,int layer =0, float rotation=0.0f, bool ySort= false);

    bool VerifyAtlas() const;

    GLuint64 GetAtlasTextureHandle() const;//??

    GLuint getTextureID()const {
        return atlasTextureID;
      }
    const std::string& GetFontPath() const { return fontPath; }
    unsigned int GetFontSize() const { return fontSize; }


private:
    GLuint atlasTextureID;                                    // Single atlas texture
    std::unordered_map<char, GGCharacterInfo> characters;     // Character lookup

    // Atlas dimensions
    unsigned int atlasWidth;
    unsigned int atlasHeight;

    // Rendering resources
    GLuint VAO, VBO;

    // Font information
    unsigned int fontSize;
    std::string fontPath;


    //helper for the maxwidth
    std::vector<std::string> WrapText(const std::string& text, float maxWidth, float scale) const;

    /*************************************************************************
      \brief
        Generate atlas texture from loaded characters
      \param[in] face
        FreeType face object
      \return
        True if atlas generated successfully
    *************************************************************************/
    bool GenerateAtlas(FT_Face face);

    /*************************************************************************
      \brief
        Setup OpenGL resources for rendering
    *************************************************************************/
    void SetupRenderingResources();

    /*************************************************************************
      \brief
        Clean up OpenGL resources
    *************************************************************************/
    void Cleanup();

    float GetCharAdvance(char c, float scale) const {
        auto it = characters.find(c);
        return (it != characters.end()) ? it->second.advance * scale : 0.0f;
    }
};

/*************************************************************************
  \brief
    Font atlas manager integrating with asset system
*************************************************************************/
class GGFontAtlasManager
{
public:
    /*************************************************************************
      \brief
        Get or create font atlas
      \param[in] fontPath
        Path to font file
      \param[in] fontSize
        Font size in pixels
      \return
        Reference to font atlas
    *************************************************************************/
    static GGFontAtlas& GetFontAtlas(const std::string& fontPath, unsigned int fontSize);

    /*************************************************************************
      \brief
        Clear all cached atlases
    *************************************************************************/
    static void ClearCache();


    // for assetmanager
   static const std::unordered_map<std::string, std::unique_ptr<GGFontAtlas>>& GetAtlasCache() 
    {
        return atlasCache;
    }
private:
    static std::unordered_map<std::string, std::unique_ptr<GGFontAtlas>> atlasCache;

    /*************************************************************************
      \brief
        Generate cache key for font atlas
    *************************************************************************/
    static std::string GenerateKey(const std::string& fontPath, unsigned int fontSize);
};

/*************************************************************************
  \brief
    Test function to demonstrate font atlas functionality
*************************************************************************/
void TestFontAtlas();