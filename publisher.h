#ifndef PUBLISHER_H
#define PUBLISHER_H

class SaveState;

class Publisher {
	public:
		virtual void publish(SaveState &state) = 0;
};

#endif

