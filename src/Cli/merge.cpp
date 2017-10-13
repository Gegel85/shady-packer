#include "command.hpp"
#include "../Core/resource/readerwriter.hpp"
#include <png.h>

static void pngRead(png_structp pngData, png_bytep buffer, png_size_t length) {
	std::istream* input = (std::istream*) png_get_io_ptr(pngData);
	input->read((char*)buffer, length);
}

static void pngWrite(png_structp pngData, png_bytep buffer, png_size_t length) {
	std::ostream* output = (std::ostream*) png_get_io_ptr(pngData);
	output->write((char*)buffer, length);
}

static void pngFlush(png_structp pngData) {
	std::ostream* output = (std::ostream*) png_get_io_ptr(pngData);
	output->flush();
}

static void pngError(png_structp pngData, png_const_charp message) {
	printf("%s\n", message);
	throw;
}

static void pngWarning(png_structp pngData, png_const_charp message) {
	printf("%s\n", message);
}

static inline ShadyCore::Palette* getPalette(std::string filename) {
    if (!boost::filesystem::is_regular_file(filename)) {
        printf("File \"%s\" is not readable.", filename.c_str());
        return 0;
    }

    std::ifstream input(filename.c_str(), std::ios::binary);
    const ShadyCore::FileType& type = ShadyCore::FileType::get(filename.c_str(), input);
    if (type != ShadyCore::FileType::TYPE_PALETTE) {
        printf("File \"%s\" is not a palette.", filename.c_str());
        return 0;
    }

    return (ShadyCore::Palette*)ShadyCore::readResource(type, input);
}

static inline bool isPNG(std::istream& input) {
    uint8_t buffer[8];
    input.read((char*)buffer, 8);
    input.seekg(0);
    return !png_sig_cmp(buffer, 0, 8);
}

static inline bool transformImage(uint8_t** rows, const uint8_t* palette, uint32_t rowSize, uint32_t height, int32_t channels) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x * channels < rowSize; ++x) {
            uint8_t* color = rows[y] + x * channels;
            int i; for (i = 0; i < 256; ++i)
                if (palette[i * 3] == *(color) && palette[i * 3 + 1] == *(color + 1) && palette[i * 3 + 1] == *(color + 1) ) break;
            if (i >= 256) return false;
            *(rows[y] + x) = (uint8_t)i;
        }
    }

    return true;
}

void ShadyCli::MergeCommand::buildOptions(cxxopts::Options& options) {
	options.add_options()
		("p,palette", "For each PNG file specified in `files`, it makes the image indexed by this palette.", cxxopts::value<std::string>())
		("files", "Source directories or data packages", cxxopts::value<std::vector<std::string> >())
        ;

    options.parse_positional("files");
    options.positional_help("<files>...");
}

void ShadyCli::MergeCommand::processPalette(ShadyCore::Palette* palette, boost::filesystem::path filename) {
    if (!boost::filesystem::is_regular_file(filename)) return; // ignore
    if (filename.extension() != ".png") return; // ignore

    std::fstream file(filename.native(), std::ios::in | std::ios::out | std::ios::binary);
    if (!isPNG(file)) return;

	png_structp pngData = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, pngError, pngWarning);
	png_infop pngInfo = png_create_info_struct(pngData);
	png_set_read_fn(pngData, &file, pngRead);
	png_read_info(pngData, pngInfo);

    uint32_t rowSize = png_get_rowbytes(pngData, pngInfo);
    uint32_t height = png_get_image_height(pngData, pngInfo);
    int32_t type = png_get_color_type(pngData, pngInfo);
    int32_t depth = png_get_bit_depth(pngData, pngInfo);
    int32_t channels = png_get_channels(pngData, pngInfo);
    uint8_t* data = new uint8_t[rowSize*height];
    uint8_t** rows = new uint8_t*[height];
    for (int i = 0; i < height; ++i) {
        rows[i] = data + rowSize * i;
    }

    png_read_image(pngData, rows);
	png_read_end(pngData, 0);
    png_destroy_read_struct(&pngData, 0, 0);
    file.seekp(0);

    if (channels >= 3 && depth == 8) {
        if (!transformImage(rows, palette->getData(), rowSize, height, channels)) {
            printf("Image \"%s\" has missing colors.", filename.generic_string().c_str());
            delete[] rows;
            delete[] data;
            return;
        }
    } else if (type != PNG_COLOR_TYPE_PALETTE) {
        printf("Unsuported image \"%s\"", filename.generic_string().c_str());
        delete[] rows;
        delete[] data;
        return;
    }


    pngData = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, pngError, pngWarning);
    png_set_write_fn(pngData, &file, pngWrite, pngFlush);

    png_set_IHDR(pngData, pngInfo, rowSize / channels, height, 8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_PLTE(pngData, pngInfo, (png_const_colorp)palette->getData(), 256);
    png_write_info(pngData, pngInfo);

    png_write_image(pngData, rows);
	png_write_end(pngData, 0);
    png_destroy_write_struct(&pngData, &pngInfo);
}

void ShadyCli::MergeCommand::run(const cxxopts::Options& options) {
    auto& files = options["files"].as<std::vector<std::string> >();
    ShadyCore::Palette* palette = 0;
    if (options.count("palette")) {
        palette = getPalette(options["palette"].as<std::string>());
    }

	size_t size = files.size();
	for (size_t i = 0; i < size; ++i) {
		boost::filesystem::path path(files[i]);
		if (boost::filesystem::is_directory(path)) {
            for (boost::filesystem::recursive_directory_iterator iter(path), end; iter != end; ++iter) {
                if (!boost::filesystem::is_directory(iter->path())) {
                    if (palette) processPalette(palette, iter->path());
                }
            }
		} else if (boost::filesystem::exists(path)) {
			if (palette) processPalette(palette, path);
		}
	}

    delete palette;
}