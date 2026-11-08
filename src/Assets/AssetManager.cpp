// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Light Game Engine
// Copyright (C) 2026 and onwards Dr. Nathanael John Inkson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "LightGameEngine/Assets/AssetManager.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>

namespace lge
{

namespace
{

std::optional<std::filesystem::path> executableDirectory()
{
#if defined(__linux__)
	std::error_code error;
	std::filesystem::path const exe = std::filesystem::read_symlink("/proc/self/exe", error);
	if (!error)
	{
		return exe.parent_path();
	}
#endif
	// Other platforms fall back to the working-directory search.
	return std::nullopt;
}

std::optional<std::filesystem::path> searchUpwards(std::filesystem::path start, std::string_view const marker)
{
	std::error_code error;
	for (std::filesystem::path dir = start; !dir.empty(); dir = dir.parent_path())
	{
		std::filesystem::path const candidate = dir / "assets";
		if (std::filesystem::exists(candidate / std::filesystem::path {std::string {marker}}, error))
		{
			return candidate;
		}
		if (dir == dir.parent_path())
		{
			break;
		}
	}
	return std::nullopt;
}

}

AssetManager::AssetManager(std::filesystem::path root)
{
	std::error_code error;
	if (!std::filesystem::is_directory(root, error))
	{
		throw AssetError("Asset root is not a directory: " + root.string());
	}
	root_ = std::filesystem::weakly_canonical(root, error);
	if (error)
	{
		root_ = std::move(root);
	}
}

AssetManager AssetManager::locate(std::string_view const markerRelativePath)
{
	if (char const* const fromEnvironment = std::getenv("LGE_ASSET_ROOT"))
	{
		return AssetManager {std::filesystem::path {fromEnvironment}};
	}
	if (std::optional<std::filesystem::path> const exeDir = executableDirectory())
	{
		if (std::optional<std::filesystem::path> const found = searchUpwards(*exeDir, markerRelativePath))
		{
			return AssetManager {*found};
		}
	}
	std::error_code error;
	if (std::optional<std::filesystem::path> const found = searchUpwards(std::filesystem::current_path(error), markerRelativePath))
	{
		return AssetManager {*found};
	}
	throw AssetError("Could not locate an asset root containing '" + std::string {markerRelativePath} +
		"'. Set LGE_ASSET_ROOT to the assets directory.");
}

std::filesystem::path AssetManager::resolve(std::string_view const relativePath, bool const mustExist) const
{
	if (relativePath.empty())
	{
		throw AssetError("Asset path is empty");
	}
	std::filesystem::path const relative {std::string {relativePath}};
	if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
	{
		throw AssetError("Asset paths must be relative to the asset root: " + std::string {relativePath});
	}
	for (std::filesystem::path const& part : relative)
	{
		if (part == "..")
		{
			throw AssetError("Asset paths may not leave the asset root: " + std::string {relativePath});
		}
	}
	std::filesystem::path const full = (root_ / relative).lexically_normal();
	std::error_code error;
	if (mustExist && !std::filesystem::exists(full, error))
	{
		throw AssetError("Asset not found: " + std::string {relativePath} + " (looked in " + root_.string() + ")");
	}
	return full;
}

bool AssetManager::exists(std::string_view const relativePath) const
{
	try
	{
		return std::filesystem::exists(resolve(relativePath, false));
	}
	catch (AssetError const&)
	{
		return false;
	}
}

std::string AssetManager::readText(std::string_view const relativePath) const
{
	std::filesystem::path const path = resolve(relativePath);
	std::ifstream stream {path, std::ios::binary};
	if (!stream)
	{
		throw AssetError("Could not open asset: " + path.string());
	}
	return std::string {std::istreambuf_iterator<char> {stream}, std::istreambuf_iterator<char> {}};
}

std::vector<std::byte> AssetManager::readBinary(std::string_view const relativePath) const
{
	std::string const text = readText(relativePath);
	std::vector<std::byte> bytes(text.size());
	for (std::size_t i = 0; i < text.size(); ++i)
	{
		bytes[i] = static_cast<std::byte>(text[i]);
	}
	return bytes;
}

}
