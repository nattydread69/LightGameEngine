#include "TestHarness.h"

#include "LightGameEngine/Backends/NullRenderer.h"
#include "LightGameEngine/Rendering/RenderResourceCache.h"
#include "LightGameEngine/Scene/GameWorld.h"

using namespace lge;
using namespace lge::test;

namespace
{

MeshData triangle()
{
	MeshData data;
	data.vertices = {MeshVertex {{0.0f, 0.0f, 0.0f}}, MeshVertex {{1.0f, 0.0f, 0.0f}}, MeshVertex {{0.0f, 1.0f, 0.0f}}};
	data.indices = {0, 1, 2};
	return data;
}

TestCase const nullRendererLifecycle {"NullRenderer: initialises without any graphics device", []
	{
		NullRenderer renderer;
		RenderStatistics const stats = renderer.statistics();
		checkEqual(stats.framesRendered, std::uint64_t {0}, "no frames yet");
		renderer.resize(800, 600);
		checkEqual(renderer.width(), 800u, "width");
		checkEqual(renderer.height(), 600u, "height");
		renderer.render(RenderScene {});
		checkEqual(renderer.statistics().framesRendered, std::uint64_t {1}, "empty frame rendered");
	}};

TestCase const resourceHandles {"NullRenderer: resource handles are generation-checked", []
	{
		NullRenderer renderer;
		MeshHandle const cube = renderer.createMesh(MeshDescription::primitive(PrimitiveShape::Cube));
		MeshHandle const custom = renderer.createMesh(MeshDescription::custom(triangle(), "triangle"));
		MaterialHandle const red = renderer.createMaterial(MaterialDescription {{1.0f, 0.0f, 0.0f, 1.0f}});
		check(cube.isValid() && custom.isValid() && red.isValid(), "valid handles");
		checkEqual(renderer.statistics().meshes, std::size_t {2}, "two meshes");
		renderer.destroyMesh(cube);
		check(renderer.mesh(cube) == nullptr, "destroyed mesh is gone");
		checkThrows<std::invalid_argument>([&]
			{
				renderer.destroyMesh(cube);
			}, "double destroy throws");
		MeshHandle const reused = renderer.createMesh(MeshDescription::primitive(PrimitiveShape::Sphere));
		checkEqual(reused.index, cube.index, "slot reused");
		check(reused != cube, "but the handle differs");
		check(renderer.mesh(cube) == nullptr, "stale handle still rejected");
	}};

TestCase const invalidMeshRejected {"NullRenderer: invalid mesh data is rejected", []
	{
		NullRenderer renderer;
		MeshData bad = triangle();
		bad.indices = {0, 1, 7};
		checkThrows<std::invalid_argument>([&]
			{
				(void)renderer.createMesh(MeshDescription::custom(bad));
			}, "index out of range");
		bad.indices = {0, 1};
		checkThrows<std::invalid_argument>([&]
			{
				(void)renderer.createMesh(MeshDescription::custom(bad));
			}, "incomplete triangle");
		checkThrows<std::invalid_argument>([&]
			{
				(void)renderer.createMesh(MeshDescription::custom(MeshData {}));
			}, "empty");
	}};

TestCase const sceneSubmission {"NullRenderer: counts submitted, drawn and rejected objects", []
	{
		NullRenderer renderer;
		GameWorld world;
		MeshHandle const mesh = renderer.createMesh(MeshDescription::primitive(PrimitiveShape::Cube));
		MaterialHandle const material = renderer.createMaterial(MaterialDescription {});
		for (int i = 0; i < 3; ++i)
		{
			Entity const e = world.createEntity();
			world.add<MeshRendererComponent>(e, MeshRendererComponent {mesh, material, true});
		}
		Entity const broken = world.createEntity();
		world.add<MeshRendererComponent>(broken, MeshRendererComponent {MeshHandle {}, material, true});
		world.add<LightComponent>(world.createEntity());

		RenderScene scene;
		world.extractRenderScene(scene, 1.0);
		scene.texts.push_back(ScreenText {1, "Score: 0"});
		renderer.render(scene);
		RenderStatistics const stats = renderer.statistics();
		checkEqual(stats.submittedObjects, std::size_t {4}, "submitted");
		checkEqual(stats.drawnObjects, std::size_t {3}, "drawn");
		checkEqual(stats.rejectedObjects, std::size_t {1}, "invalid handle rejected, not fatal");
		checkEqual(stats.lights, std::size_t {1}, "lights");
		checkEqual(stats.texts, std::size_t {1}, "texts");

		renderer.destroyMaterial(material);
		renderer.render(scene);
		checkEqual(renderer.statistics().drawnObjects, std::size_t {0}, "destroyed material invalidates objects safely");
	}};

TestCase const resourceCache {"RenderResourceCache: creates each resource once and releases on destruction", []
	{
		NullRenderer renderer;
		{
			RenderResourceCache cache {renderer};
			MeshHandle const a = cache.primitive(PrimitiveShape::Cube);
			MeshHandle const b = cache.primitive(PrimitiveShape::Cube);
			check(a == b, "same handle for same key");
			int created = 0;
			auto const factory = [&created]()
			{
				++created;
				return MeshDescription::custom(triangle());
			};
			(void)cache.mesh("meshes/triangle", factory);
			(void)cache.mesh("meshes/triangle", factory);
			checkEqual(created, 1, "factory runs once");
			MaterialHandle const m1 = cache.material("materials/red", MaterialDescription {{1, 0, 0, 1}});
			MaterialHandle const m2 = cache.material("materials/red", MaterialDescription {{0, 1, 0, 1}});
			check(m1 == m2, "material cached by key");
			checkEqual(renderer.statistics().meshes, std::size_t {2}, "two meshes in renderer");
			cache.releaseMesh("meshes/triangle");
			check(!cache.containsMesh("meshes/triangle"), "released");
			checkEqual(renderer.statistics().meshes, std::size_t {1}, "renderer resource destroyed");
		}
		checkEqual(renderer.statistics().meshes, std::size_t {0}, "cache destructor releases meshes");
		checkEqual(renderer.statistics().materials, std::size_t {0}, "and materials");
	}};

TestCase const perspectiveProjection {"RenderCamera: perspective matrix", []
	{
		RenderCamera camera;
		camera.verticalFieldOfViewDegrees = 90.0;
		glm::dmat4 const projection = camera.projectionMatrix(1.0);
		checkNear(projection[1][1], 1.0, 1e-12, "cot(45 deg) = 1");
		glm::dvec4 const clip = projection * camera.viewMatrix() * glm::dvec4 {0.0, 0.0, -10.0, 1.0};
		check(clip.w > 0.0, "point in front of the camera");
	}};

}
