#ifndef GAMESTATE_H
#define GAMESTATE_H

#include <string>
#include <vector>
#include <glibmm/iochannel.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>
#include "referee.pb.h"
#include "socket.h"

class GameState {
	public:
		bool ok;
		SSL_Referee referee;
		sigc::signal<void> signal_updated;

		GameState(const std::string &interface, const std::string &group, const std::string &port);

	private:
		std::vector<Socket> sockets;
		sigc::connection timeout_connection;

		bool handle_io_ready(Glib::IOCondition cond);
		bool handle_timeout();
};

#endif

