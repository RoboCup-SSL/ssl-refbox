#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

class NonCopyable {
	public:
		NonCopyable(const NonCopyable &);
		NonCopyable &operator=(const NonCopyable &);

	protected:
		NonCopyable();
};



NonCopyable::NonCopyable(const NonCopyable &) = delete;

NonCopyable &NonCopyable::operator=(const NonCopyable &) = delete;

inline NonCopyable::NonCopyable() {
}

#endif

