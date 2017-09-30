#include "fileentry.hpp"
#include "package.hpp"
#include "resource/readerwriter.hpp"
#include <boost/filesystem.hpp>

ShadyCore::FilePackageEntry::FilePackageEntry(const char* name, const char* filename, bool deleteOnDestroy)
	: BasePackageEntry(name, boost::filesystem::file_size(filename)), filename(filename), deleteOnDestroy(deleteOnDestroy) {}

ShadyCore::FilePackageEntry::~FilePackageEntry() {
	if (deleteOnDestroy) boost::filesystem::remove(filename);
}

//-------------------------------------------------------------

void ShadyCore::Package::appendDirPackage(const char* basePath) {
	for (boost::filesystem::recursive_directory_iterator iter(basePath), end; iter != end; ++iter) {
		if (boost::filesystem::is_regular_file(iter->path())) {
			const char* filename = allocateString(iter->path().string().c_str());
			const char* name = allocateString(boost::filesystem::relative(filename, basePath).generic_string().c_str());
			addOrReplace(new FilePackageEntry(name, filename));
		}
	}
}

void ShadyCore::Package::appendFile(const char* name, const char* filename, bool deleteOnDestroy) {
	if (boost::filesystem::exists(filename) && boost::filesystem::is_regular_file(filename)) {
		addOrReplace(new FilePackageEntry(allocateString(name), allocateString(filename), deleteOnDestroy));
	} else throw; // TODO better error handling
}

void ShadyCore::Package::appendFile(const char* name, std::istream& input) {
	boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	std::ofstream output(tempFile.string().c_str(), std::ios::binary);

	char buffer[4096]; int read;
	while (read = input.read(buffer, 4096).gcount()) {
		output.write(buffer, read);
	} output.close();

	addOrReplace(new FilePackageEntry(allocateString(name), allocateString(tempFile.string().c_str()), true));
}

void ShadyCore::Package::saveDirectory(const char* directory, Callback* callback) {
	char buffer[4096];
	if (!boost::filesystem::exists(directory)) {
		boost::filesystem::create_directories(directory);
	}
	else if (!boost::filesystem::is_directory(directory)) return;

	unsigned int index = 0, fileCount = entries.size();
	boost::filesystem::path target(directory);
	boost::filesystem::absolute(target);
	for (auto entry : entries) {
		boost::filesystem::path filename(target / entry.first);
		if (!boost::filesystem::exists(filename.parent_path()))
			boost::filesystem::create_directories(filename.parent_path());

		if (callback) callback(entry.first, ++index, fileCount);
		boost::filesystem::path tempFile = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
		std::ofstream output(tempFile.native(), std::ios::binary);
		std::istream& input = entry.second->open();

		int read = input.read(buffer, 4096).gcount();
		while (read) {
			output.write(buffer, read);
			read = input.read(buffer, 4096).gcount();
		}

		entry.second->close();
		output.close();

		boost::filesystem::copy_file(tempFile, filename, boost::filesystem::copy_option::overwrite_if_exists);
		boost::filesystem::remove(tempFile);
	}
}
