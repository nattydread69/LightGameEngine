#include "TestHarness.h"

#include "LightGameEngine/Scene/GameWorld.h"
#include "LightGameEngine/Scene/PhysicsSystem2D.h"

#include <glm/gtc/constants.hpp>

using namespace lge;
using namespace lge::test;

namespace
{

MeshHandle const someMesh {0, 1};
MaterialHandle const someMaterial {0, 1};

TestCase const namedEntities {"GameWorld: entities have transforms and names", []
	{
		GameWorld world;
		Transform start;
		start.position = {1.0, 2.0, 3.0};
		Entity const player = world.createEntity("Player", start);
		Entity const anonymous = world.createEntity();
		check(world.has<TransformComponent>(player), "transform added");
		checkNear(world.get<TransformComponent>(player).position.z, 3.0, 0.0, "initial transform");
		check(world.findByName("Player") == player, "find by name");
		check(!world.findByName("Nobody").isValid(), "missing name");
		check(!world.has<NameComponent>(anonymous), "no name component when unnamed");
		checkEqual(world.entityCount(), std::size_t {2}, "two entities");
	}};

TestCase const deferredDestruction {"GameWorld: queued destruction happens at flush", []
	{
		GameWorld world;
		Entity const e = world.createEntity("Doomed");
		world.queueDestroy(e);
		world.queueDestroy(e);
		check(world.isAlive(e), "still alive before flush");
		world.flushDestroyQueue();
		check(!world.isAlive(e), "destroyed after flush");
		world.flushDestroyQueue();
	}};

TestCase const velocityIntegration {"GameWorld: velocities integrate position and rotation", []
	{
		GameWorld world;
		Entity const e = world.createEntity();
		world.add<VelocityComponent>(e, VelocityComponent {{2.0, 0.0, -1.0}, {0.0, glm::pi<double>(), 0.0}});
		for (int i = 0; i < 10; ++i)
		{
			world.integrateVelocities(0.05);
		}
		TransformComponent const& transform = world.get<TransformComponent>(e);
		checkNear(transform.position.x, 1.0, 1e-12, "x after 0.5 s");
		checkNear(transform.position.z, -0.5, 1e-12, "z after 0.5 s");
		checkNear(transform.forward().x, -1.0, 1e-9, "half a turn per second for 0.5 s = 90 degrees");
	}};

TestCase const cameraConfiguration {"GameWorld: active camera is extracted", []
	{
		GameWorld world;
		Transform cameraTransform;
		cameraTransform.position = {0.0, 30.0, 0.0};
		cameraTransform.rotation = lookRotation({0.0, -1.0, 0.0}, {0.0, 0.0, -1.0});
		Entity const camera = world.createEntity("Camera", cameraTransform);
		CameraComponent settings;
		settings.projection = ProjectionType::Orthographic;
		settings.orthographicHeight = 24.0;
		world.add<CameraComponent>(camera, settings);
		world.setActiveCamera(camera);

		RenderScene scene;
		world.extractRenderScene(scene, 1.0);
		check(scene.camera.projection == ProjectionType::Orthographic, "projection");
		checkNear(scene.camera.orthographicHeight, 24.0, 0.0, "ortho height");
		checkNear(scene.camera.position.y, 30.0, 0.0, "position");
		checkNear(scene.camera.forward().y, -1.0, 1e-9, "looks down");

		glm::dmat4 const projection = scene.camera.projectionMatrix(2.0);
		checkNear(projection[1][1], 2.0 / 24.0, 1e-12, "ortho vertical scale");
		checkNear(projection[0][0], 2.0 / 48.0, 1e-12, "ortho horizontal scale uses aspect");

		world.destroyEntity(camera);
		check(!world.activeCamera().isValid(), "destroying the camera clears it");
	}};

TestCase const renderExtraction {"GameWorld: render extraction includes only visible renderables", []
	{
		GameWorld world;
		for (int i = 0; i < 5; ++i)
		{
			Entity const e = world.createEntity();
			world.add<MeshRendererComponent>(e, MeshRendererComponent {someMesh, someMaterial, i != 2});
		}
		world.createEntity("NoRenderer");
		Entity const light = world.createEntity("Sun");
		world.add<LightComponent>(light);

		RenderScene scene;
		world.extractRenderScene(scene, 1.0);
		checkEqual(scene.objects.size(), std::size_t {4}, "hidden and non-rendered entities skipped");
		checkEqual(scene.lights.size(), std::size_t {1}, "one light");
		checkNear(scene.lights.front().direction.y, 0.0, 1e-6, "identity light faces -Z");
		check(scene.objects.front().id < scene.objects.back().id, "stable creation order");

		world.extractRenderScene(scene, 1.0);
		checkEqual(scene.objects.size(), std::size_t {4}, "re-extraction replaces rather than appends");
	}};

TestCase const interpolatedExtraction {"GameWorld: extraction interpolates between ticks", []
	{
		GameWorld world;
		Entity const e = world.createEntity();
		world.add<MeshRendererComponent>(e, MeshRendererComponent {someMesh, someMaterial, true});
		world.add<VelocityComponent>(e, VelocityComponent {{10.0, 0.0, 0.0}, {}});
		world.storeTransformHistory();
		world.integrateVelocities(0.1);

		RenderScene scene;
		world.extractRenderScene(scene, 0.25);
		checkNear(scene.objects.front().position.x, 0.25, 1e-12, "a quarter of the way from 0 to 1");
		world.extractRenderScene(scene, 1.0);
		checkNear(scene.objects.front().position.x, 1.0, 1e-12, "alpha 1 is the current state");

		Transform jump;
		jump.position = {100.0, 0.0, 0.0};
		world.teleport(e, jump);
		world.extractRenderScene(scene, 0.5);
		checkNear(scene.objects.front().position.x, 100.0, 1e-12, "teleport suppresses interpolation");
	}};

TestCase const physicsPushesOutOfWalls {"PhysicsSystem2D: dynamic colliders are pushed out of static walls", []
	{
		GameWorld world;
		Entity const wall = world.createEntity("Wall");
		world.add<Collider2DComponent>(wall, Collider2DComponent {ColliderShape2D::box({1.0, 1.0}), 1, allCollisionLayers, true, false});
		Transform robotStart;
		robotStart.position = {1.3, 0.7, 0.0};
		Entity const robot = world.createEntity("Robot", robotStart);
		world.add<Collider2DComponent>(robot, Collider2DComponent {ColliderShape2D::circle(0.5), 2, allCollisionLayers, false, false});

		PhysicsSystem2D physics;
		physics.step(world);
		checkNear(world.get<TransformComponent>(robot).position.x, 1.5, 1e-9, "pushed out to the wall surface");
		checkNear(world.get<TransformComponent>(robot).position.y, 0.7, 0.0, "height untouched");
		checkNear(world.get<TransformComponent>(wall).position.x, 0.0, 0.0, "static wall unmoved");
		checkEqual(physics.events().size(), std::size_t {1}, "one collision event");
		check(!physics.events().front().trigger, "solid contact");

		physics.step(world);
		checkEqual(physics.events().size(), std::size_t {0}, "resolved: no contact on the next step");
	}};

TestCase const physicsDynamicPairs {"PhysicsSystem2D: dynamic pairs share the correction; triggers report only", []
	{
		GameWorld world;
		Transform left;
		left.position = {-0.4, 0.0, 0.0};
		Transform right;
		right.position = {0.4, 0.0, 0.0};
		Entity const a = world.createEntity("A", left);
		Entity const b = world.createEntity("B", right);
		world.add<Collider2DComponent>(a, Collider2DComponent {ColliderShape2D::circle(0.5)});
		world.add<Collider2DComponent>(b, Collider2DComponent {ColliderShape2D::circle(0.5)});
		Entity const pickup = world.createEntity("Pickup");
		Collider2DComponent trigger {ColliderShape2D::circle(0.5)};
		trigger.isTrigger = true;
		world.add<Collider2DComponent>(pickup, trigger);

		PhysicsSystem2D physics;
		physics.step(world);
		checkNear(world.get<TransformComponent>(a).position.x, -0.5, 1e-9, "A moved half");
		checkNear(world.get<TransformComponent>(b).position.x, 0.5, 1e-9, "B moved half");
		checkNear(world.get<TransformComponent>(pickup).position.x, 0.0, 0.0, "trigger not moved");
		int triggers = 0;
		for (CollisionEvent const& event : physics.events())
		{
			triggers += event.trigger ? 1 : 0;
		}
		checkEqual(triggers, 2, "trigger overlaps both");
	}};

TestCase const physicsLineOfSight {"PhysicsSystem2D: raycasts and line of sight against walls", []
	{
		GameWorld world;
		Transform wallTransform;
		wallTransform.position = {0.0, 0.0, 0.0};
		Entity const wall = world.createEntity("Wall", wallTransform);
		world.add<Collider2DComponent>(wall, Collider2DComponent {ColliderShape2D::box({0.5, 3.0}), 1, allCollisionLayers, true, false});
		PhysicsSystem2D physics;
		physics.step(world);
		check(!physics.hasLineOfSight({-5.0, 0.0}, {5.0, 0.0}, 1), "wall blocks");
		check(physics.hasLineOfSight({-5.0, 5.0}, {5.0, 5.0}, 1), "clear above the wall");
		check(physics.hasLineOfSight({-5.0, 0.0}, {5.0, 0.0}, 2), "other layers do not block");
		std::optional<EntityRayHit> const hit = physics.raycast(Ray2 {{-5.0, 0.0}, {1.0, 0.0}}, 20.0);
		check(hit.has_value() && hit->entity == wall, "ray reports the entity");
		checkNear(hit->hit.distance, 4.5, 1e-12, "distance");
	}};

TestCase const attachmentsFollowParents {"GameWorld: attachments follow parents through chains", []
	{
		GameWorld world;
		Entity const body = world.createEntity("Body");
		Entity const head = world.createEntity("Head");
		Entity const antenna = world.createEntity("Antenna");
		Transform headOffset;
		headOffset.position = {0.0, 1.0, 0.0};
		Transform antennaOffset;
		antennaOffset.position = {0.0, 0.0, -0.5};
		// Attach in child-first order to prove resolution does not depend on it.
		world.attach(antenna, head, antennaOffset);
		world.attach(head, body, headOffset);

		TransformComponent& bodyTransform = world.get<TransformComponent>(body);
		bodyTransform.position = {5.0, 0.0, 0.0};
		bodyTransform.rotation = yawRotation(glm::half_pi<double>());
		world.updateAttachments();

		glm::dvec3 const headPosition = world.get<TransformComponent>(head).position;
		glm::dvec3 const antennaPosition = world.get<TransformComponent>(antenna).position;
		checkNear(headPosition.x, 5.0, 1e-12, "head above body x");
		checkNear(headPosition.y, 1.0, 1e-12, "head above body y");
		// Antenna is 0.5 forward of the head; the body faces -X after a +90 degree yaw.
		checkNear(antennaPosition.x, 4.5, 1e-12, "rotation propagates down the chain");
		checkNear(antennaPosition.y, 1.0, 1e-12, "antenna height");
	}};

TestCase const attachmentCyclesRejected {"GameWorld: attachment cycles are rejected", []
	{
		GameWorld world;
		Entity const a = world.createEntity();
		Entity const b = world.createEntity();
		world.attach(b, a);
		checkThrows<std::invalid_argument>([&]
			{
				world.attach(a, b);
			}, "two-entity cycle");
		checkThrows<std::invalid_argument>([&]
			{
				world.attach(a, a);
			}, "self attachment");
		world.detach(b);
		world.attach(a, b);
		check(world.has<AttachmentComponent>(a), "allowed after detaching");
	}};

TestCase const attachmentsDestroyedWithParent {"GameWorld: destroying a parent destroys attached children", []
	{
		GameWorld world;
		Entity const parent = world.createEntity();
		Entity const child = world.createEntity();
		Entity const grandchild = world.createEntity();
		Entity const bystander = world.createEntity();
		world.attach(child, parent);
		world.attach(grandchild, child);
		world.queueDestroy(parent);
		world.flushDestroyQueue();
		check(!world.isAlive(child), "child destroyed");
		check(!world.isAlive(grandchild), "grandchild destroyed");
		check(world.isAlive(bystander), "unrelated entity untouched");
	}};

}
