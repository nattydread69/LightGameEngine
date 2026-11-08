#include "LightGameEngine/Rendering/RenderResources.h"
#include "LightGameEngine/Rendering/RenderScene.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>
#include <string>

namespace lge
{

glm::dvec3 RenderCamera::forward() const
{
	return orientation * glm::dvec3 {0.0, 0.0, -1.0};
}

glm::dvec3 RenderCamera::up() const
{
	return orientation * glm::dvec3 {0.0, 1.0, 0.0};
}

glm::dmat4 RenderCamera::viewMatrix() const
{
	return glm::lookAt(position, position + forward(), up());
}

glm::dmat4 RenderCamera::projectionMatrix(double const aspectRatio) const
{
	if (projection == ProjectionType::Orthographic)
	{
		double const halfHeight = orthographicHeight * 0.5;
		double const halfWidth = halfHeight * aspectRatio;
		return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
	}
	return glm::perspective(glm::radians(verticalFieldOfViewDegrees), aspectRatio, nearPlane, farPlane);
}

void validateMeshData(MeshData const& data)
{
	if (data.vertices.empty() || data.indices.empty())
	{
		throw std::invalid_argument("MeshData: vertices and indices must not be empty");
	}
	if (data.indices.size() % 3 != 0)
	{
		throw std::invalid_argument("MeshData: index count must be a multiple of 3");
	}
	for (std::uint32_t const index : data.indices)
	{
		if (index >= data.vertices.size())
		{
			throw std::invalid_argument("MeshData: index " + std::to_string(index) + " is out of range");
		}
	}
	auto const finite = [](auto const& v)
	{
		for (int i = 0; i < v.length(); ++i)
		{
			if (!std::isfinite(v[i]))
			{
				return false;
			}
		}
		return true;
	};
	for (MeshVertex const& vertex : data.vertices)
	{
		if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.color))
		{
			throw std::invalid_argument("MeshData: vertex attributes must be finite");
		}
	}
}

}
