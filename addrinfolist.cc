#include "addrinfolist.h"
#include "exception.h"

AddrInfoList::AddrInfoList(const char *node, const char *service, const addrinfo *hints) {
	ai = 0;
	int rc = getaddrinfo(node, service, hints, &ai);
	if (rc) {
		throw GAIError("Cannot look up address info", rc);
	}
}

AddrInfoList::~AddrInfoList() {
	freeaddrinfo(ai);
}

addrinfo *AddrInfoList::get() const {
	return ai;
}

