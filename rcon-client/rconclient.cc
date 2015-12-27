#include "rcon.pb.h"
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <netdb.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#define MAX_REPLY_LENGTH 4096

namespace {
	void usage(const char *appName) {
		std::cerr << "Usage:\n" << appName << " refbox_address [refbox_rcon_port]\n";
		std::exit(EXIT_FAILURE);
	}

	bool recvFully(int fd, void *buffer, std::size_t length) {
		char *ptr = static_cast<char *>(buffer);
		while (length) {
			ssize_t ret = recv(fd, ptr, length, 0);
			if (ret < 0) {
				std::cerr << std::strerror(errno) << '\n';
				return false;
			} else if (!ret) {
				std::cerr << "Socket closed by remote peer.\n";
				return false;
			}
			ptr += ret;
			length -= ret;
		}
		return true;
	}

	bool sendFully(int fd, const void *buffer, std::size_t length) {
		const char *ptr = static_cast<const char *>(buffer);
		while (length) {
			ssize_t ret = send(fd, ptr, length, 0);
			if (ret < 0) {
				std::cerr << std::strerror(errno) << '\n';
				return false;
			}
			ptr += ret;
			length -= ret;
		}
		return true;
	}

	uint32_t allocMessageID() {
		static uint32_t nextMessageID = 0;
		return nextMessageID++;
	}

	void doExit(int sock) {
		std::cout << "Goodbye.\n";
		close(sock);
		std::exit(EXIT_SUCCESS);
	}

	void doRequest(int sock, const SSL_RefereeRemoteControlRequest &request) {
		{
			const std::string &message = request.SerializeAsString();
			uint32_t messageLength = htonl(static_cast<uint32_t>(message.size()));
			std::cout << "Send " << (sizeof(messageLength) + message.size()) << " bytes: ";
			std::cout.flush();
			if (!sendFully(sock, &messageLength, sizeof(messageLength)) || !sendFully(sock, message.data(), message.size())) {
				doExit(sock);
			}
			std::cout << "OK\n";
		}

		SSL_RefereeRemoteControlReply reply;
		{
			std::cout << "Receive reply: ";
			std::cout.flush();
			uint32_t replyLength;
			if (!recvFully(sock, &replyLength, sizeof(replyLength))) {
				doExit(sock);
			}
			replyLength = ntohl(replyLength);
			if (replyLength > MAX_REPLY_LENGTH) {
				std::cerr << "Got reply length " << replyLength << " which is greater than limit " << MAX_REPLY_LENGTH << ".\n";
				doExit(sock);
			}
			std::vector<char> buffer(replyLength);
			if (!recvFully(sock, &buffer[0], replyLength)) {
				doExit(sock);
			}
			std::cout << (4 + replyLength) << " bytes OK.\n";
			if (!reply.ParseFromArray(&buffer[0], replyLength)) {
				std::cerr << "Error in reply message structure.\n";
				doExit(sock);
			}
		}
		if (reply.message_id() != request.message_id()) {
			std::cerr << "Reply message ID " << reply.message_id() << " does not match request message ID " << request.message_id() << ".\n";
			doExit(sock);
		}
		std::cout << "Command result is: " << SSL_RefereeRemoteControlReply_Outcome_descriptor()->FindValueByNumber(reply.outcome())->name() << ".\n";
	}

	void doHelp(const std::vector<std::string> &commands) {
		std::cout << "Available commands are “help”, “exit”, “ping”, “card”, and the name of any game stage or\nreferee command converted to lowercase with underscores replaced by spaces.\nThe full list of commands is:\n";
		for (const std::string &command : commands) {
			std::cout << "- " << command << '\n';
		}
	}

	void doCard(int sock) {
		SSL_RefereeRemoteControlRequest request;
		request.set_message_id(allocMessageID());
		for (;;) {
			std::cout << "[Y]ellow or [R]ed card> ";
			std::cout.flush();
			std::string line;
			if (!std::getline(std::cin, line)) {
				doExit(sock);
			}
			if (line == "y" || line == "Y") {
				request.mutable_card()->set_type(SSL_RefereeRemoteControlRequest::CardInfo::CARD_YELLOW);
				break;
			} else if (line == "r" || line == "R") {
				request.mutable_card()->set_type(SSL_RefereeRemoteControlRequest::CardInfo::CARD_RED);
				break;
			}
		}
		for (;;) {
			std::cout << "[Y]ellow or [B]lue team> ";
			std::cout.flush();
			std::string line;
			if (!std::getline(std::cin, line)) {
				doExit(sock);
			}
			if (line == "y" || line == "Y") {
				request.mutable_card()->set_team(SSL_RefereeRemoteControlRequest::CardInfo::TEAM_YELLOW);
				break;
			} else if (line == "b" || line == "B") {
				request.mutable_card()->set_team(SSL_RefereeRemoteControlRequest::CardInfo::TEAM_BLUE);
				break;
			}
		}
		doRequest(sock, request);
	}

