/*************************************************************************
  \file       Transform2D.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       7 September 2025
  \version    1.0
  \brief
    Defines the Transform2D class in the GGMath namespace.
  \details
    The Transform2D class provides a convenient wrapper for 2D transformations
    combining position, rotation, and scale components. It maintains these
    transformation components separately for easy manipulation while providing
    matrix generation for rendering and world-space coordinate calculations.
    Supports hierarchical transformations and common game object operations.
  \dependencies
    <iostream>, Constants.hpp, Vector2.hpp, Matrix3x3.hpp, Types.hpp
  \see
    Vector2, Matrix3x3, Rectangle
  \history
    - 1.0 (7 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <iostream>
#include "Constants.hpp"
#include "Vector2.hpp"
#include "Matrix3x3.hpp"
#include "Types.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        2D transformation class combining position, rotation, and scale components
      \par Usage
      \code
        Transform2D playerTransform(playerPosition, playerAngle, Vector2(1, 1));
        Transform2D entityTransform;
        entityTransform.setWorldPosition(spawnLocation);
        Matrix3x3 worldMatrix = playerTransform.computeTransformMatrix();
      \endcode
    *************************************************************************/
    class Transform2D
    {
    public:
        Vector2 worldPosition;      // Translation coordinates in world space
        float32 rotationAngle;      // Rotation angle in radians (positive = counterclockwise)
        Vector2 scaleFactors;       // Scale multipliers for X and Y axes (1.0 = no scaling)

        /*************************************************************************
          \brief
            Default constructor - creates identity transformation at origin
          \par Usage
          \code
            Transform2D entityTransform;        // Position (0,0), rotation 0, scale (1,1)
            Transform2D defaultTransform;       // Identity transformation
          \endcode
          \par Output
            Creates transformation with no translation, rotation, or scaling applied
          \par Purpose
            Initialize default transformation state for game objects at origin
        *************************************************************************/
        Transform2D() : worldPosition(Vector2::zero()), rotationAngle(0.0f), scaleFactors(Vector2(1.0f, 1.0f))
        {
        }

        /*************************************************************************
          \brief
            Constructor with world position only, default rotation and scale
          \param[in] initialPosition
            Starting world position coordinates
          \par Usage
          \code
            Transform2D spriteTransform(spawnPosition);
            Transform2D uiElementTransform(Vector2(100, 200));
          \endcode
          \par Output
            Creates transformation with specified position, zero rotation, unit scale
          \par Purpose
            Initialize positioned object with default orientation and size
        *************************************************************************/
        explicit Transform2D(const Vector2& initialPosition)
            : worldPosition(initialPosition), rotationAngle(0.0f), scaleFactors(Vector2(1.0f, 1.0f))
        {
        }

        /*************************************************************************
          \brief
            Constructor with world position and rotation, default scale
          \param[in] initialPosition
            Starting world position coordinates
          \param[in] initialRotation
            Starting rotation angle in radians
          \par Usage
          \code
            Transform2D playerTransform(playerSpawnPoint, playerFacingAngle);
            Transform2D enemyTransform(enemyPosition, attackDirection);
          \endcode
          \par Output
            Creates transformation with position and rotation, unit scale factors
          \par Purpose
            Initialize positioned and oriented object with default size
        *************************************************************************/
        Transform2D(const Vector2& initialPosition, float32 initialRotation)
            : worldPosition(initialPosition), rotationAngle(initialRotation), scaleFactors(Vector2(1.0f, 1.0f))
        {
        }

        /*************************************************************************
          \brief
            Full constructor with position, rotation, and scale components
          \param[in] initialPosition
            Starting world position coordinates
          \param[in] initialRotation
            Starting rotation angle in radians
          \param[in] initialScale
            Starting scale factors for both axes
          \par Usage
          \code
            Transform2D spriteTransform(spritePosition, spriteAngle, Vector2(2, 2));
            Transform2D uiTransform(menuPosition, 0, menuScale);
          \endcode
          \par Output
            Creates transformation with all specified transformation components
          \par Purpose
            Complete initialization of transformation for complex game objects
        *************************************************************************/
        Transform2D(const Vector2& initialPosition, float32 initialRotation, const Vector2& initialScale)
            : worldPosition(initialPosition), rotationAngle(initialRotation), scaleFactors(initialScale)
        {
        }

        /*************************************************************************
          \brief
            Update world position using Vector2 coordinates
          \param[in] newPosition
            New world position coordinates
          \par Usage
          \code
            entityTransform.setWorldPosition(targetLocation);
            playerTransform.setWorldPosition(respawnPoint);
          \endcode
          \par Output
            Updates worldPosition component of transformation
          \par Purpose
            Move game object to specific world coordinates
        *************************************************************************/
        void setWorldPosition(const Vector2& newPosition)
        {
            worldPosition = newPosition;
        }

        /*************************************************************************
          \brief
            Update world position using separate coordinate parameters
          \param[in] xCoordinate
            New X coordinate in world space
          \param[in] yCoordinate
            New Y coordinate in world space
          \par Usage
          \code
            entityTransform.setWorldPosition(mouseX, mouseY);
            spriteTransform.setWorldPosition(100.0f, 200.0f);
          \endcode
          \par Output
            Updates worldPosition.x and worldPosition.y components
          \par Purpose
            Convenient position setting when coordinates are available separately
        *************************************************************************/
        void setWorldPosition(float32 xCoordinate, float32 yCoordinate)
        {
            worldPosition.x = xCoordinate;
            worldPosition.y = yCoordinate;
        }

        /*************************************************************************
          \brief
            Update rotation angle in radians
          \param[in] newRotationAngle
            New rotation angle in radians (positive = counterclockwise)
          \par Usage
          \code
            entityTransform.setRotationAngle(PI / 4);       // 45 degrees counterclockwise
            playerTransform.setRotationAngle(targetAngle);
          \endcode
          \par Output
            Updates rotationAngle component of transformation
          \par Purpose
            Orient game object to specific angle for rendering and collision
        *************************************************************************/
        void setRotationAngle(float32 newRotationAngle)
        {
            rotationAngle = newRotationAngle;
        }

        /*************************************************************************
          \brief
            Update scale factors with uniform scaling
          \param[in] uniformScaleFactor
            Scale multiplier applied to both X and Y axes
          \par Usage
          \code
            entityTransform.setUniformScale(2.0f);          // Double size
            spriteTransform.setUniformScale(0.5f);          // Half size
          \endcode
          \par Output
            Updates both scaleFactors.x and scaleFactors.y to uniformScaleFactor
          \par Purpose
            Resize game object while maintaining aspect ratio
        *************************************************************************/
        void setUniformScale(float32 uniformScaleFactor)
        {
            scaleFactors.x = uniformScaleFactor;
            scaleFactors.y = uniformScaleFactor;
        }

        /*************************************************************************
          \brief
            Update scale factors with non-uniform scaling
          \param[in] newScaleFactors
            Scale multipliers for X and Y axes as Vector2
          \par Usage
          \code
            entityTransform.setScaleFactors(Vector2(2.0f, 1.0f));    // Stretch horizontally
            uiTransform.setScaleFactors(menuScaling);
          \endcode
          \par Output
            Updates scaleFactors component with new X and Y scale values
          \par Purpose
            Resize game object with different scaling for each axis
        *************************************************************************/
        void setScaleFactors(const Vector2& newScaleFactors)
        {
            scaleFactors = newScaleFactors;
        }

        /*************************************************************************
          \brief
            Apply relative translation to current world position
          \param[in] translationOffset
            Movement vector to add to current position
          \par Usage
          \code
            entityTransform.applyTranslation(velocityVector * deltaTime);
            playerTransform.applyTranslation(movementInput);
          \endcode
          \par Output
            Adds translationOffset to current worldPosition
          \par Purpose
            Move game object relative to current position for movement updates
        *************************************************************************/
        void applyTranslation(const Vector2& translationOffset)
        {
            worldPosition += translationOffset;
        }

        /*************************************************************************
          \brief
            Apply relative rotation to current angle
          \param[in] rotationOffset
            Additional rotation angle in radians to add
          \par Usage
          \code
            entityTransform.applyRotation(angularVelocity * deltaTime);
            playerTransform.applyRotation(PI / 4);          // Add 45 degrees
          \endcode
          \par Output
            Adds rotationOffset to current rotationAngle
          \par Purpose
            Rotate game object relative to current orientation
        *************************************************************************/
        void applyRotation(float32 rotationOffset)
        {
            rotationAngle += rotationOffset;
        }

        /*************************************************************************
          \brief
            Apply relative scaling to current scale factors
          \param[in] scaleMultipliers
            Scale factors to multiply with current scale
          \par Usage
          \code
            entityTransform.applyScaling(Vector2(1.1f, 1.1f));     // Grow by 10%
            spriteTransform.applyScaling(Vector2(2.0f, 1.0f));     // Double width only
          \endcode
          \par Output
            Multiplies current scaleFactors by scaleMultipliers component-wise
          \par Purpose
            Apply scaling relative to current size for growth/shrink effects
        *************************************************************************/
        void applyScaling(const Vector2& scaleMultipliers)
        {
            scaleFactors.x *= scaleMultipliers.x;
            scaleFactors.y *= scaleMultipliers.y;
        }

        /*************************************************************************
          \brief
            Generate transformation matrix for rendering and coordinate conversion
          \return
            Matrix3x3 representing complete transformation (Translation * Rotation * Scale)
          \par Usage
          \code
            Matrix3x3 worldMatrix = entityTransform.computeTransformMatrix();
            Matrix3x3 mvpMatrix = projectionMatrix * viewMatrix * transform.computeTransformMatrix();
          \endcode
          \par Output
            Returns transformation matrix applying scale, then rotation, then translation
          \par Purpose
            Convert transformation components to matrix for graphics operations
        *************************************************************************/
        Matrix3x3 computeTransformMatrix() const
        {
            return Matrix3x3::TRS(worldPosition, rotationAngle, scaleFactors);
        }

        /*************************************************************************
          \brief
            Calculate forward direction vector based on current rotation
          \return
            Unit vector pointing in transformation's forward direction
          \par Usage
          \code
            Vector2 forwardDirection = playerTransform.computeForwardVector();
            Vector2 movementDirection = entityTransform.computeForwardVector() * speed;
          \endcode
          \par Output
            Returns normalized vector calculated as (cos(rotation), sin(rotation))
          \par Purpose
            Determine movement direction based on object's current orientation
        *************************************************************************/
        Vector2 computeForwardVector() const
        {
            return Vector2(std::cos(rotationAngle), std::sin(rotationAngle));
        }

        /*************************************************************************
          \brief
            Calculate right direction vector perpendicular to forward direction
          \return
            Unit vector pointing to transformation's right side
          \par Usage
          \code
            Vector2 rightDirection = entityTransform.computeRightVector();
            Vector2 strafeMovement = playerTransform.computeRightVector() * strafeSpeed;
          \endcode
          \par Output
            Returns normalized vector perpendicular to forward direction
          \par Purpose
            Calculate perpendicular movement direction for strafing mechanics
        *************************************************************************/
        Vector2 computeRightVector() const
        {
            return Vector2(-std::sin(rotationAngle), std::cos(rotationAngle));
        }

        /*************************************************************************
          \brief
            Transform point from local object space to world space coordinates
          \param[in] localSpacePoint
            Point coordinates relative to object's local coordinate system
          \return
            Point transformed to world space coordinate system
          \par Usage
          \code
            Vector2 worldPoint = entityTransform.transformPointToWorldSpace(localOffset);
            Vector2 worldCorner = spriteTransform.transformPointToWorldSpace(Vector2(0.5f, 0.5f));
          \endcode
          \par Output
            Returns point with scale, rotation, and translation applied
          \par Purpose
            Convert local object coordinates to world coordinates for collision detection
        *************************************************************************/
        Vector2 transformPointToWorldSpace(const Vector2& localSpacePoint) const
        {
            return computeTransformMatrix() * localSpacePoint;
        }

        /*************************************************************************
          \brief
            Transform direction vector from local space to world space (ignores translation)
          \param[in] localSpaceDirection
            Direction vector in object's local coordinate system
          \return
            Direction vector transformed to world space coordinate system
          \par Usage
          \code
            Vector2 worldDirection = entityTransform.transformDirectionToWorldSpace(localDirection);
            Vector2 worldVelocity = playerTransform.transformDirectionToWorldSpace(Vector2::right());
          \endcode
          \par Output
            Returns direction with only rotation and scale applied (no translation)
          \par Purpose
            Transform direction vectors while preserving vector properties
        *************************************************************************/
        Vector2 transformDirectionToWorldSpace(const Vector2& localSpaceDirection) const
        {
            // Apply only rotation and scale transformations, exclude translation
            Matrix3x3 rotationScaleMatrix = Matrix3x3::rotation(rotationAngle) * Matrix3x3::scale(scaleFactors.x, scaleFactors.y);
            return rotationScaleMatrix * localSpaceDirection;
        }

        /*************************************************************************
          \brief
            Reset transformation to identity state (origin, no rotation, unit scale)
          \par Usage
          \code
            entityTransform.resetToIdentity();     // Clear all transformations
            playerTransform.resetToIdentity();     // Return to default state
          \endcode
          \par Output
            Sets worldPosition to (0,0), rotationAngle to 0, scaleFactors to (1,1)
          \par Purpose
            Clear all transformations and return object to default state
        *************************************************************************/
        void resetToIdentity()
        {
            worldPosition = Vector2::zero();
            rotationAngle = 0.0f;
            scaleFactors = Vector2(1.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Linear interpolation between two transformation states
          \param[in] sourceTransform
            Starting transformation state (t = 0.0)
          \param[in] targetTransform
            Target transformation state (t = 1.0)
          \param[in] interpolationParameter
            Interpolation factor [0.0, 1.0] determining blend amount
          \return
            Interpolated transformation between source and target
          \par Usage
          \code
            Transform2D smoothTransform = Transform2D::interpolateTransforms(startTransform, endTransform, 0.5f);
            Transform2D cameraTransform = Transform2D::interpolateTransforms(currentCamera, targetCamera, smoothingFactor);
          \endcode
          \par Output
            Returns blended transformation with proper angle interpolation
          \par Purpose
            Smooth animation between transformation states with wrap-around angle handling
        *************************************************************************/
        static Transform2D interpolateTransforms(const Transform2D& sourceTransform, const Transform2D& targetTransform, float32 interpolationParameter)
        {
            Transform2D resultTransform;
            resultTransform.worldPosition = Vector2::lerp(sourceTransform.worldPosition, targetTransform.worldPosition, interpolationParameter);

            // Interpolate rotation angle with proper wrap-around handling for shortest path
            float32 angleDifference = targetTransform.rotationAngle - sourceTransform.rotationAngle;
            if (angleDifference > PI) angleDifference -= 2.0f * PI;
            if (angleDifference < -PI) angleDifference += 2.0f * PI;
            resultTransform.rotationAngle = sourceTransform.rotationAngle + angleDifference * interpolationParameter;

            resultTransform.scaleFactors = Vector2::lerp(sourceTransform.scaleFactors, targetTransform.scaleFactors, interpolationParameter);
            return resultTransform;
        }
    };

    /*************************************************************************
      \brief
        Stream output operator for debugging and logging
      \param[in] outputStream
        Output stream to write transformation data to
      \param[in] transform
        Transform2D object to output
      \return
        Reference to output stream for method chaining
      \par Usage
      \code
        std::cout << playerTransformation;      // Print transformation components
        debugLog << "Entity transform: " << entityTransform;
      \endcode
      \par Output
        Outputs transform in format "Transform2D(pos: Vector2(x, y), rot: angle, scale: Vector2(x, y))"
      \par Purpose
        Debug output and logging support for transformation objects
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& outputStream, const Transform2D& transform)
    {
        outputStream << "Transform2D(pos: " << transform.worldPosition
            << ", rot: " << transform.rotationAngle
            << ", scale: " << transform.scaleFactors << ")";
        return outputStream;
    }
}