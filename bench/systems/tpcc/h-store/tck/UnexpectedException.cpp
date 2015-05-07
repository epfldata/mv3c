#include "UnexpectedException.h"

UnexpectedException::UnexpectedException() : Exception()
{}

UnexpectedException::UnexpectedException(const char* msg_) : Exception(msg_)
{}
UnexpectedException::~UnexpectedException()
{}