	void doCommand(int sock, bool hasStage, SSL_Referee::Stage stage, bool hasCommand, SSL_Referee::Command command, bool hasDesignatedPosition) {
		SSL_RefereeRemoteControlRequest request;
		request.set_message_id(allocMessageID());
		if (hasStage) {
			request.set_stage(stage);
		}
		if (hasCommand) {
			request.set_command(command);
		}

		if (hasDesignatedPosition) {
			static const char letters[] = "XY";
			float pos[2];
			for (unsigned int i = 0; i != 2; ++i) {
				for (;;) {
					std::cout << "Designated position " << letters[i] << "> ";
					std::cout.flush();
					std::string line;
					if (!std::getline(std::cin, line)) {
						doExit(sock);
					}
					std::size_t endPos;
					try {
						pos[i] = std::stof(line, &endPos);
						if (endPos != line.size()) {
							std::cerr << "Invalid coordinate; must be a floating-point number.\n";
						} else {
							break;
						}
					} catch (const std::invalid_argument &) {
						std::cerr << "Invalid coordinate; must be a floating-point number.\n";
					} catch (const std::out_of_range &) {
						std::cerr << "Value is too large or too small.\n";
					}
				}
			}
			request.mutable_designated_position()->set_x(pos[0]);
			request.mutable_designated_position()->set_y(pos[1]);
		}

		doRequest(sock, request);
	}
}

int main(int argc, char **argv) {
	if (argc < 2 || argc > 3) {
		usage(argv[0]);
	}

	// Parse target address.
	struct addrinfo *refboxAddresses;
	{
		struct addrinfo hints;
		hints.ai_flags = 0;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		int err = getaddrinfo(argv[1], argc == 3 ? argv[2] : "10007", &hints, &refboxAddresses);
		if (err != 0) {
			std::cerr << gai_strerror(err) << '\n';
			return EXIT_FAILURE;
		}
	}

	// Try to connect to the returned addresses until one of them succeeds.
	int sock = -1;
	for (const addrinfo *i = refboxAddresses; i; i = i->ai_next) {
		char host[256], port[32];
		int err = getnameinfo(i->ai_addr, i->ai_addrlen, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
		if (err != 0) {
			std::cerr << gai_strerror(err) << '\n';
			return EXIT_FAILURE;
		}
		std::cout << "Trying host " << host << " port " << port << ": ";
		std::cout.flush();
		sock = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if (sock < 0) {
			std::cout << std::strerror(errno) << '\n';
			continue;
		}
		if (connect(sock, i->ai_addr, i->ai_addrlen) < 0) {
			std::cout << std::strerror(errno) << '\n';
			close(sock);
			sock = -1;
			continue;
		}
		std::cout << "OK\n";
		break;
	}
	if (sock < 0) {
		std::cerr << "Unable to connect to referee box.\n";
		return EXIT_FAILURE;
	}

	// Free address info.
	freeaddrinfo(refboxAddresses);

	// Set up the command table.
	std::unordered_map<std::string, std::function<void(int)>> commands;
	std::vector<std::string> commandsOrdered;
	commands["help"] = std::bind(&doHelp, std::ref(commandsOrdered));
	commandsOrdered.push_back("help");
	commands["exit"] = &doExit;
	commandsOrdered.push_back("exit");
	commands["ping"] = std::bind(&doCommand, std::placeholders::_1, false, SSL_Referee::Stage_MIN, false, SSL_Referee::Command_MIN, false);
	commandsOrdered.push_back("ping");
	commands["card"] = &doCard;
	commandsOrdered.push_back("card");
	{
		const google::protobuf::EnumDescriptor *descriptor = SSL_Referee::Stage_descriptor();
		for (int i = 0; i != descriptor->value_count(); ++i) {
			const google::protobuf::EnumValueDescriptor *valueDescriptor = descriptor->value(i);
			std::string name = valueDescriptor->name();
			for (auto i = name.begin(), iEnd = name.end(); i != iEnd; ++i) {
				if (*i == '_') {
					*i = ' ';
				} else {
					*i = static_cast<char>(std::tolower(*i));
				}
			}
			commands[name] = std::bind(&doCommand, std::placeholders::_1, true, static_cast<SSL_Referee::Stage>(valueDescriptor->number()), false, SSL_Referee::Command_MIN, false);
			commandsOrdered.push_back(name);
		}
	}
	{
		std::unordered_set<int> hasDesignatedPosition {
			SSL_Referee::BALL_PLACEMENT_YELLOW,
			SSL_Referee::BALL_PLACEMENT_BLUE,
		};

		const google::protobuf::EnumDescriptor *descriptor = SSL_Referee::Command_descriptor();
		for (int i = 0; i != descriptor->value_count(); ++i) {
			const google::protobuf::EnumValueDescriptor *valueDescriptor = descriptor->value(i);
			std::string name = valueDescriptor->name();
			for (auto i = name.begin(), iEnd = name.end(); i != iEnd; ++i) {
				if (*i == '_') {
					*i = ' ';
				} else {
					*i = static_cast<char>(std::tolower(*i));
				}
			}
			commands[name] = std::bind(&doCommand, std::placeholders::_1, false, SSL_Referee::Stage_MIN, true, static_cast<SSL_Referee::Command>(valueDescriptor->number()), hasDesignatedPosition.count(valueDescriptor->number()) != 0);
			commandsOrdered.push_back(name);
		}
	}

	// Run the remote control client.
	std::cout << "\nRemote control client up and running. Type “help” for help.\n";
	for(;;) {
		std::cout << "> ";
		std::cout.flush();
		std::string line;
		if (!std::getline(std::cin, line)) {
			line = "exit";
			std::cout << '\n';
		}
		auto i = commands.find(line);
		if (i == commands.end()) {
			std::cerr << "Unrecognized command. Type “help” for help.\n";
		} else {
			i->second(sock);
		}
	}
}
