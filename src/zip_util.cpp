#include "zip_util.h"
#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <iostream>

namespace rsjfw {

namespace fs = std::filesystem;

static bool copyData(struct archive* a, struct archive* ext) {
    const void* buff;
    size_t size;
    la_int64_t offset;

    while (true) {
        int r = archive_read_data_block(a, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) return true;
        if (r < ARCHIVE_OK) return false;
        r = archive_write_data_block(ext, buff, size, offset);
        if (r < ARCHIVE_OK) return false;
    }
}

bool ZipUtil::extract(const std::string& archivePath,
                      const std::string& destPath,
                      ProgressCallback cb)
{
    namespace fs = std::filesystem;

    if (!fs::exists(archivePath) || fs::file_size(archivePath) == 0)
        return false;

    size_t totalBytes = fs::file_size(archivePath);

    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, archivePath.c_str(), 10240) != ARCHIVE_OK) {
        std::cerr << archive_error_string(a) << "\n";
        archive_read_free(a);
        return false;
    }

    struct archive* ext = archive_write_disk_new();
    archive_write_disk_set_options(
        ext,
        ARCHIVE_EXTRACT_TIME |
        ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_FFLAGS |
        ARCHIVE_EXTRACT_ACL
    );
    archive_write_disk_set_standard_lookup(ext);

    archive_entry* entry;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string entryName = archive_entry_pathname(entry);

        if (!entryName.empty() && entryName[0] == '/')
            entryName.erase(0, 1);

        fs::path p = fs::path(entryName).lexically_normal();
        fs::path safePath;
        for (auto& part : p) {
            if (part == "..") continue;
            safePath /= part;
        }

        fs::path fullPath = fs::path(destPath) / safePath;
        archive_entry_set_pathname(entry, fullPath.c_str());

        if (cb) {
            float progress = (float)archive_filter_bytes(a, -1) / totalBytes;
            std::string name = safePath.string();
            // Truncate if too long, but allow more length for detail
            if (name.length() > 60) name = "..." + name.substr(name.length() - 57);
            cb(progress, "extracting " + name);
        }

        int r = archive_write_header(ext, entry);
        if (r < ARCHIVE_OK)
            std::cerr << "Header failed: " << archive_error_string(ext) << "\n";
        else if (archive_entry_size(entry) > 0) {
            const void* buff;
            size_t size;
            la_int64_t offset;
            while (true) {
                int rd = archive_read_data_block(a, &buff, &size, &offset);
                if (rd == ARCHIVE_EOF) break;
                if (rd < ARCHIVE_OK) {
                    std::cerr << "Read failed: " << archive_error_string(a) << "\n";
                    break;
                }
                int wr = archive_write_data_block(ext, buff, size, offset);
                if (wr < ARCHIVE_OK) {
                    std::cerr << "Write failed: " << archive_error_string(ext) << "\n";
                    break;
                }
            }
        }

        archive_write_finish_entry(ext);
    }

    if (cb)
        cb(1.0f, "Extraction complete");

    archive_write_close(ext);
    archive_write_free(ext);
    archive_read_close(a);
    archive_read_free(a);

    return true;
}

}
