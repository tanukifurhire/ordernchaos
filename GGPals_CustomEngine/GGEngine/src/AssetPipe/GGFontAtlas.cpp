/*************************************************************************
  \file       GGFontAtlas.cpp
  \author     Zafir Haqqym, Abhishek Akilandeswari Patchamuthu
  \par        Email: zafir.h@digipen.edu  , a.patchamuthu@digipen.edu
  \contribute Zafir(80%),Abhishek(%20 Generateintances for text comp,wrap text)
  \date       29 January 2026
  \version    1.3
  \brief
    Implementation of font atlas system for text rendering.
  \details
    Packs all font characters into single texture atlas for performance.
    Replaces the inefficient one texture per character approach.
  \dependencies
    GGFontAtlas.hpp, <iostream>, <algorithm>, <cmath>
  \see
    GGFontAtlas.hpp
  \history
    - 1.0 (20 September 2025): Initial implementation.
    - 1.1 (1 October 2025): Fixed cross-platform crash by limiting character
      range to printable ASCII (32-126) instead of full ASCII (0-127).
      Control characters (0-31) caused FreeType to return inconsistent
      fallback glyphs across systems. Added GL_UNPACK_ALIGNMENT setting
      for better FreeType compatibility. Added comprehensive error checking
      for texture operations.
    - 1.2 (1 October 2025): Fixed UV coordinate inversion for correct text
      orientation. Added model matrix uniform support for shader compatibility.
      Added texture sampler uniform binding. Integrated with editor debug
      rendering system.
    - 1.3 (29 January 2026): Fixed GL_INVALID_OPERATION error spam by detecting
      when texture is created successfully despite spurious GL error. Added
      comprehensive debug logging and texture validation. Texture creation now
      continues if glIsTexture confirms texture is valid.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
// Debug toggle for FontAtlas
#ifndef FONT_ATLAS_DEBUG_ENABLED
#define FONT_ATLAS_DEBUG_ENABLED 0
#endif

#if FONT_ATLAS_DEBUG_ENABLED
#define FONTATLAS_DEBUG(msg) std::cout << "DEBUG: " << msg << std::endl
#else
#define FONTATLAS_DEBUG(msg)
#endif

#include <GGFontAtlas.hpp>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <GGEngine.hpp>

// Static member for atlas manager
std::unordered_map<std::string, std::unique_ptr<GGFontAtlas>> GGFontAtlasManager::atlasCache;

/*************************************************************************
  \brief
    Constructor initializing member variables
*************************************************************************/
GGFontAtlas::GGFontAtlas() : atlasTextureID(0), atlasWidth(0), atlasHeight(0), VAO(0), VBO(0), fontSize(0)
{
}

/*************************************************************************
  \brief
    Destructor with cleanup
*************************************************************************/
GGFontAtlas::~GGFontAtlas()
{
    Cleanup();
}

/*************************************************************************
  \brief
    Load font and generate atlas texture with AssetManager integration
*************************************************************************/
bool GGFontAtlas::LoadFont(const std::string& fontFilePath, unsigned int fontPixelSize)
{
    // Store font metadata for later reference
    this->fontPath = fontFilePath;
    this->fontSize = fontPixelSize;

    // Initialize FreeType library for font loading
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR: Could not init FreeType Library" << std::endl;
        return false;
    }

    // Load font face using AssetManager path resolution
    FT_Face face;
    std::string resolvedPath;

    // Integrate with AssetManager for proper path resolution
    try
    {
        // AssetManager automatically prepends base path
        resolvedPath = GGEngine::GetEngine().GetAssetManager()->ResolvePath(fontFilePath);
        FONTATLAS_DEBUG("FontAtlas resolved '" << fontFilePath << "' to '" << resolvedPath << "'");
    }
    catch (...)
    {
        // Fallback to direct path if AssetManager unavailable
        resolvedPath = fontFilePath;
        std::cout << "WARNING: FontAtlas using direct path: " << fontFilePath << std::endl;
    }

    // Attempt to load the font file
    if (FT_New_Face(ft, resolvedPath.c_str(), 0, &face))
    {
        std::cout << "ERROR: Failed to load font: " << resolvedPath << std::endl;
        FT_Done_FreeType(ft);
        return false;
    }

    // Set font size in pixels (0 for width means auto-calculate from height)
    FT_Set_Pixel_Sizes(face, 0, fontPixelSize);

    // Generate atlas texture from all characters
    bool success = GenerateAtlas(face);

    // Cleanup FreeType resources
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Setup OpenGL rendering resources if atlas generation succeeded
    if (success)
    {
        SetupRenderingResources();
        std::cout << "SUCCESS: Font atlas created from " << fontFilePath << " (" << atlasWidth << "x" << atlasHeight << ")" << std::endl;
    }

    return success;
}

