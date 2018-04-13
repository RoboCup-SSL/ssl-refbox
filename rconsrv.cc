#include "rconsrv.h"
#include "configuration.h"
#include "gamecontroller.h"
#include "logger.h"
#include "rcon.pb.h"
#include <cstring>
#include <giomm/error.h>
#include <giomm/inetsocketaddress.h>
#include <giomm/socketaddress.h>
#include <glibmm/ustring.h>

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

namespace {
	const unsigned int MAX_PACKET_SIZE = 4096;

	Glib::ustring format_address(const Glib::RefPtr<Gio::SocketAddress> &address) {
		const Glib::RefPtr<Gio::InetSocketAddress> &inet_address = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(address);
		if (inet_address) {
			return Glib::ustring::compose(u8"%1:%2", inet_address->get_address()->to_string(), inet_address->get_port());
		} else {
			return u8"<Unknown Address>";
		}
	}
}



RConServer::RConServer(GameController &controller) :
		controller(controller),
		listener(Gio::SocketService::create()),
		connections()
{
	listener->add_inet_port(controller.configuration.rcon_port);
	listener->signal_incoming().connect(sigc::mem_fun(this, &RConServer::on_incoming));
	listener->start();
}

RConServer::~RConServer() {
	listener->stop();
	listener->close();
}

bool RConServer::on_incoming(const Glib::RefPtr<Gio::SocketConnection> &sock, const Glib::RefPtr<Glib::Object> &) {
	connections.emplace_back(*this, sock);
	std::list<Connection>::iterator iterator = connections.end();
	--iterator;
	iterator->set_connection_list_iterator(iterator);
	return false;
}



RConServer::Connection::Connection(RConServer &server, const Glib::RefPtr<Gio::SocketConnection> &sock) :
		server(server),
		sock(sock)
{
	server.controller.logger.write(Glib::ustring::compose(u8"Accepted remote control connection from %1", format_address(sock->get_remote_address())));
	if (!sock->get_socket()->set_option(IPPROTO_TCP, TCP_NODELAY, 1)) {
		server.controller.logger.write(u8"Warning: unable to set TCP_NODELAY option");
	}
	start_read_length();
}

RConServer::Connection::~Connection() {
	server.controller.logger.write(Glib::ustring::compose(u8"End remote control connection from %1", format_address(sock->get_remote_address())));
	sock->close();
}

void RConServer::Connection::set_connection_list_iterator(std::list<Connection>::iterator iter) {
	connection_list_iterator = iter;
}

void RConServer::Connection::start_read_length() {
	start_read_fully(&length, sizeof(length), sigc::mem_fun(this, &RConServer::Connection::finished_read_length));
}

void RConServer::Connection::finished_read_length(bool ok) {
	if (ok) {
		length = ntohl(length);
		if (length <= MAX_PACKET_SIZE) {
			buffer.resize(length);
			start_read_fully(&buffer[0], buffer.size(), sigc::mem_fun(this, &RConServer::Connection::finished_read_data));
		} else {
			server.controller.logger.write(Glib::ustring::compose(u8"Packet size %1 too large", length));
			server.connections.erase(connection_list_iterator);
		}
	} else {
		server.connections.erase(connection_list_iterator);
	}
}

void RConServer::Connection::finished_read_data(bool ok) {
	if (ok) {
		SSL_RefereeRemoteControlRequest request;
		if (request.ParseFromArray(&buffer[0], static_cast<int>(buffer.size()))) {
			SSL_RefereeRemoteControlReply reply;
			execute_request(request, reply);
			length = reply.ByteSize();
			buffer.resize(sizeof(length) + length);
			reply.SerializeWithCachedSizesToArray(&buffer[sizeof(length)]);
			length = htonl(length);
			std::memcpy(&buffer[0], &length, sizeof(length));
			start_write_fully(&buffer[0], buffer.size(), sigc::mem_fun(this, &RConServer::Connection::finished_write_reply));
		} else {
			server.controller.logger.write(u8"Protobuf parsing failed");
			server.connections.erase(connection_list_iterator);
		}
	} else {
		server.connections.erase(connection_list_iterator);
	}
}

void RConServer::Connection::finished_write_reply(bool ok) {
	if (ok) {
		start_read_length();
	} else {
		server.connections.erase(connection_list_iterator);
	}
}

