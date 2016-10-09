#ifndef PROGRAM_H
#define PROGRAM_H
#include "types.h"
#include <iostream>

struct Program {
    const char prgType;
    Transaction *xact;
    Program(char id) : prgType(id), xact(nullptr){}
    Program(const Program* p): prgType(p->prgType), xact(nullptr){}

    virtual TransactionReturnStatus execute() {
        return SUCCESS;
    }

    virtual std::ostream& print(std::ostream& s) {
    }
    virtual void cleanUp(){
        
    }

};


#endif /* PROGRAM_H */

