#include "LightGameEngine/Scene/GameWorld.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lge
{

namespace
{

Transform visualTransform(Registry const& registry, Entity const entity, Transform const& current, double const alpha)
{
	if (alpha >= 1.0)
	{
		return current;
	}
	PreviousTransformComponent const* const previous = registry.tryGet<PreviousTransformComponent>(entity);
	return previous != nullptr ? interpolate(previous->value, current, alpha) : current;
}

// Rigid composition. Non-uniform parent scale is applied to the child's offset but
// does not shear the child, which is adequate for rigid attached parts.
Transform compose(Transform const& parent, Transform const& local)
{
	Transform result;
	result.position = parent.position + parent.rotation * (parent.scale * local.position);
	result.rotation = glm::normalize(parent.rotation * local.rotation);
	result.scale = parent.scale * local.scale;
	return result;
}

constexpr int maxAttachmentDepth = 64;

}

Entity GameWorld::createEntity(std::string_view const name, Transform const& transform)
{
	Entity const entity = registry_.create();
	registry_.emplace<TransformComponent>(entity, transform);
	if (!name.empty())
	{
		registry_.emplace<NameComponent>(entity, std::string {name});
	}
	return entity;
}

void GameWorld::destroyEntity(Entity const entity)
{
	if (entity == activeCamera_)
	{
		activeCamera_ = nullEntity;
	}
	registry_.destroy(entity);
}

void GameWorld::queueDestroy(Entity const entity)
{
	if (registry_.isAlive(entity) && std::find(destroyQueue_.begin(), destroyQueue_.end(), entity) == destroyQueue_.end())
	{
		destroyQueue_.push_back(entity);
	}
}

void GameWorld::flushDestroyQueue()
{
	for (Entity const entity : destroyQueue_)
	{
		destroyEntity(entity);
	}
	destroyQueue_.clear();

	// Cascade to attached children, then to their children, until nothing is orphaned.
	std::vector<Entity> orphans;
	do
	{
		orphans.clear();
		registry_.each<AttachmentComponent>([&](Entity const entity, AttachmentComponent const& attachment)
			{
				if (!registry_.isAlive(attachment.parent))
				{
					orphans.push_back(entity);
				}
			});
		for (Entity const orphan : orphans)
		{
			destroyEntity(orphan);
		}
	} while (!orphans.empty());
}

Entity GameWorld::findByName(std::string_view const name) const
{
	Entity result = nullEntity;
	registry_.each<NameComponent>([&](Entity const entity, NameComponent const& component)
		{
			if (component.value == name)
			{
				result = entity;
			}
		});
	return result;
}

void GameWorld::setActiveCamera(Entity const camera)
{
	activeCamera_ = camera;
}

void GameWorld::teleport(Entity const entity, Transform const& transform)
{
	registry_.get<TransformComponent>(entity) = transform;
	if (PreviousTransformComponent* const previous = registry_.tryGet<PreviousTransformComponent>(entity))
	{
		previous->value = transform;
	}
}

void GameWorld::attach(Entity const child, Entity const parent, Transform const& local)
{
	if (!registry_.isAlive(child) || !registry_.isAlive(parent))
	{
		throw std::invalid_argument("GameWorld::attach: both entities must be alive");
	}
	// Walk up from the parent; reaching the child means the link would close a loop.
	Entity ancestor = parent;
	for (int depth = 0; ancestor.isValid(); ++depth)
	{
		if (ancestor == child || depth > maxAttachmentDepth)
		{
			throw std::invalid_argument("GameWorld::attach: attachment would create a cycle");
		}
		AttachmentComponent const* const next = registry_.tryGet<AttachmentComponent>(ancestor);
		ancestor = next != nullptr ? next->parent : nullEntity;
	}
	registry_.emplace<AttachmentComponent>(child, AttachmentComponent {parent, local});
	teleport(child, compose(registry_.get<TransformComponent>(parent), local));
}

void GameWorld::detach(Entity const child)
{
	registry_.remove<AttachmentComponent>(child);
}

void GameWorld::updateAttachments()
{
	++attachmentPass_;
	// Recursion resolves parents first so chains settle in one pass. The depth bound
	// stops runaway recursion if a cycle was introduced by editing components directly.
	auto const resolve = [this](auto const& self, Entity const entity, int const depth) -> void
	{
		AttachmentComponent const* const attachment = registry_.tryGet<AttachmentComponent>(entity);
		if (attachment == nullptr || depth > maxAttachmentDepth)
		{
			return;
		}
		if (entity.index >= attachmentStamp_.size())
		{
			attachmentStamp_.resize(static_cast<std::size_t>(entity.index) + 1, 0);
		}
		if (attachmentStamp_[entity.index] == attachmentPass_)
		{
			return;
		}
		attachmentStamp_[entity.index] = attachmentPass_;
		TransformComponent const* const parentTransform = registry_.tryGet<TransformComponent>(attachment->parent);
		if (parentTransform == nullptr || !registry_.isAlive(attachment->parent))
		{
			return;
		}
		self(self, attachment->parent, depth + 1);
		registry_.get<TransformComponent>(entity) = compose(*parentTransform, attachment->local);
	};
	registry_.each<AttachmentComponent>([&](Entity const entity, AttachmentComponent const&)
		{
			resolve(resolve, entity, 0);
		});
}

void GameWorld::storeTransformHistory()
{
	// Only entities whose visuals are interpolated need history.
	registry_.each<TransformComponent, MeshRendererComponent>(
		[this](Entity const entity, TransformComponent const& transform, MeshRendererComponent const&)
		{
			registry_.emplace<PreviousTransformComponent>(entity, transform);
		});
	registry_.each<TransformComponent, CameraComponent>(
		[this](Entity const entity, TransformComponent const& transform, CameraComponent const&)
		{
			registry_.emplace<PreviousTransformComponent>(entity, transform);
		});
}

void GameWorld::integrateVelocities(double const stepSeconds)
{
	registry_.each<TransformComponent, VelocityComponent>(
		[stepSeconds](Entity, TransformComponent& transform, VelocityComponent const& velocity)
		{
			transform.position += velocity.linear * stepSeconds;
			double const rate = glm::length(velocity.angular);
			if (rate > 1e-12)
			{
				glm::dquat const delta = glm::angleAxis(rate * stepSeconds, velocity.angular / rate);
				transform.rotation = glm::normalize(delta * transform.rotation);
			}
		});
}

void GameWorld::extractRenderScene(RenderScene& scene, double const alpha) const
{
	scene.clear();
	scene.environment = environment_;

	if (registry_.isAlive(activeCamera_))
	{
		TransformComponent const* const transform = registry_.tryGet<TransformComponent>(activeCamera_);
		CameraComponent const* const camera = registry_.tryGet<CameraComponent>(activeCamera_);
		if (transform != nullptr && camera != nullptr)
		{
			Transform const visual = visualTransform(registry_, activeCamera_, *transform, alpha);
			scene.camera.projection = camera->projection;
			scene.camera.position = visual.position;
			scene.camera.orientation = visual.rotation;
			scene.camera.verticalFieldOfViewDegrees = camera->verticalFieldOfViewDegrees;
			scene.camera.orthographicHeight = camera->orthographicHeight;
			scene.camera.nearPlane = camera->nearPlane;
			scene.camera.farPlane = camera->farPlane;
		}
	}

	scene.objects.reserve(registry_.count<MeshRendererComponent>());
	registry_.each<TransformComponent, MeshRendererComponent>(
		[&](Entity const entity, TransformComponent const& transform, MeshRendererComponent const& renderer)
		{
			if (!renderer.visible)
			{
				return;
			}
			Transform const visual = visualTransform(registry_, entity, transform, alpha);
			scene.objects.push_back(RenderObject {entity.id(), renderer.mesh, renderer.material,
				visual.position, visual.rotation, visual.scale});
		});
	// Iteration runs in reverse storage order; restore creation order so submissions
	// are stable and easy to reason about.
	std::reverse(scene.objects.begin(), scene.objects.end());

	registry_.each<TransformComponent, LightComponent>(
		[&](Entity, TransformComponent const& transform, LightComponent const& light)
		{
			RenderLight out;
			out.type = light.type;
			out.position = transform.position;
			out.direction = glm::vec3 {transform.forward()};
			out.color = light.color;
			out.intensity = light.intensity;
			out.range = light.range;
			out.innerConeRadians = light.innerConeRadians;
			out.outerConeRadians = light.outerConeRadians;
			out.castsShadow = light.castsShadow;
			scene.lights.push_back(out);
		});
	std::reverse(scene.lights.begin(), scene.lights.end());
}

void GameWorld::clear()
{
	registry_.clear();
	destroyQueue_.clear();
	activeCamera_ = nullEntity;
}

}
