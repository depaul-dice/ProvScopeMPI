
#ifndef MESSAGEPOOL_H
#define MESSAGEPOOL_H

#include <mpi.h>
#include <unordered_map>
#include <string>

#include "tools.h"

const int msgSize = 256;

// this class does not have to be visible to users
class MessageBuffer {
public:
    MessageBuffer(
            MPI_Request *request, 
            void *buf, 
            MPI_Datatype dataType,
            int count,
            int tag,
            MPI_Comm comm,
            int src);
    MessageBuffer(const MessageBuffer&) = delete;
    MessageBuffer& operator=(const MessageBuffer&) = delete;
    /*
     * The destructor should delete the realBuf
     */
    ~MessageBuffer();

    MPI_Request *request_;
    void *buf_;
    void *realBuf_;
    MPI_Datatype dataType_;
    int count_;
    int tag_;
    MPI_Comm comm_;
    int src_;
};

class MessagePool {
public:
    MessagePool() = default;
    MessagePool(const MessagePool&) = delete;
    MessagePool& operator=(const MessagePool&) = delete;
    /*
     * This will delete all the MessageBuffers created in the pool 
     * Call it at the MPI_Finalize
     */
    ~MessagePool();

    /*
     * This is the abstraction function
     * it should be given MPI_Request *, buffer, datatype, and rank and convert them into a message
     * return the pointer to a message
     */
    void *addMessage(
            MPI_Request *request, 
            void *buf, 
            MPI_Datatype dataType, 
            int count,
            int tag,
            MPI_Comm comm,
            int src);

    /*
     * Given that the message is already delivered, this function should load the original buffer
     * and return the string that represents the node timing
     * throws an exception if the request is not found
     * deletes the message buffer after loading the message
     */
    std::string loadMessage(MPI_Request *request);
    
    /*
     * This function deletes the message buffer
     * should be called at MPI_Cancel
     */
    void deleteMessage(MPI_Request *request);
    
private:
    std::unordered_map<MPI_Request *, MessageBuffer *> pool_;
};

#endif // MESSAGEPOOL_H
