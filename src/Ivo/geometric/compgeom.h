/*
    Ivo - a free software for unfolding 3D models and papercrafting
    Copyright (C) 2015-2018 Oleksii Sierov (seriousalexej@gmail.com)
	
    Ivo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Ivo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Ivo.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef IVO_COMP_GEOM_H
#define IVO_COMP_GEOM_H
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat2x2.hpp>
#include <glm/mat3x3.hpp>

namespace glm
{
float   sign(float f);
int     crossSign(const ivec2& v1, const ivec2& v2);
float   cross(const vec2& v1, const vec2& v2);
float   angleFromTo(const vec2& v1, const vec2& v2);
float   angleBetween(const vec2& v1, const vec2& v2);
float   angleBetween(const vec3& v1, const vec3& v2);
bool    rightTurn(const vec2& v1, const vec2& v2);
bool    leftTurn(const vec2& v1, const vec2& v2);
mat2    rotation(float angleFromTo);
mat3    transformation(const vec2& translation, float rotation);
float   hFOVtovFOV(float hFOV, float aspect);
}

#endif
