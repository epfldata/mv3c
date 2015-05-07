#ifndef EXCEPTION_H
#define EXCEPTION_H

class Exception{
public:

	Exception();
	Exception(const char *msg_);
	virtual ~Exception();
	const char* msg;
};

#endif // EXCEPTION_H