/*************************************************************************
  \brief
    Wraps input text into multiple lines based on maxWidth constraint.
    Respects word boundaries if possible, and splits words exceeding maxWidth at the character level.
  \param[in] text
    Input string to wrap.
  \param[in] maxWidth
    Maximum allowed width in units; words exceeding this width are split.
  \param[in] scale
    Scale factor to compute actual width of characters.
  \return
    Vector of strings, each representing a wrapped line.
*************************************************************************/
std::vector<std::string> GGFontAtlas::WrapText(const std::string& text, float maxWidth, float scale) const
{
    std::vector<std::string> lines;
    if (maxWidth <= 0.0f) {
        lines.push_back(text);
        return lines;
    }

    std::string currentLine;
    float currentWidth = 0.0f;

    auto getCharWidth = [&](char c)
        {
            auto it = characters.find(c);
            return (it != characters.end()) ? it->second.advance * scale : 0.0f;
        };

    std::istringstream words(text);
    std::string word;

    while (words >> word) {
        // Calculate word width
        float wordWidth = 0.0f;
        for (char c : word)
            wordWidth += getCharWidth(c);

        float spaceWidth = currentLine.empty()
            ? 0.0f
            : getCharWidth(' ');

        // Word fits on current line
        if (currentWidth + spaceWidth + wordWidth <= maxWidth)
        {
            if (!currentLine.empty())
            {
                currentLine += " ";
                currentWidth += spaceWidth;
            }
            currentLine += word;
            currentWidth += wordWidth;
        }
        else
        {
            // Push current line if not empty
            if (!currentLine.empty())
            {
                lines.push_back(currentLine);
                currentLine.clear();
                currentWidth = 0.0f;
            }

            // Word itself is too large  character wrapping
            if (wordWidth > maxWidth)
            {
                for (char c : word)
                {
                    float charWidth = getCharWidth(c);

                    if (currentWidth + charWidth > maxWidth)
                    {
                        lines.push_back(currentLine);
                        currentLine.clear();
                        currentWidth = 0.0f;
                    }

                    currentLine += c;
                    currentWidth += charWidth;
                }
            }
            else
            {
                // Word fits on a new line
                currentLine = word;
                currentWidth = wordWidth;
            }
        }
    }

    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    return lines;
}

