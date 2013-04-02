#include "savegame.h"
#include "noncopyable.h"
#include "referee.pb.h"
#include "savestate.pb.h"
#include <fstream>
#include <stdexcept>
#include <glibmm/convert.h>
#include <glibmm/ustring.h>

#ifdef __linux__
#include "exception.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

namespace {
	class FD : public NonCopyable {
		public:
			FD(const std::string &filename, int options, int mode);
			~FD();
			void write(const void *data, std::size_t length);
			void fsync();
			void close();

		private:
			int fd;
	};
}

FD::FD(const std::string &filename, int options, int mode) : fd(::open(filename.c_str(), options, mode)) {
	if (fd < 0) {
		int rc = errno;
		throw SystemError(Glib::locale_from_utf8(Glib::ustring::compose(u8"Error creating file %1", Glib::filename_to_utf8(filename))), rc);
	}
}

FD::~FD() {
	if (fd >= 0) {
		close();
	}
}

void FD::write(const void *data, std::size_t length) {
	ssize_t ssz = ::write(fd, data, length);
	if (ssz != static_cast<ssize_t>(length)) {
		throw SystemError("Error writing to saved state file");
	}
}

void FD::fsync() {
	if (::fsync(fd) < 0) {
		throw SystemError("Error flushing saved state file");
	}
}

void FD::close() {
	if (::close(fd) < 0) {
		throw SystemError("Error closing saved state file");
	}
	fd = -1;
}
#endif

void save_game(const SaveState &ss, const std::string &save_filename) {
	// We never save the current game state if we are in post-game.
	// It is better to leave the save file holding the last state just before we ended the game.
	// Thus, if the transition to post-game was accidental, the operator can recover.
	if (ss.referee().stage() == SSL_Referee::POST_GAME) {
		return;
	}

	// If there’s no filename provided, don’t try to save.
	if (save_filename.empty()) {
		return;
	}

#ifdef __linux__
	// On Linux, we can do the 100%-safe create-new-file, write-to-new-file, fsync-new-file, close-new-file, rename-over-old-file method.
	std::string data;
	if (!ss.SerializeToString(&data)) {
		throw std::runtime_error("Protobuf error serializing game state!");
	}
	std::string temp_filename = save_filename + ".new";
	FD fd(temp_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	fd.write(data.data(), data.size());
	fd.fsync();
	fd.close();
	if (::rename(temp_filename.c_str(), save_filename.c_str()) < 0) {
		int rc = errno;
		throw SystemError(Glib::locale_from_utf8(Glib::ustring::compose(u8"I/O error renaming \"%1\" to \"%2\"!", Glib::filename_to_utf8(temp_filename), Glib::filename_to_utf8(save_filename))), rc);
	}
#else
	// On other platforms, just do an ordinary C++ file write and hope we don’t get a whole system crash or power loss that destroys the file.
	std::ofstream ofs;
	ofs.exceptions(std::ios_base::badbit | std::ios_base::failbit);
	ofs.open(save_filename, std::ios_base::out | std::ios_base::binary);
	if (!ss.SerializeToOstream(&ofs)) {
		throw std::runtime_error(Glib::locale_from_utf8(Glib::ustring::compose(u8"Protobuf error saving game state to file \"%1\"!", Glib::filename_to_utf8(save_filename))));
	}
	ofs.close();
#endif
}