void RConServer::Connection::execute_request(const SSL_RefereeRemoteControlRequest &request, SSL_RefereeRemoteControlReply &reply) {
	reply.set_message_id(request.message_id());
	reply.set_outcome(SSL_RefereeRemoteControlReply::OK);

	if (request.has_last_command_counter()) {
		if (request.last_command_counter() != server.controller.state.referee().command_counter()) {
			reply.set_outcome(SSL_RefereeRemoteControlReply::BAD_COMMAND_COUNTER);
			return;
		}
	}

	unsigned int actions = request.has_stage() + request.has_command() + request.has_card();
	if (actions > 1) {
		reply.set_outcome(SSL_RefereeRemoteControlReply::MULTIPLE_ACTIONS);
		return;
	}

	if ((request.has_designated_position() && !request.has_command())
			|| (request.has_command() && (request.has_designated_position() != server.controller.command_needs_designated_position(request.command())))) {
		reply.set_outcome(SSL_RefereeRemoteControlReply::BAD_DESIGNATED_POSITION);
		return;
	}

	if (request.has_stage()) {
		if (server.controller.can_enter_stage(request.stage())) {
			server.controller.enter_stage(request.stage());
		} else {
			reply.set_outcome(SSL_RefereeRemoteControlReply::BAD_STAGE);
			return;
		}
	} else if (request.has_command()) {
		if (server.controller.can_set_command(request.command())) {

			server.controller.set_command(request.command(), request.designated_position().x(), request.designated_position().y(), false);
		} else {
			reply.set_outcome(SSL_RefereeRemoteControlReply::BAD_COMMAND);
			return;
		}
	} else if (request.has_card()) {
		if (server.controller.can_issue_card()) {
			SaveState::Team team = request.card().team() == SSL_RefereeRemoteControlRequest::CardInfo::TEAM_YELLOW ? SaveState::TEAM_YELLOW : SaveState::TEAM_BLUE;
			if (request.card().type() == SSL_RefereeRemoteControlRequest::CardInfo::CARD_YELLOW) {
				server.controller.yellow_card(team);
			} else {
				server.controller.red_card(team);
			}
		} else {
			reply.set_outcome(SSL_RefereeRemoteControlReply::BAD_CARD);
			return;
		}
	}

	if(request.has_gameevent()) {
		server.controller.set_game_event(&request.gameevent());
	}
}

void RConServer::Connection::start_read_fully(void *buffer, std::size_t length, const sigc::slot<void, bool> &slot) {
	sock->get_input_stream()->read_async(buffer, length, sigc::bind(sigc::mem_fun(this, &RConServer::Connection::finished_read_fully_partial), buffer, length, slot));
}

void RConServer::Connection::finished_read_fully_partial(Glib::RefPtr<Gio::AsyncResult> &result, void *rptr, std::size_t left, const sigc::slot<void, bool> &slot) {
	try {
		gssize bytes_read = sock->get_input_stream()->read_finish(result);
		if (bytes_read > 0) {
			unsigned char *rptr_ch = static_cast<unsigned char *>(rptr);
			rptr_ch += bytes_read;
			left -= bytes_read;
			if (left) {
				sock->get_input_stream()->read_async(rptr_ch, left, sigc::bind(sigc::mem_fun(this, &RConServer::Connection::finished_read_fully_partial), rptr_ch, left, slot));
			} else {
				slot(true);
			}
		} else {
			slot(false);
		}
	} catch (const Gio::Error &) {
		slot(false);
	}
}

void RConServer::Connection::start_write_fully(const void *data, std::size_t length, const sigc::slot<void, bool> &slot) {
	sock->get_output_stream()->write_async(data, 1, sigc::bind(sigc::mem_fun(this, &RConServer::Connection::finished_write_fully_partial), data, length, slot));
}

void RConServer::Connection::finished_write_fully_partial(Glib::RefPtr<Gio::AsyncResult> &result, const void *wptr, std::size_t left, const sigc::slot<void, bool> &slot) {
	try {
		gssize bytes_written = sock->get_output_stream()->write_finish(result);
		if (bytes_written > 0) {
			const unsigned char *wptr_ch = static_cast<const unsigned char *>(wptr);
			wptr_ch += bytes_written;
			left -= bytes_written;
			if (left) {
				sock->get_output_stream()->write_async(wptr_ch, left, sigc::bind(sigc::mem_fun(this, &RConServer::Connection::finished_write_fully_partial), wptr_ch, left, slot));
			} else {
				slot(true);
			}
		} else {
			slot(false);
		}
	} catch (const Gio::Error &) {
		slot(false);
	}
}
