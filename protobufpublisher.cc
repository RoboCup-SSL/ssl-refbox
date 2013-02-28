#include "protobufpublisher.h"
#include "configuration.h"
#include "referee.pb.h"
#include "savestate.pb.h"
#include <chrono>
#include <ctime>
#include <string>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

ProtobufPublisher::ProtobufPublisher(const Configuration &configuration, Logger &logger) : bcast(logger, configuration.address, configuration.protobuf_port, configuration.interface) {
}

void ProtobufPublisher::publish(SaveState &state) {
	// Shove in the packet timestamp.
	std::chrono::microseconds diff = std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(0);
	state.mutable_referee()->set_packet_timestamp(diff.count());

	// Serialize the packet.
	std::string packet;
	{
		google::protobuf::io::StringOutputStream sos(&packet);
		state.referee().SerializeToZeroCopyStream(&sos);
	}

	// Send the packet.
	bcast.send(packet.data(), packet.size());
}

