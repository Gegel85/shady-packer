#pragma once

#include <filesystem>

namespace ShadyUtil {
	class FileWatcher {
	public:
		static FileWatcher* create(const std::filesystem::path& path);
		static FileWatcher* getNextChange();
		enum Action {CREATED = 1, REMOVED = 2, MODIFIED = 3, RENAMED = 5};

		Action action;
		std::filesystem::path filename;

		~FileWatcher();
	private:
		inline FileWatcher(const std::filesystem::path& filename) : filename(filename) {}
	};
}