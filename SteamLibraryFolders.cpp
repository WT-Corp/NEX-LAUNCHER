#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#pragma comment(lib, "Advapi32.lib")

std::vector<std::string> ParseSteamLibraryFolders(const std::string& vdfPath) {
	std::vector<std::string> libraries;
	std::ifstream file(vdfPath);
	std::string line;

	while (std::getline(file, line)) {
		const auto keyStart = line.find('\"');
		if (keyStart == std::string::npos) {
			continue;
		}

		const auto keyEnd = line.find('\"', keyStart + 1);
		if (keyEnd == std::string::npos ||
			line.substr(keyStart + 1, keyEnd - keyStart - 1) != "path") {
			continue;
		}

		const auto valueStart = line.find('\"', keyEnd + 1);
		if (valueStart == std::string::npos) {
			continue;
		}

		const auto valueEnd = line.find('\"', valueStart + 1);
		if (valueEnd == std::string::npos) {
			continue;
		}

		const std::string value =
			line.substr(valueStart + 1, valueEnd - valueStart - 1);

		std::string path;
		path.reserve(value.size());

		bool previousWasSeparator = false;
		for (const char character : value) {
			const bool isSeparator = character == '\\' || character == '/';
			if (!isSeparator || !previousWasSeparator) {
				path.push_back(isSeparator ? '/' : character);
			}
			previousWasSeparator = isSeparator;
		}

		std::error_code error;
		if (std::filesystem::exists(std::filesystem::u8path(path), error) &&
			!error) {
			libraries.push_back(std::move(path));
		}
	}

	return libraries;
}

int main() {
	HKEY steamKey = nullptr;
	if (RegOpenKeyExW(
			HKEY_CURRENT_USER,
			L"Software\\Valve\\Steam",
			0,
			KEY_QUERY_VALUE,
			&steamKey) != ERROR_SUCCESS) {
		std::cerr << "Could not open the Steam registry key.\n";
		return 1;
	}

	DWORD valueType = 0;
	DWORD valueSize = 0;
	const LSTATUS sizeResult = RegQueryValueExW(
		steamKey, L"SteamPath", nullptr, &valueType, nullptr, &valueSize);

	if (sizeResult != ERROR_SUCCESS ||
		(valueType != REG_SZ && valueType != REG_EXPAND_SZ)) {
		RegCloseKey(steamKey);
		std::cerr << "Could not read the Steam install path from the registry.\n";
		return 1;
	}

	std::vector<wchar_t> steamPath(valueSize / sizeof(wchar_t) + 1, L'\0');
	const LSTATUS pathResult = RegQueryValueExW(
		steamKey,
		L"SteamPath",
		nullptr,
		&valueType,
		reinterpret_cast<LPBYTE>(steamPath.data()),
		&valueSize);

	RegCloseKey(steamKey);

	if (pathResult != ERROR_SUCCESS) {
		std::cerr << "Could not read the Steam install path from the registry.\n";
		return 1;
	}

	const std::filesystem::path vdfPath =
		std::filesystem::path(steamPath.data()) /
		L"steamapps" /
		L"libraryfolders.vdf";

	const auto libraries = ParseSteamLibraryFolders(vdfPath.u8string());
	for (const auto& library : libraries) {
		std::cout << library << '\n';
	}

	return 0;
}
