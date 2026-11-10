/*************************************************************************
  \file       Matrix3x3.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       7 September 2025
  \version    1.0
  \brief
    Defines the Matrix3x3 class in the GGMath namespace.
  \details
    The Matrix3x3 class provides 2D transformation capabilities using
    homogeneous coordinates. It supports translation, rotation, scaling,
    and combination of transformations for 2D graphics and game objects.
    The matrix uses column-major storage for compatibility with graphics APIs.
  \dependencies
    <array>, <cmath>, <iostream>, Constants.hpp, Vector2.hpp, Types.hpp
  \see
    Vector2, Vector3, Transform2D
  \history
    - 1.0 (7 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <array>
#include <cmath>
#include <iostream>
#include "Constants.hpp"
#include "Vector2.hpp"
#include "Types.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        3x3 matrix class for 2D transformations using homogeneous coordinates
      \par Usage
      \code
        Matrix3x3 transform = Matrix3x3::translation(100, 50);
        Matrix3x3 rotation = Matrix3x3::rotation(PI / 4);
        Matrix3x3 combined = transform * rotation;
      \endcode
    *************************************************************************/
    class Matrix3x3
    {
    public:
        // Column-major storage: m[col][row] = m[col * 3 + row]
        // [m[0] m[3] m[6]]
        // [m[1] m[4] m[7]]
        // [m[2] m[5] m[8]]
        std::array<float32, 9> m;

        /*************************************************************************
          \brief
            Default constructor - creates identity matrix
          \par Usage
          \code
            Matrix3x3 transform;            // Identity matrix
            Matrix3x3 identity;             // No transformation
          \endcode
          \par Output
          Creates 3x3 identity matrix (no transformation)
          \par Purpose
          Initialise matrix with no transformation applied
        *************************************************************************/
        Matrix3x3()
        {
            identity();
        }

        /*************************************************************************
          \brief
            Element access operator for matrix[row][col] style access
          \param[in] row
            Row index (0-2)
          \param[in] col
            Column index (0-2)
          \return
            Reference to matrix element
          \par Usage
          \code
            Matrix3x3 mat;
            mat(0, 0) = 1.0f;               // Set top-left element
            float32 value = mat(1, 2);      // Get element at row 1, col 2
          \endcode
          \par Output
          Returns reference to element at specified row and column
          \par Purpose
          Direct access to matrix elements for custom operations
        *************************************************************************/
        float32& operator()(size_t row, size_t col)
        {
            return m[col * 3 + row];
        }

        /*************************************************************************
          \brief
            Const element access operator
          \param[in] row
            Row index (0-2)
          \param[in] col
            Column index (0-2)
          \return
            Const reference to matrix element
          \par Usage
          \code
            const Matrix3x3& mat = getTransform();
            float32 value = mat(0, 0);      // Read element safely
          \endcode
          \par Output
          Returns const reference to element for read-only access
          \par Purpose
          Safe read access to matrix elements
        *************************************************************************/
        const float32& operator()(size_t row, size_t col) const
        {
            return m[col * 3 + row];
        }

        /*************************************************************************
          \brief
            Matrix multiplication - combines transformations
          \param[in] rhs
            Right-hand side matrix to multiply with
          \return
            New matrix representing combined transformation
          \par Usage
          \code
            Matrix3x3 translate = Matrix3x3::translation(100, 50);
            Matrix3x3 rotate = Matrix3x3::rotation(PI / 4);
            Matrix3x3 combined = translate * rotate;    // Apply rotation then translation
          \endcode
          \par Output
          Returns new matrix representing the composition of transformations
          \par Purpose
          Combine multiple transformations into single matrix
        *************************************************************************/
        Matrix3x3 operator*(const Matrix3x3& rhs) const
        {
            Matrix3x3 result;

            for (size_t col = 0; col < 3; ++col)
            {
                for (size_t row = 0; row < 3; ++row)
                {
                    result(row, col) = 0.0f;
                    for (size_t k = 0; k < 3; ++k)
                    {
                        result(row, col) += (*this)(row, k) * rhs(k, col);
                    }
                }
            }

            return result;
        }

        /*************************************************************************
          \brief
            Calculate matrix inverse for transformation reversal
          \return
            Inverse matrix, or identity if not invertible
          \par Usage
          \code
            Matrix3x3 worldToLocal = worldToGlobal.inverse();
            Matrix3x3 undoTransform = transform.inverse();
          \endcode
          \par Output
            Returns inverse transformation matrix
          \par Purpose
            Convert from world space to local space coordinates
        *************************************************************************/
        Matrix3x3 inverse() const
        {
            Matrix3x3 result;

            // Calculate determinant
            float32 det = m[0] * (m[4] * m[8] - m[7] * m[5]) -
                m[3] * (m[1] * m[8] - m[7] * m[2]) +
                m[6] * (m[1] * m[5] - m[4] * m[2]);

            // Check if matrix is invertible
            if (std::abs(det) < FLOAT_EPSILON)
            {
                result.identity();
                return result;
            }

            float32 invDet = 1.0f / det;

            // Calculate cofactor matrix and transpose
            result.m[0] = (m[4] * m[8] - m[7] * m[5]) * invDet;
            result.m[1] = (m[7] * m[2] - m[1] * m[8]) * invDet;
            result.m[2] = (m[1] * m[5] - m[4] * m[2]) * invDet;
            result.m[3] = (m[6] * m[5] - m[3] * m[8]) * invDet;
            result.m[4] = (m[0] * m[8] - m[6] * m[2]) * invDet;
            result.m[5] = (m[3] * m[2] - m[0] * m[5]) * invDet;
            result.m[6] = (m[3] * m[7] - m[6] * m[4]) * invDet;
            result.m[7] = (m[6] * m[1] - m[0] * m[7]) * invDet;
            result.m[8] = (m[0] * m[4] - m[3] * m[1]) * invDet;

            return result;
        }

        /*************************************************************************
          \brief
            Calculate matrix transpose
          \return
            Transposed matrix (rows become columns)
          \par Usage
          \code
            Matrix3x3 transposed = transform.transpose();
            Matrix3x3 normalMatrix = modelMatrix.transpose().inverse();
          \endcode
          \par Output
            Returns matrix with rows and columns swapped
          \par Purpose
            Transform normal vectors and mathematical operations
        *************************************************************************/
        Matrix3x3 transpose() const
        {
            Matrix3x3 result;
            result.m[0] = m[0]; result.m[3] = m[1]; result.m[6] = m[2];
            result.m[1] = m[3]; result.m[4] = m[4]; result.m[7] = m[5];
            result.m[2] = m[6]; result.m[5] = m[7]; result.m[8] = m[8];
            return result;
        }

        /*************************************************************************
          \brief
            Transform Vector2 using homogeneous coordinates
          \param[in] vector
            2D vector to transform
          \return
            Transformed 2D vector
          \par Usage
          \code
            Matrix3x3 transform = Matrix3x3::translation(50, 100);
            Vector2 position(10, 20);
            Vector2 newPos = transform * position;      // Result: (60, 120)
          \endcode
          \par Output
          Returns transformed vector with translation, rotation, and scaling applied
          \par Purpose
          Apply 2D transformation to points and vectors
        *************************************************************************/
        Vector2 operator*(const Vector2& vector) const
        {
            // Transform as homogeneous coordinate (x, y, 1)
            float32 x = m[0] * vector.x + m[3] * vector.y + m[6];
            float32 y = m[1] * vector.x + m[4] * vector.y + m[7];
            float32 w = m[2] * vector.x + m[5] * vector.y + m[8];

            // Normalise by w component (perspective division)
            if (std::abs(w) > FLOAT_EPSILON)
            {
                return Vector2(x / w, y / w);
            }
            return Vector2(x, y);
        }

        /*************************************************************************
          \brief
            Set matrix to identity (no transformation)
          \par Usage
          \code
            Matrix3x3 transform;
            transform.identity();           // Reset to no transformation
          \endcode
          \par Output
          Sets matrix to identity state
          \par Purpose
          Reset transformation to default state
        *************************************************************************/
        void identity()
        {
            m = { 1.0f, 0.0f, 0.0f,
                 0.0f, 1.0f, 0.0f,
                 0.0f, 0.0f, 1.0f };
        }

        /*************************************************************************
          \brief
            Extract translation component from matrix
          \return
            Vector2 representing translation offset
          \par Usage
          \code
            Matrix3x3 transform = Matrix3x3::translation(100, 50);
            Vector2 translation = transform.getTranslation();   // Result: (100, 50)
          \endcode
          \par Output
          Returns translation vector from matrix
          \par Purpose
          Extract position information from transformation matrix
        *************************************************************************/
        Vector2 getTranslation() const
        {
            return Vector2(m[6], m[7]);
        }

        /*************************************************************************
          \brief
            Extract scale component from matrix
          \return
            Vector2 representing scale
          \par Usage
          \code
            Matrix3x3 transform = Matrix3x3::scale(100, 50);
            Vector2 scale = transform.getScale();   // Result: (100, 50)
          \endcode
          \par Output
          Returns scale vector from matrix
          \par Purpose
          Extract scale information from transformation matrix
        *************************************************************************/
        Vector2 getScale() const
        {
            return Vector2(m[0], m[4]);
        }

        /*************************************************************************
          \brief
            Create translation matrix
          \param[in] x
            X offset
          \param[in] y
            Y offset
          \return
            Matrix representing translation transformation
          \par Usage
          \code
            Matrix3x3 moveRight = Matrix3x3::translation(100, 0);
            Matrix3x3 offset = Matrix3x3::translation(playerPos);
          \endcode
          \par Output
          Returns matrix that translates by specified offset
          \par Purpose
          Move objects to new positions
        *************************************************************************/
        static Matrix3x3 translation(float32 x, float32 y)
        {
            Matrix3x3 result;
            result.m[6] = x;  // Translation X
            result.m[7] = y;  // Translation Y
            return result;
        }

        /*************************************************************************
          \brief
            Create translation matrix from Vector2
          \param[in] offset
            Translation vector
          \return
            Matrix representing translation transformation
          \par Usage
          \code
            Vector2 playerPos(100, 200);
            Matrix3x3 transform = Matrix3x3::translation(playerPos);
          \endcode
          \par Output
          Returns matrix that translates by vector amount
          \par Purpose
          Convenient vector-based translation creation
        *************************************************************************/
        static Matrix3x3 translation(const Vector2& offset)
        {
            return translation(offset.x, offset.y);
        }

        /*************************************************************************
          \brief
            Create rotation matrix
          \param[in] radians
            Rotation angle in radians
          \return
            Matrix representing rotation transformation
          \par Usage
          \code
            Matrix3x3 rotate90 = Matrix3x3::rotation(HALF_PI);
            Matrix3x3 turn = Matrix3x3::rotation(playerAngle);
          \endcode
          \par Output
          Returns matrix that rotates by specified angle
          \par Purpose
          Rotate objects around origin
        *************************************************************************/
        static Matrix3x3 rotation(float32 radians)
        {
            float32 c = std::cos(radians);
            float32 s = std::sin(radians);

            Matrix3x3 result;
            result.m[0] = c;   result.m[3] = -s;  result.m[6] = 0.0f;
            result.m[1] = s;   result.m[4] = c;   result.m[7] = 0.0f;
            result.m[2] = 0.0f; result.m[5] = 0.0f; result.m[8] = 1.0f;
            return result;
        }

        /*************************************************************************
          \brief
            Create scaling matrix
          \param[in] scaleX
            X-axis scaling factor
          \param[in] scaleY
            Y-axis scaling factor
          \return
            Matrix representing scaling transformation
          \par Usage
          \code
            Matrix3x3 doubleSize = Matrix3x3::scale(2.0f, 2.0f);
            Matrix3x3 stretch = Matrix3x3::scale(1.5f, 0.8f);
          \endcode
          \par Output
          Returns matrix that scales by specified factors
          \par Purpose
          Resize objects with different X and Y scaling
        *************************************************************************/
        static Matrix3x3 scale(float32 scaleX, float32 scaleY)
        {
            Matrix3x3 result;
            result.m[0] = scaleX;
            result.m[4] = scaleY;
            return result;
        }

        /*************************************************************************
          \brief
            Create uniform scaling matrix
          \param[in] uniformScale
            Scaling factor for both axes
          \return
            Matrix representing uniform scaling transformation
          \par Usage
          \code
            Matrix3x3 halfSize = Matrix3x3::scale(0.5f);
            Matrix3x3 bigger = Matrix3x3::scale(1.5f);
          \endcode
          \par Output
          Returns matrix that scales equally in X and Y
          \par Purpose
          Resize objects maintaining aspect ratio
        *************************************************************************/
        static Matrix3x3 scale(float32 uniformScale)
        {
            return scale(uniformScale, uniformScale);
        }

        /*************************************************************************
          \brief
            Create combined Translation-Rotation-Scale transformation matrix
          \param[in] translation
            Translation offset
          \param[in] rotation
            Rotation angle in radians
          \param[in] scale
            Scaling factors
          \return
            Combined transformation matrix
          \par Usage
          \code
            Vector2 pos(100, 50);
            Matrix3x3 transform = Matrix3x3::TRS(pos, PI/4, Vector2(2, 2));
            Matrix3x3 sprite = Matrix3x3::TRS(spritePos, angle, size);
          \endcode
          \par Output
            Returns matrix that applies scale, then rotation, then translation
          \par Purpose
            Create complete object transformations in correct order
        *************************************************************************/
        static Matrix3x3 TRS(const Vector2& translation, float32 rotation, const Vector2& scale)
        {
            // Order: Translation * Rotation * Scale
            return Matrix3x3::translation(translation) *
                Matrix3x3::rotation(rotation) *
                Matrix3x3::scale(scale.x, scale.y);
        }

        /*************************************************************************
          \brief
            Create combined TRS transformation with uniform scaling
          \param[in] translation
            Translation offset
          \param[in] rotation
            Rotation angle in radians
          \param[in] uniformScale
            Uniform scaling factor
          \return
            Combined transformation matrix
          \par Usage
          \code
            Matrix3x3 transform = Matrix3x3::TRS(position, angle, 1.5f);
            Matrix3x3 entity = Matrix3x3::TRS(entityPos, entityRot, entityScale);
          \endcode
          \par Output
            Returns matrix with uniform scaling applied
          \par Purpose
            Convenient transformation creation with single scale value
        *************************************************************************/
        static Matrix3x3 TRS(const Vector2& translation, float32 rotation, float32 uniformScale)
        {
            return TRS(translation, rotation, Vector2(uniformScale, uniformScale));
        }
    };

    /*************************************************************************
      \brief
        Stream output for debugging
      \param[in] os
        Output stream
      \param[in] matrix
        Matrix to output
      \return
        Reference to output stream
      \par Usage
      \code
        std::cout << transform;             // Prints matrix elements
        debugLog << "Transform: " << matrix;
      \endcode
      \par Output
      Outputs matrix in readable 3x3 format
      \par Purpose
      Debug output and logging for transformation matrices
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& os, const Matrix3x3& matrix)
    {
        os << "Matrix3x3(\n";
        for (size_t row = 0; row < 3; ++row)
        {
            os << "  [";
            for (size_t col = 0; col < 3; ++col)
            {
                os << matrix(row, col);
                if (col < 2) os << ", ";
            }
            os << "]\n";
        }
        os << ")";
        return os;
    }
}