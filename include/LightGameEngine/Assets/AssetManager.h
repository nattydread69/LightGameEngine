#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lge
{

class AssetError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

// Resolves relative asset paths ("textures/floor.png") against a single asset root
// directory and loads source files from it. Asset paths are always relative, use
// forward slashes, and may not escape the root, so games never embed absolute
// filesystem paths. This class handles source assets only; GPU resources are created
// by the renderer and cached by RenderResourceCache.
class AssetManager
{
public:
	explicit AssetManager(std::filesystem::path root);

	// Searches for an asset root containing the given relative marker file, checking
	// in order: the LGE_ASSET_ROOT environment variable, the executable's directory
	// and its parents, then the working directory and its parents (each at
	// "<dir>/assets"). Throws AssetError if none is found.
	[[nodiscard]] static AssetManager locate(std::string_view markerRelativePath);

	[[nodiscard]] std::filesystem::path const& root() const noexcept
	{
		return root_;
	}

	// Returns the absolute path of an asset. Throws AssetError for absolute paths,
	// paths containing "..", or (when mustExist) assets that do not exist.
	[[nodiscard]] std::filesystem::path resolve(std::string_view relativePath, bool mustExist = true) const;
	[[nodiscard]] bool exists(std::string_view relativePath) const;

	[[nodiscard]] std::string readText(std::string_view relativePath) const;
	[[nodiscard]] std::vector<std::byte> readBinary(std::string_view relativePath) const;

private:
	std::filesystem::path root_;
};

}
