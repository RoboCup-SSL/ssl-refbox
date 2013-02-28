#ifndef PROTOBUF_PUBLISHER_H
#define PROTOBUF_PUBLISHER_H

#include "noncopyable.h"
#include "publisher.h"
#include "udpbroadcast.h"

class Configuration;
class Logger;

class ProtobufPublisher : public NonCopyable, public Publisher {
	public:
		ProtobufPublisher(const Configuration &configuration, Logger &logger);
		void publish(SaveState &state);

	private:
		UDPBroadcast bcast;
};

#endif

