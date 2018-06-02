#ifndef RCONSRV_H
#define RCONSRV_H

#include "noncopyable.h"
#include "logger.h"
#include <list>
#include <set>
#include <giomm/asyncresult.h>
#include <giomm/socketconnection.h>
#include <giomm/socketservice.h>
#include <glibmm/refptr.h>
#include <sigc++/trackable.h>
#include <referee.pb.h>

class Configuration;
class GameController;
class SSL_RefereeRemoteControlRequest;
class SSL_RefereeRemoteControlReply;

class RConServer : public NonCopyable, public sigc::trackable {
	public:
		RConServer(GameController &controller);
		~RConServer();
		Logger &logger;
		std::set<SSL_Referee_Command> commands_on_hold;

		void set_commands_on_hold(const std::set<SSL_Referee_Command> &commands);

	private:
		class Connection : public NonCopyable, public sigc::trackable {
			public:
				Connection(RConServer &server, const Glib::RefPtr<Gio::SocketConnection> &sock);
				~Connection();
				bool paused;

				void set_connection_list_iterator(std::list<Connection>::iterator iter);
				void finished_read_data(bool ok);

			private:
				RConServer &server;
				Glib::RefPtr<Gio::SocketConnection> sock;
				std::list<Connection>::iterator connection_list_iterator;
				uint32_t length;
				std::vector<unsigned char> buffer;

				void start_read_length();
				void finished_read_length(bool ok);
				void finished_write_reply(bool ok);

				void execute_request(const SSL_RefereeRemoteControlRequest &request, SSL_RefereeRemoteControlReply &reply, bool &delayRequest);

				void start_read_fully(void *buffer, std::size_t length, const sigc::slot<void, bool> &slot);
				void finished_read_fully_partial(Glib::RefPtr<Gio::AsyncResult> &result, void *rptr, std::size_t left, const sigc::slot<void, bool> &slot);

				void start_write_fully(const void *data, std::size_t length, const sigc::slot<void, bool> &slot);
				void finished_write_fully_partial(Glib::RefPtr<Gio::AsyncResult> &result, const void *wptr, std::size_t left, const sigc::slot<void, bool> &slot);
		};

		GameController &controller;
		Glib::RefPtr<Gio::SocketService> listener;
		std::list<Connection> connections;

		bool on_incoming(const Glib::RefPtr<Gio::SocketConnection> &sock, const Glib::RefPtr<Glib::Object> &);
};

#endif
