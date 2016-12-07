#ifndef PROGRAM_H
#define PROGRAM_H
#include "util/types.h"
#include <iostream>

struct Program {
    const char prgType;
    Transaction xact;
    ThreadLocal *threadVar;

    Program(char id) : prgType(id) {
    }

    Program(const Program* p) : prgType(p->prgType) {
    }

    virtual TransactionReturnStatus execute() {
        return SUCCESS;
    }

    virtual std::ostream& print(std::ostream& s) {
        return s;
    }

};


#endif /* PROGRAM_H */

