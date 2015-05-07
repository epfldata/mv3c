#include "UnexpectedException.h"

UnexpectedException::UnexpectedException() : Exception()
{}

UnexpectedException::UnexpectedException(char* msg_) : Exception(msg_)
{}
UnexpectedException::~UnexpectedException()
{}
