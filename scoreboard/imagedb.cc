#include "imagedb.h"
#include <glibmm/fileutils.h>

image_database_t load_image_database(const std::string &path) {
	image_database_t db;
	Glib::Dir dir(path);
	for (std::string file : dir) {
		const std::string &full_path = Glib::build_filename(path, file);
		if (full_path.size() > 4 && full_path[full_path.size() - 4] == '.' && full_path[full_path.size() - 3] == 'p' && full_path[full_path.size() - 2] == 'n' && full_path[full_path.size() - 1] == 'g') {
			Glib::RefPtr<Gdk::Pixbuf> pb(Gdk::Pixbuf::create_from_file(full_path));
			file.erase(file.end() - 4, file.end());
			db[Glib::filename_to_utf8(file).casefold_collate_key()] = pb;
		}
	}
	return db;
}

