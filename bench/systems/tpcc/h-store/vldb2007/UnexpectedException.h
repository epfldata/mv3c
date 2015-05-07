#ifndef UNEXPECTEDEXCEPTION_H
#define UNEXPECTEDEXCEPTION_H

#include "Exception.h"

class UnexpectedException : public Exception{
public:

	UnexpectedException();
	UnexpectedException(char* msg_);
	virtual ~UnexpectedException();
};

#endif // UNEXPECTEDEXCEPTION_H
