
#ifndef MESSAGEPOOL_H
#define MESSAGEPOOL_H

#include <mpi.h>
#include <unordered_map>
#include <string>
#include <climits>

#include "tools.h"

const int msgSize = 512;

// this class does not have to be visible to users
class MessageBuffer {
public:
    MessageBuffer(
            MPI_Request *request, 
            void *buf, /* this can be nullptr when isSend == true */ 
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
    void *buf_;
    void *realBuf_;
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
    void *addMessage(
            MPI_Request *request, 
            void *buf, 
            MPI_Datatype dataType, 
            int count,
            int tag,
            MPI_Comm comm,
            int src,
            bool isSend = false);

    /*
     * Given that the message is already delivered, this function should load the original buffer
     * and return the string that represents the node timing
     * throws an exception if the request is not found
     * deletes the message buffer after loading the message
     * updates the count of the status too
     * returns an empty string when the request is send and not receive
     */
    std::string loadMessage(
            MPI_Request *request, 
            MPI_Status *status = nullptr);
    
    /*
     * This simply updates the member called count on MPI_Status of the message request
     */
    void updateStatusCount(
            MPI_Request *request,
            MPI_Status *status);

    /*
     * This function is simply used when 
     * MPI_Request * is not available
     */
    void updateStatusCount(
            int source,
            int tag,
            MPI_Comm comm,
            MPI_Status *status);

    /*
     * This function adds an element to peeked_ vector
     * it returns count
     */
    int addPeekedMessage(
            void *realBuf, 
            MPI_Datatype dataType, 
            int count,
            int tag,
            MPI_Comm comm,
            int src,
            MPI_Status *status);
 
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
        int src);

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
