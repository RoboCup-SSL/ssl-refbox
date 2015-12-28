#include "rconsrv.h"
#include "configuration.h"
#include "gamecontroller.h"
#include "logger.h"
#include "rcon.pb.h"
#include <cstring>
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
	sock->get_input_stream()->read_all_async(&length, sizeof(length), sigc::mem_fun(this, &RConServer::Connection::finished_read_length));
}

void RConServer::Connection::finished_read_length(const Glib::RefPtr<Gio::AsyncResult> &result) {
	std::size_t bytes_read;
	if (sock->get_input_stream()->read_all_finish(result, bytes_read) && bytes_read == sizeof(length)) {
		length = ntohl(length);
		if (length <= MAX_PACKET_SIZE) {
			buffer.resize(length);
			sock->get_input_stream()->read_all_async(&buffer[0], buffer.size(), sigc::mem_fun(this, &RConServer::Connection::finished_read_data));
		} else {
			server.controller.logger.write(Glib::ustring::compose(u8"Packet size %1 too large", length));
			server.connections.erase(connection_list_iterator);
		}
	} else {
		server.connections.erase(connection_list_iterator);
	}
}

void RConServer::Connection::finished_read_data(const Glib::RefPtr<Gio::AsyncResult> &result) {
	std::size_t bytes_read;
	if (sock->get_input_stream()->read_all_finish(result, bytes_read) && bytes_read == buffer.size()) {
		SSL_RefereeRemoteControlRequest request;
		if (request.ParseFromArray(&buffer[0], static_cast<int>(buffer.size()))) {
			SSL_RefereeRemoteControlReply reply;
			execute_request(request, reply);
			length = reply.ByteSize();
			buffer.resize(sizeof(length) + length);
			reply.SerializeWithCachedSizesToArray(&buffer[sizeof(length)]);
			length = htonl(length);
			std::memcpy(&buffer[0], &length, sizeof(length));
			sock->get_output_stream()->write_all_async(&buffer[0], buffer.size(), sigc::mem_fun(this, &RConServer::Connection::finished_write_reply));
		} else {
			server.controller.logger.write(u8"Protobuf parsing failed");
			server.connections.erase(connection_list_iterator);
		}
	} else {
		server.connections.erase(connection_list_iterator);
	}
}

void RConServer::Connection::finished_write_reply(const Glib::RefPtr<Gio::AsyncResult> &result) {
	std::size_t bytes_written;
	if (sock->get_output_stream()->write_all_finish(result, bytes_written) && bytes_written == buffer.size()) {
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
		}
	} else if (request.has_command()) {
		if (server.controller.can_set_command(request.command())) {
			server.controller.set_command(request.command(), request.designated_position().x(), request.designated_position().y());
		} else {
			reply.set_outcome(SSL_RefereeRemoteControlReply::BAD_COMMAND);
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
		}
	}
}
