#ifndef IMAGEDB_H
#define IMAGEDB_H

#include <string>
#include <gdkmm/pixbuf.h>
#include <glibmm/refptr.h>
#include <unordered_map>

typedef std::unordered_map<std::string, Glib::RefPtr<Gdk::Pixbuf>> image_database_t;

image_database_t load_image_database(const std::string &path);

#endif

