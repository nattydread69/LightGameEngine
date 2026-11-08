#include "TestHarness.h"

#include "LightGameEngine/Input/ActionMap.h"
#include "LightGameEngine/Input/InputState.h"

using namespace lge;
using namespace lge::test;

namespace
{

TestCase const keyTransitions {"InputState: press and release edges", []
	{
		InputState input;
		input.setKeyDown(Key::W, true);
		check(input.isDown(Key::W), "level is immediate");
		check(!input.wasPressed(Key::W), "edge not visible before a tick");
		input.beginTick();
		check(input.wasPressed(Key::W), "pressed in first tick");
		input.beginTick();
		check(!input.wasPressed(Key::W), "not repeated in the second tick");
		check(input.isDown(Key::W), "still held");
		input.setKeyDown(Key::W, false);
		input.beginTick();
		check(input.wasReleased(Key::W), "released edge");
		check(!input.isDown(Key::W), "up");
	}};

TestCase const repeatedLevelsIgnored {"InputState: polling an unchanged level creates no edges", []
	{
		InputState input;
		input.setKeyDown(Key::A, true);
		input.beginTick();
		input.setKeyDown(Key::A, true);
		input.setKeyDown(Key::A, true);
		input.beginTick();
		check(!input.wasPressed(Key::A), "no new press");
	}};

TestCase const pressAcrossTicks {"InputState: a press is consumed by exactly one of several ticks in a frame", []
	{
		InputState input;
		input.setKeyDown(Key::Space, true);
		int pressedTicks = 0;
		for (int tick = 0; tick < 4; ++tick)
		{
			input.beginTick();
			pressedTicks += input.wasPressed(Key::Space) ? 1 : 0;
		}
		checkEqual(pressedTicks, 1, "one tick sees the press");
	}};

TestCase const pressSurvivesZeroTickFrames {"InputState: a press survives frames that run no ticks", []
	{
		InputState input;
		input.setKeyDown(Key::Space, true);
		// Two rendered frames with no tick: nothing calls beginTick.
		input.setKeyDown(Key::Space, true);
		input.beginTick();
		check(input.wasPressed(Key::Space), "press delivered to the next tick");
	}};

TestCase const tapBetweenTicks {"InputState: a tap between two ticks is not lost", []
	{
		InputState input;
		input.setKeyDown(Key::E, true);
		input.setKeyDown(Key::E, false);
		input.beginTick();
		check(input.wasPressed(Key::E), "pressed seen");
		check(input.wasReleased(Key::E), "released seen");
		check(!input.isDown(Key::E), "not held");
	}};

TestCase const mouseState {"InputState: mouse buttons, position and delta", []
	{
		InputState input;
		input.setMousePosition({100.0, 50.0});
		input.setMousePosition({110.0, 45.0});
		input.setMouseButtonDown(MouseButton::Left, true);
		input.beginTick();
		check(input.wasPressed(MouseButton::Left), "button pressed");
		checkNear(input.mouseDelta().x, 10.0, 0.0, "first report sets position only; delta x");
		checkNear(input.mouseDelta().y, -5.0, 0.0, "delta y");
		input.beginTick();
		checkNear(input.mouseDelta().x, 0.0, 0.0, "delta consumed");
		checkNear(input.mousePosition().x, 110.0, 0.0, "position kept");
	}};

TestCase const actionMapping {"ActionMap: actions map to multiple bindings", []
	{
		ActionMap actions;
		ActionId const forward = actions.bind("MoveForward", Key::W);
		actions.bind("MoveForward", Key::Up);
		ActionId const fire = actions.bind("Fire", Key::Space);
		actions.bind("Fire", MouseButton::Left);
		check(actions.find("MoveForward") == forward, "lookup by name");
		check(!actions.find("Jump").has_value(), "unknown action");
		checkEqual(actions.bindings(forward).size(), std::size_t {2}, "two bindings");
		actions.bind(forward, InputBinding::fromKey(Key::W));
		checkEqual(actions.bindings(forward).size(), std::size_t {2}, "duplicate ignored");

		InputState input;
		input.setKeyDown(Key::Up, true);
		input.setMouseButtonDown(MouseButton::Left, true);
		input.beginTick();
		check(actions.isActive(forward, input), "alternate key activates");
		check(actions.wasTriggered(fire, input), "mouse binding triggers");
		input.beginTick();
		check(!actions.wasTriggered(fire, input), "trigger is one tick only");
		check(actions.isActive(fire, input), "but still active while held");
	}};

TestCase const actionAxis {"ActionMap: opposing actions form an axis", []
	{
		ActionMap actions;
		ActionId const left = actions.bind("Left", Key::A);
		ActionId const right = actions.bind("Right", Key::D);
		InputState input;
		checkNear(actions.axis(left, right, input), 0.0, 0.0, "idle");
		input.setKeyDown(Key::D, true);
		checkNear(actions.axis(left, right, input), 1.0, 0.0, "right");
		input.setKeyDown(Key::A, true);
		checkNear(actions.axis(left, right, input), 0.0, 0.0, "both cancel");
	}};

TestCase const rebinding {"ActionMap: bindings can be replaced without touching game code", []
	{
		ActionMap actions;
		ActionId const fire = actions.bind("Fire", Key::Space);
		actions.clearBindings(fire);
		actions.bind(fire, InputBinding::fromKey(Key::F));
		InputState input;
		input.setKeyDown(Key::Space, true);
		check(!actions.isActive(fire, input), "old key no longer fires");
		input.setKeyDown(Key::F, true);
		check(actions.isActive(fire, input), "new key fires");
		checkThrows<std::out_of_range>([&]
			{
				(void)actions.isActive(ActionId {99}, input);
			}, "invalid id throws");
		checkThrows<std::invalid_argument>([&]
			{
				actions.define("");
			}, "empty name throws");
	}};

TestCase const keyNames {"Input: key names", []
	{
		check(keyName(Key::W) == "W", "W");
		check(keyName(Key::Space) == "Space", "Space");
		check(keyName(Key::GraveAccent) == "GraveAccent", "last key");
	}};

}