/*************************************************************************
  \brief
    Generate atlas texture from loaded characters
*************************************************************************/
bool GGFontAtlas::GenerateAtlas(FT_Face face)
{
    // First pass: calculate total area needed for all characters
    std::vector<glm::ivec2> characterSizes;
    int totalArea = 0;
    int maxHeight = 0;

    // Loop through printable ASCII characters only (32-126)
    // Skipping control characters (0-31) prevents cross-platform crashes
    for (unsigned char c = 32; c < 127; c++)
    {
        // Load character glyph with rendering
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "WARNING: Failed to load character: " << (int)c << std::endl;
            continue;
        }

        // Get character dimensions from FreeType bitmap
        int width = face->glyph->bitmap.width;
        int height = face->glyph->bitmap.rows;

        // Accumulate size data for atlas dimension calculations
        characterSizes.push_back({ width, height });
        totalArea += width * height;
        maxHeight = std::max(maxHeight, height);
    }

    // Calculate atlas dimensions (square-ish, power of 2 for GPU efficiency)
    int atlasSize = (int)std::sqrt(totalArea * 2.0f); // Add 50% padding for spacing
    atlasSize = 1 << (int)std::ceil(std::log2(atlasSize)); // Round up to nearest power of 2
    atlasSize = std::max(atlasSize, 256); // Minimum 256x256 texture

    atlasWidth = atlasSize;
    atlasHeight = atlasSize;

    // === INVESTIGATION: Debug logging ===
    std::cout << "\n=== FONT ATLAS DEBUG ===" << std::endl;
    std::cout << "Font: " << fontPath << " | Size: " << fontSize << "px" << std::endl;
    std::cout << "Calculated atlas: " << atlasWidth << "x" << atlasHeight << std::endl;

    // Clear any previous GL errors
    GLenum priorErr;
    int priorErrCount = 0;
    while ((priorErr = glGetError()) != GL_NO_ERROR)
    {
        std::cout << "! Cleared prior GL error: " << priorErr << std::endl;
        priorErrCount++;
        if (priorErrCount > 10) break;
    }
    if (priorErrCount > 0)
        std::cout << "! Had " << priorErrCount << " prior errors" << std::endl;
    else
        std::cout << "No prior GL errors" << std::endl;

    // Check GPU limits
    GLint maxTexSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    std::cout << "GPU max texture: " << maxTexSize << std::endl;
    if (atlasWidth > static_cast<unsigned int>(maxTexSize) || atlasHeight > static_cast<unsigned int>(maxTexSize))
    {
        std::cout << "ERROR: ATLAS TOO LARGE!" << std::endl;
        return false;
    }

    std::cout << "Creating texture..." << std::endl;
    glGenTextures(1, &atlasTextureID);
    GLenum genErr = glGetError();
    std::cout << "  glGenTextures: ID=" << atlasTextureID << " err=" << genErr << std::endl;

    glBindTexture(GL_TEXTURE_2D, atlasTextureID);
    GLenum bindErr = glGetError();
    std::cout << "  glBindTexture: err=" << bindErr << std::endl;

    std::cout << "  glTexImage2D: GL_RED format..." << std::endl;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    GLenum err = glGetError();
    std::cout << "  glTexImage2D result: err=" << err << std::endl;
    if (err != GL_NO_ERROR)
    {
        std::cout << "WARNING: glTexImage2D reported error: " << err << std::endl;
        std::cout << "  1280=GL_INVALID_ENUM 1281=GL_INVALID_VALUE 1282=GL_INVALID_OPERATION" << std::endl;

        // Check if texture actually exists despite error
        GLboolean texExists = glIsTexture(atlasTextureID);
        std::cout << "  glIsTexture: " << (int)texExists << std::endl;

        GLint actualWidth = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &actualWidth);
        GLenum queryErr = glGetError();
        std::cout << "  Query texture width: " << actualWidth << " (err=" << queryErr << ")" << std::endl;

        // FIX: If texture exists and is queryable, continue despite error
        if (texExists && actualWidth > 0 && queryErr == GL_NO_ERROR)
        {
            std::cout << "  --> TEXTURE IS VALID - Continuing despite GL error" << std::endl;
            std::cout << "  --> This is a known GL_INVALID_OPERATION false positive" << std::endl;
            std::cout << "SUCCESS: Texture usable (error ignored)" << std::endl;
            std::cout << "===========================\n" << std::endl;
            // Continue with texture creation (don't return false)
        }
        else
        {
            // Texture genuinely failed
            std::cout << "  ERROR: Texture creation genuinely failed" << std::endl;
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &atlasTextureID);
            atlasTextureID = 0;
            std::cout << "=== CREATION FAILED ===\n" << std::endl;
            return false;
        }
    }
    else
    {
        std::cout << "SUCCESS: Texture created without errors!" << std::endl;
        std::cout << "===========================\n" << std::endl;
    }

    // Set texture sampling parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Prevent wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Smooth scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Set pixel unpack alignment for FreeType bitmap compatibility
    // FreeType bitmaps are tightly packed, so we need 1-byte alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Initialize packing position with small padding
    int currentX = 2;
    int currentY = 2;
    int rowHeight = 0;

    // Log initial state and clear previous OpenGL errors
    std::cout << "Beginning atlas packing: " << atlasWidth << "x" << atlasHeight << std::endl;
    glGetError();

    // Second pass: pack character bitmaps into the atlas texture
    for (unsigned char c = 32; c < 127; c++)
    {
        // Reload character for bitmap access
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            continue;

        int charWidth = face->glyph->bitmap.width;
        int charHeight = face->glyph->bitmap.rows;

        // Check if character fits in current row
        if (currentX + charWidth + 2 > (int)atlasWidth)
        {
            // Move to next row with padding
            currentX = 2;
            currentY += rowHeight + 2;
            rowHeight = 0;
        }

        // Check if we have vertical space remaining
        if (currentY + charHeight + 2 > (int)atlasHeight)
        {
            std::cout << "ERROR: Atlas too small for all characters" << std::endl;
            return false;
        }

        // Copy character bitmap to atlas if it has dimensions
        if (charWidth > 0 && charHeight > 0)
        {
            // Verify character fits within atlas bounds
            if (currentX + charWidth > (int)atlasWidth || currentY + charHeight > (int)atlasHeight)
            {
                std::cout << "ERROR: Character '" << (char)c << "' out of bounds at (" << currentX << "," << currentY << ") size (" << charWidth << "x" << charHeight << ")" << std::endl;
                glBindTexture(GL_TEXTURE_2D, 0);
                return false;
            }

            // Verify bitmap buffer is valid (prevents crashes on some systems)
            if (face->glyph->bitmap.buffer == nullptr)
            {
                std::cout << "WARNING: Character '" << (char)c << "' has null bitmap buffer" << std::endl;
                continue;
            }

            // Debug logging for character upload
            //std::cout << "Uploading char '" << (char)c << "' at (" << currentX << "," << currentY << ") size (" << charWidth << "x" << charHeight << ")" << std::endl;

            // Upload character bitmap to atlas at current position
            glTexSubImage2D(GL_TEXTURE_2D, 0, currentX, currentY, charWidth, charHeight, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

            // Check for upload errors
            GLenum uploadErr = glGetError();
            if (uploadErr != GL_NO_ERROR)
            {
                std::cout << "ERROR: glTexSubImage2D failed for char '" << (char)c << "' with error: " << uploadErr << std::endl;
                glBindTexture(GL_TEXTURE_2D, 0);
                return false;
            }
        }

        // Store character metadata for later rendering
        GGCharacterInfo& charInfo = characters[c];

        // Calculate UV coordinates (normalized 0-1 range)
        charInfo.uvX = (float)currentX / atlasWidth;
        charInfo.uvY = (float)currentY / atlasHeight;
        charInfo.uvWidth = (float)charWidth / atlasWidth;
        charInfo.uvHeight = (float)charHeight / atlasHeight;

        // Store character rendering metrics
        charInfo.size = glm::ivec2(charWidth, charHeight);
        charInfo.bearing = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
        charInfo.advance = face->glyph->advance.x >> 6; // Convert from 1/64 pixels to pixels

        // Update packing position for next character
        currentX += charWidth + 2; // Add padding between characters
        rowHeight = std::max(rowHeight, charHeight);
    }

    // Unbind texture after packing complete
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

/*************************************************************************
  \brief
    Setup OpenGL resources for rendering
*************************************************************************/
void GGFontAtlas::SetupRenderingResources()
{
    // Create VAO and VBO for quad rendering
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Allocate buffer for quad vertices (6 vertices, 4 floats each: x, y, u, v)
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    // Setup vertex attribute: position (xy) + UV coordinates (zw)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

    // Unbind for safety
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/*************************************************************************
  \brief
    Render text using the font atlas
*************************************************************************/
void GGFontAtlas::RenderText(GLuint shaderProgram, const std::string& text, float x, float y, float scale, const glm::vec3& color)
{
    // Early exit if atlas not ready
    if (!IsValid()) return;

    // Activate shader and set text color uniform
    glUseProgram(shaderProgram);
    glUniform3f(glGetUniformLocation(shaderProgram, "textColor"), color.x, color.y, color.z);

    // Set identity model matrix (shader expects this uniform)
    GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
    if (modelLoc != -1)
    {
        glm::mat4 identityModel = glm::mat4(1.0f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &identityModel[0][0]);
    }

    // Set texture sampler uniform
    GLint texLoc = glGetUniformLocation(shaderProgram, "text");
    if (texLoc != -1)
    {
        glUniform1i(texLoc, 0); // Texture unit 0
    }
    else
    {
        std::cout << "WARNING: 'text' uniform not found in shader" << std::endl;
    }

    // Bind atlas texture and VAO for rendering
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTextureID);
    // std::cout << "DEBUG: Rendering with atlas texture ID: " << atlasTextureID << std::endl;
    glBindVertexArray(VAO);

    // Render each character in the string
    for (const char& c : text)
    {
        // Find character in atlas
        auto it = characters.find(c);
        if (it == characters.end()) continue; // Skip unknown characters

        const GGCharacterInfo& ch = it->second;

        // Calculate character position on screen
        float xpos = x + ch.bearing.x * scale;
        float ypos = y - (ch.size.y - ch.bearing.y) * scale; // Adjust for baseline
        float w = ch.size.x * scale;
        float h = ch.size.y * scale;

        // Create quad vertices with position and UV coordinates
        float vertices[6][4] = {
            { xpos,     ypos + h, ch.uvX,               ch.uvY               }, // Top-left (V flipped)
            { xpos,     ypos,     ch.uvX,               ch.uvY + ch.uvHeight }, // Bottom-left (V flipped)
            { xpos + w, ypos,     ch.uvX + ch.uvWidth,  ch.uvY + ch.uvHeight }, // Bottom-right (V flipped)

            { xpos,     ypos + h, ch.uvX,               ch.uvY               }, // Top-left (V flipped)
            { xpos + w, ypos,     ch.uvX + ch.uvWidth,  ch.uvY + ch.uvHeight }, // Bottom-right (V flipped)
            { xpos + w, ypos + h, ch.uvX + ch.uvWidth,  ch.uvY               }  // Top-right (V flipped)
        };

        // Update VBO with current character quad data
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Draw the character quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Advance cursor position for next character
        x += ch.advance * scale;
    }

    // Cleanup bindings
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/*************************************************************************
  \brief
    Calculate text dimensions for layout
*************************************************************************/
glm::vec2 GGFontAtlas::GetTextSize(const std::string& text, float scale) const
{
    float width = 0;
    float height = 0;

    // Accumulate width from character advances and find max height
    for (const char& c : text)
    {
        auto it = characters.find(c);
        if (it == characters.end()) continue;

        const GGCharacterInfo& ch = it->second;
        width += ch.advance * scale; // Horizontal advance includes spacing
        height = std::max(height, ch.size.y * scale); // Track tallest character
    }

    return glm::vec2(width, height);
}

void GGFontAtlas::DebugPrintCharacterInfo(char c) const
{
    auto it = characters.find(c);
    if (it == characters.end()) {
        std::cout << "Character '" << c << "' (ASCII " << (int)c << ") NOT FOUND in atlas" << std::endl;
        return;
    }

    const GGCharacterInfo& ch = it->second;
    std::cout << "Character '" << c << "' (ASCII " << (int)c << "):" << std::endl;
    std::cout << "  UV: (" << ch.uvX << ", " << ch.uvY << ") size (" << ch.uvWidth << ", " << ch.uvHeight << ")" << std::endl;
    std::cout << "  Size: " << ch.size.x << "x" << ch.size.y << " pixels" << std::endl;
    std::cout << "  Bearing: (" << ch.bearing.x << ", " << ch.bearing.y << ")" << std::endl;
    std::cout << "  Advance: " << ch.advance << std::endl;
}

////need to remove
//TextLayoutInfo GGFontAtlas::CalculateTextLayout(const std::string& text, float x, float y, float scale, TextAlignment alignment, VerticalAlignment verticalAlignment, float maxWidth) const
//{
//    TextLayoutInfo layout;
//
//    // Calculate text dimensions
//    layout.totalWidth = 0.0f;
//    layout.totalHeight = 0.0f;
//    layout.baselineOffset = 0.0f;
//
//    for (const char& c : text) {
//        auto it = characters.find(c);
//        if (it == characters.end()) continue;
//
//        const GGCharacterInfo& ch = it->second;
//        layout.totalWidth += ch.advance * scale;
//        layout.totalHeight = std::max(layout.totalHeight, (float)ch.size.y * scale);
//        layout.baselineOffset = std::max(layout.baselineOffset, (float)ch.bearing.y * scale);
//    }
//
//    // Apply horizontal alignment
//    float alignedX = x;
//    switch (alignment) {
//    case TextAlignment::CENTER:
//        alignedX = x - layout.totalWidth * 0.5f;
//        break;
//    case TextAlignment::RIGHT:
//        alignedX = x - layout.totalWidth;
//        break;
//    case TextAlignment::LEFT:
//    default:
//        alignedX = x;
//        break;
//    }
//
//    // Apply vertical alignment
//    float alignedY = y;
//    switch (verticalAlignment) {
//    case VerticalAlignment::TOP:
//        alignedY = y + layout.totalHeight;
//        break;
//    case VerticalAlignment::MIDDLE:
//        alignedY = y + layout.totalHeight * 0.5f;
//        break;
//    case VerticalAlignment::BOTTOM:
//        alignedY = y;
//        break;
//    case VerticalAlignment::BASELINE:
//    default:
//        alignedY = y + layout.baselineOffset;
//        break;
//    }
//
//    layout.startPos = glm::vec2(alignedX, alignedY);
//
//    // Calculate bounding box
//    layout.boundingBox = glm::vec4(
//        alignedX,
//        alignedY - layout.totalHeight,
//        layout.totalWidth,
//        layout.totalHeight
//    );
//
//    return layout;
//}

/*************************************************************************
  \brief
    Generate GPU text instances for rendering, with alignment, wrapping, and optional rotation.
    Supports horizontal alignment (LEFT, CENTER, RIGHT), vertical alignment (TOP, MIDDLE, BOTTOM, BASELINE),
    line wrapping based on maxWidth, and optional rotation around the anchor point.
  \param[in] text
    The string to render.
  \param[in] x
    X-coordinate of the text anchor point.
  \param[in] y
    Y-coordinate of the text anchor point.
  \param[in] scale
    Uniform scale for the text size.
  \param[in] color
    RGB color applied to the text.
  \param[out] outInstances
    Vector to store generated TextInstanceData for GPU rendering.
  \param[in] layer
    Layer ID for the instances.
  \param[in] rotation
    Rotation in radians applied around the anchor point.
  \param[in] ySort
    Whether to enable Y-sorting for rendering.
  \param[in] alignment
    Horizontal text alignment (LEFT, CENTER, RIGHT).
  \param[in] verticalAlignment
    Vertical text alignment (TOP, MIDDLE, BOTTOM, BASELINE).
  \param[in] maxWidth
    Maximum width before wrapping lines; <=0 disables wrapping.
  \param[out] outLayout
    Optional pointer to store layout info (total width, total height, bounding box).
*************************************************************************/
void GGFontAtlas::GenerateTextInstancesAligned(const std::string& text, float x, float y, float scale, const glm::vec4& color, std::vector<TextInstanceData>& outInstances, int layer, float rotation, bool ySort, TextAlignment alignment, VerticalAlignment verticalAlignment, float maxWidth, TextLayoutInfo* outLayout,float lineSpacing)
{
    if (!IsValid()) return;
    if (text.empty()) return;

    // Store atlas texture ID for CPU-side texture binding
    GLuint atlasTexID = atlasTextureID;

    // Handle text wrapping if maxWidth is specified
    std::vector<std::string> baseLines;
    std::stringstream ss(text);
    std::string segment;

    while (std::getline(ss, segment, '\n')) {
        baseLines.push_back(segment);
    }
    std::vector<std::string> lines;
    for (const auto& line : baseLines)
    {
        if (maxWidth > 0.0f)
        {
            auto wrapped = WrapText(line, maxWidth, scale);
            lines.insert(lines.end(), wrapped.begin(), wrapped.end());
        }
        else
        {
            lines.push_back(line);
        }
    }
    //fail safe for the -ve spacing

    if (lineSpacing <= 0.0f)
        lineSpacing = 1.2f;
    // Calculate line height (use max character height)
    float lineHeight = 0.0f;
    for (const auto& pair : characters) {
        lineHeight = std::max(lineHeight, (float)pair.second.size.y * scale);
    }
    lineHeight *= lineSpacing; // Add 20% line spacing

    float totalTextHeight = lines.size() * lineHeight;


    std::vector<float> lineWidths;
    float maxLineWidth = 0.0f;

    for (const std::string& line : lines) {
        float lineWidth = 0.0f;
        for (const char& c : line) {
            auto it = characters.find(c);
            if (it != characters.end()) {
                lineWidth += it->second.advance * scale;
            }
        }
        lineWidths.push_back(lineWidth);
        maxLineWidth = std::max(maxLineWidth, lineWidth);
    }

    // Calculate the starting X position for the entire text block based on max width
    float textBlockStartX = x;
    //switch (alignment) {
    //case TextAlignment::CENTER:
    //    // Center the text block around the anchor point x
    //    textBlockStartX = x;
    //    break;
    //case TextAlignment::RIGHT:
    //    // Align the text block's right edge to the anchor point x
    //    textBlockStartX = x;
    //    break;
    //case TextAlignment::LEFT:
    //default:
    //    // Align the text block's left edge to the anchor point x
    //    textBlockStartX = x;
    //    break;
    //}

    // Adjust Y position based on vertical alignment for the ENTIRE text block
    float startY = y;
    float adjustedX = x;
    switch (verticalAlignment) {
    case VerticalAlignment::TOP:
        startY = y;
        break;
    case VerticalAlignment::MIDDLE:
        // Center the entire text block vertically around the anchor point
        startY = y + (totalTextHeight * 0.5f);
        textBlockStartX = x - (maxLineWidth * 0.5f);
        adjustedX = x;
        break;
    case VerticalAlignment::BOTTOM:
        startY = y + totalTextHeight;
        break;
    case VerticalAlignment::BASELINE:
    default:
        startY = y;
        break;
    }

    // Track bounding box for all lines
    float minX = FLT_MAX;
    float maxX = -FLT_MAX;
    float minY = FLT_MAX;
    float maxY = -FLT_MAX;

    float currentY = startY;
    float texalX = (1.f / static_cast<float>(atlasWidth)) * 0.5f;
    float texalY = (1.f / static_cast<float>(atlasHeight)) * 0.5f;
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::string& line = lines[lineIndex];
        float lineWidth = lineWidths[lineIndex];

        // Each line starts at the text block's left edge
        // Then we can optionally center/right-align within the block
        float lineStartX = textBlockStartX;



        switch (alignment) {
        case TextAlignment::CENTER:
            lineStartX = textBlockStartX + (maxLineWidth - lineWidth) * 0.5f;
            break;
        case TextAlignment::RIGHT:
            lineStartX = textBlockStartX + (maxLineWidth - lineWidth);
            break;
        case TextAlignment::LEFT:
        default:
            lineStartX = textBlockStartX;
            break;
        }


        // Calculate line bounds for bounding box
        float lineTop = currentY;
        float lineBottom = currentY - lineHeight;

        // Update overall bounding box
        minX = std::min(minX, lineStartX);
        maxX = std::max(maxX, lineStartX + lineWidth);
        minY = std::min(minY, lineBottom);
        maxY = std::max(maxY, lineTop);

        // Render characters in this line
        float currentX = lineStartX;
        float cosR = cosf(rotation);
        float sinR = sinf(rotation);

        for (const char& c : line) {
            auto it = characters.find(c);
            if (it == characters.end()) continue;

            const GGCharacterInfo& ch = it->second;

            // Calculate character position
            float xpos = currentX + ch.bearing.x * scale;
            float ypos = currentY - (ch.size.y - ch.bearing.y) * scale;
            float w = ch.size.x * scale;
            float h = ch.size.y * scale;

            // Update overall bounding box
            minX = std::min(minX, xpos);
            maxX = std::max(maxX, xpos + w);
            minY = std::min(minY, ypos);
            maxY = std::max(maxY, ypos + h);

            // Calculate center for rotation
            float centerX = xpos + w * 0.5f;
            float centerY = ypos + h * 0.5f;

            float rotationCenterX = (verticalAlignment == VerticalAlignment::MIDDLE) ? adjustedX : x;

            // Apply rotation around the anchor point (x, y)
            if (rotation != 0.0f) {
                float dx = centerX - rotationCenterX;
                float dy = centerY - y;
                centerX = rotationCenterX + (dx * cosR - dy * sinR);
                centerY = y + (dx * sinR + dy * cosR);
            }

            // Create instance
            TextInstanceData instance{};
            instance.translation = glm::vec3(centerX, centerY, 0.0f);
            instance.rotation = rotation;
            instance.scale = glm::vec3(w, h, 1.0f);

            instance.uvBounds = glm::vec4(
                ch.uvX+texalX,
                ch.uvY + ch.uvHeight - texalY,
                ch.uvX + ch.uvWidth - texalX,
                ch.uvY + texalY
            );

            instance.colorMultiply = glm::vec4(color);
            instance.colorAdd = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
            instance.textureID = atlasTexID;
            instance.useTexture = 1;
            instance.isFontTexture = 1;
            instance.layer = layer;
            instance.ySort = ySort ? 1 : 0;

            outInstances.push_back(instance);

            currentX += ch.advance * scale;
        }

        currentY -= lineHeight; // Move to next line (downward)
    }

    // Fill out layout info if requested
    if (outLayout) {
        outLayout->totalWidth = maxLineWidth;
        outLayout->totalHeight = totalTextHeight;
        outLayout->startPos = glm::vec2(x, startY);

        // Bounding box: left, bottom, width, height
        if (minX <= maxX && minY <= maxY) {
            outLayout->boundingBox = glm::vec4(
                minX,              // left edge
                minY,              // bottom edge
                maxX - minX,       // width
                maxY - minY        // height
            );
        }
        else {
            // Empty text or no valid characters
            outLayout->boundingBox = glm::vec4(x, startY, 0.0f, 0.0f);
        }
    }
}
//void GGFontAtlas::GenerateTextInstances(const std::string& text, float x, float y, float scale, const glm::vec3& color, std::vector<TextInstanceData>& outInstances, int layer, float rotation, bool ySort)
//{
//    if (!IsValid()) return;
//    if (text.empty()) return;
//    
//  /*  std::cout << "\n=== GenerateTextInstances DEBUG ===" << std::endl;
//    std::cout << "Text: \"" << text << "\"" << std::endl;
//    std::cout << "Text length: " << text.length() << std::endl;
//    std::cout << "Position: (" << x << ", " << y << ")" << std::endl;
//    std::cout << "Characters in atlas: " << characters.size() << std::endl;*/
//    // Get bindless texture handle for the atlas
//    GLuint64 atlasHandle = glGetTextureHandleARB(atlasTextureID);
//    if (!glIsTextureHandleResidentARB(atlasHandle)) {
//        glMakeTextureHandleResidentARB(atlasHandle);
//    }
//    
//    float currentX = x;
//    
//    // Precompute rotation values if rotating entire text
//    float cosR = cosf(rotation);
//    float sinR = sinf(rotation);
//    
//    int charIndex = 0;
//    for (const char& c : text) {
//        //std::cout << "\nProcessing char #" << charIndex << ": '" << c << "' (ASCII " << (int)c << ")" << std::endl;
//
//        auto it = characters.find(c);
//        if (it == characters.end()) {
//            //std::cout << "  WARNING: Character not found in atlas!" << std::endl;
//
//            //// Print what characters ARE available
//            //if (charIndex == 0) {
//            //    std::cout << "  Available characters: ";
//            //    int count = 0;
//            //    for (const auto& pair : characters) {
//            //        std::cout << "'" << pair.first << "' ";
//            //        if (++count > 20) {
//            //            std::cout << "...";
//            //            break;
//            //        }
//            //    }
//            //    std::cout << std::endl;
//            //}
//
//            charIndex++;
//            continue;
//        } // Skip unknown characters
//
//        const GGCharacterInfo& ch = it->second;
//
//       /* std::cout << "  Found! UV(" << ch.uvX << "," << ch.uvY
//            << ") Size(" << ch.size.x << "x" << ch.size.y
//            << ") Advance(" << ch.advance << ")" << std::endl;*/
//
//        // Calculate character position with bearing
//        float xpos = currentX + ch.bearing.x * scale;
//        float ypos = y - (ch.size.y - ch.bearing.y) * scale;
//        float w = ch.size.x * scale;
//        float h = ch.size.y * scale;
//
//        // Calculate center position for rotation
//        float centerX = xpos + w * 0.5f;
//        float centerY = ypos + h * 0.5f;
//
//        // Apply rotation around text origin if needed
//        if (rotation != 0.0f) {
//            float dx = centerX - x;
//            float dy = centerY - y;
//            centerX = x + (dx * cosR - dy * sinR);
//            centerY = y + (dx * sinR + dy * cosR);
//        }
//
//        // Create instance data for this character
//        TextInstanceData instance{};
//        instance.translation = glm::vec3(centerX, centerY, 0.0f);
//        instance.rotation = rotation;
//        instance.scale = glm::vec3(w, h, 1.0f);
//
//        // Set UV bounds for this character in the atlas
//       instance.uvBounds = glm::vec4(
//            ch.uvX,                    // minU - left edge
//            ch.uvY + ch.uvHeight,                    // minV - top edge
//            ch.uvX + ch.uvWidth,       // maxU - right edge (left + width)
//            ch.uvY        // maxV - bottom edge (top + height)
//        );
//
//        // Set color
//        instance.colorMultiply = glm::vec4(color, 1.0f);
//        instance.colorAdd = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
//
//        // Set texture handle (all characters share same atlas)
//        instance.textureHandle = atlasHandle;
//        instance.useTexture = 1;
//
//        //set font texture
//        instance.isFontTexture = 1;
//        // Set rendering order
//        instance.layer = layer;
//        instance.ySort = ySort ? 1 : 0;
//
//        outInstances.push_back(instance);
//
//        // Advance to next character position
//        currentX += ch.advance * scale;
//       // std::cout << "  Next X position: " << currentX << std::endl;
//        charIndex++;
//    }
//
//    //std::cout << "=== GenerateTextInstances END (created " << outInstances.size() << " instances) ===" << std::endl;
//}
bool GGFontAtlas::VerifyAtlas() const
{
    std::cout << "\n=== ATLAS VERIFICATION ===" << std::endl;
    std::cout << "Atlas dimensions: " << atlasWidth << "x" << atlasHeight << std::endl;
    std::cout << "Texture ID: " << atlasTextureID << std::endl;
    std::cout << "Characters loaded: " << characters.size() << std::endl;

    // Check if we have the basic ASCII characters
    const char* testChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 !";
    bool allFound = true;

    for (const char* p = testChars; *p != '\0'; ++p) {
        if (characters.find(*p) == characters.end()) {
            std::cout << "ERROR: Missing character '" << *p << "' (ASCII " << (int)*p << ")" << std::endl;
            allFound = false;
        }
    }

    if (allFound) {
        std::cout << "SUCCESS: All test characters found" << std::endl;
    }

    // Print first few characters for inspection
    std::cout << "\nFirst 10 characters in atlas:" << std::endl;
    int count = 0;
    for (const auto& pair : characters) {
        const char c = pair.first;
        const GGCharacterInfo& info = pair.second;
        std::cout << "  '" << c << "' (ASCII " << (int)c << "): "
            << "UV(" << info.uvX << "," << info.uvY << ") "
            << "Size(" << info.size.x << "x" << info.size.y << ")" << std::endl;
        if (++count >= 10) break;
    }

    std::cout << "=== VERIFICATION END ===" << std::endl;
    return allFound;
}
GLuint64 GGFontAtlas::GetAtlasTextureHandle() const
{
    // Bindless textures removed for AMD compatibility.
    // Use getTextureID() instead for standard texture binding.
    return static_cast<GLuint64>(atlasTextureID);
}

/*************************************************************************
  \brief
    Clean up OpenGL resources
*************************************************************************/
void GGFontAtlas::Cleanup()
{
    // Delete atlas texture
    if (atlasTextureID != 0)
    {
        glDeleteTextures(1, &atlasTextureID);
        atlasTextureID = 0;
    }

    // Delete VAO
    if (VAO != 0)
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }

    // Delete VBO
    if (VBO != 0)
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }

    // Clear character metadata
    characters.clear();
}

