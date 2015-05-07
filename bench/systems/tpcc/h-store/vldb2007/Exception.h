#ifndef EXCEPTION_H
#define EXCEPTION_H

class Exception{
public:

	Exception();
	Exception(char *msg_);
	virtual ~Exception();
	char* msg;
};

#endif // EXCEPTION_H
