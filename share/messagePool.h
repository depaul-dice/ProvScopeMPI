
#ifndef MESSAGEPOOL_H
#define MESSAGEPOOL_H

#include <mpi.h>
#include <unordered_map>
#include <string>
/* #include <climits> */
#include <stdexcept>
#include <limits>

#include "utils.h"
#include "messageTools.h"
#include "alignment.h"

const int msgSize = 8192;

// this class does not have to be visible to users
class MessageBuffer {
public:
    MessageBuffer(
            MPI_Request *request, 
            void * const buf, /* this can be nullptr when isSend == true */ 
            MPI_Datatype dataType,
            int count,
            int tag,
            MPI_Comm comm,
            int src,
            bool isSend,
            unsigned long timestamp);
    MessageBuffer(const MessageBuffer&) = delete;
    MessageBuffer& operator=(const MessageBuffer&) = delete;
    /*
     * The destructor should delete the realBuf
     */
    ~MessageBuffer();

    MPI_Request *request_;
    void * const buf_;
    char *realBuf_;
    MPI_Datatype dataType_;
    int count_;
    int tag_;
    MPI_Comm comm_;
    int src_;
    bool isSend_;
    unsigned long timestamp_;
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
    char *addMessage(
            MPI_Request *request, 
            void * const buf, 
            MPI_Datatype dataType, 
            int count,
            int tag,
            MPI_Comm comm,
            int src,
            bool isSend = false);

    /*
     * This peeks the message buffer from the pool
     */
    MessageBuffer *peekMessage(
            MPI_Request *request);

    /*
     * Given that the message is already delivered, this function should load the original buffer
     * and return the string that represents the node timing
     * throws an exception if the request is not found
     * updates the count of the status too
     * returns an empty string when the request is send and not receive
     */
    std::string loadMessage(
            MPI_Request *request, 
            MPI_Status *status = nullptr);
    

    /*
     * checks if the request is a send request or not
     */
    bool isSend(
            MPI_Request *request);

    /*
     * This function returns the real buffer of the message
     */
    char *getRealBuf(
            MPI_Request *request);

    /*
     * This function looks into the peeked_ vector and manipulates status accordingly
     * returns -1 if not found
     */
    int peekPeekedMessage(
            int src,
            int tag,
            MPI_Comm comm,
            MPI_Status *status);
    /*
     * This function adds an element to peeked_ vector
     * CAUTION: this function does NOT manipulate status in any way
     * (as you can see that it's not included in the argument)
     */
    void addPeekedMessage(
            void *buf, 
            int count,
            int tag,
            MPI_Comm comm,
            int src);
 
    /*
     * This loads the buf with the message from peeked_ vector
     * peeked_ element will be deleted
     * returns the count
     * in case of not finding the right element, it returns -1
     */
    int loadPeekedMessage(
        void *buf,
        MPI_Datatype dataType,
        int count,
        int tag,
        MPI_Comm comm,
        int src,
        int *retSrc = nullptr);

    /*
     * This function deletes the message buffer
     * should be called at MPI_Cancel
     */
    void deleteMessage(MPI_Request *request);
    
private:
    std::unordered_map<MPI_Request *, MessageBuffer *> pool_;
    std::vector<MessageBuffer *> peeked_;
    unsigned long timestamp_ = 0;
};

#endif // MESSAGEPOOL_H
