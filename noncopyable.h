#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

class NonCopyable {
	public:
		NonCopyable(const NonCopyable &) = delete;
		NonCopyable &operator=(const NonCopyable &) = delete;

	protected:
		NonCopyable();
};



inline NonCopyable::NonCopyable() {
}

#endif

