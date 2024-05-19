#include "pch.h"
#include "Components.h"
#include "Renderer/Renderer.h"

namespace Vulture
{
	void ScriptComponent::InitializeScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnCreate();
		}
	}

	void ScriptComponent::UpdateScripts(double deltaTime)
	{
		for (auto& script : Scripts)
		{
			script->OnUpdate(deltaTime);
		}
	}

	void ScriptComponent::DestroyScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnDestroy();
		}
	}

	ScriptComponent::~ScriptComponent()
	{
		for (auto& script : Scripts)
		{
			delete script;
		}
	}

	SpriteComponent::SpriteComponent(const glm::vec2 atlasOffsets)
		: AtlasOffsets(atlasOffsets)
	{

	}
}