/*************************************************************************
  \brief
    Get or create font atlas
*************************************************************************/
GGFontAtlas& GGFontAtlasManager::GetFontAtlas(const std::string& fontPath, unsigned int fontSize)
{
    // Generate unique cache key for this font+size combination
    std::string key = GenerateKey(fontPath, fontSize);

    // Check if atlas already exists in cache
    auto it = atlasCache.find(key);
    if (it != atlasCache.end())
    {
        return *(it->second);
    }

    // Create new atlas if not cached
    auto atlas = std::make_unique<GGFontAtlas>();
    if (atlas->LoadFont(fontPath, fontSize))
    {
        // Store in cache and return reference
        GGFontAtlas& atlasRef = *atlas;
        atlasCache[key] = std::move(atlas);
        return atlasRef;
    }
    else
    {
        throw std::runtime_error("Failed to load font atlas: " + fontPath);
    }

}

/*************************************************************************
  \brief
    Clear all cached atlases
*************************************************************************/
void GGFontAtlasManager::ClearCache()
{
    // Clear all cached atlases (unique_ptr handles cleanup)
    std::cout << "Font atlas cache cleared" << std::endl;
}

/*************************************************************************
  \brief
    Generate cache key for font atlas
*************************************************************************/
std::string GGFontAtlasManager::GenerateKey(const std::string& fontPath, unsigned int fontSize)
{
    // Combine font path and size into unique key
    return fontPath + ":" + std::to_string(fontSize);
}

/*************************************************************************
  \brief
    Test function with clean asset paths
*************************************************************************/
void TestFontAtlas()
{
    std::cout << "\n=== Font Atlas Test ===" << std::endl;

    try
    {
        // Use clean relative path - AssetManager handles "Assets/" prefix
        GGFontAtlas& atlas = GGFontAtlasManager::GetFontAtlas(
            "Fonts/SourceSans3-Bold.ttf", 48);

        // Verify atlas was created successfully
        if (atlas.IsValid())
        {
            auto dimensions = atlas.GetAtlasDimensions();
            std::cout << "Font atlas created: " << dimensions.x << "x" << dimensions.y << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Font atlas failed: " << e.what() << std::endl;
    }

    std::cout << "=== Font Atlas Complete ===" << std::endl;
}