
#include "messagePool.h"

using namespace std;

MessageBuffer::MessageBuffer(
        MPI_Request *request, 
        void *buf, 
        MPI_Datatype dataType,
        int count,
        int tag,
        MPI_Comm comm,
        int src) :
    request_(request),
    buf_(buf),
    dataType_(dataType),
    count_(count),
    tag_(tag),
    comm_(comm),
    src_(src) {
        realBuf_ = malloc(sizeof(char) * msgSize);
    }

MessageBuffer::~MessageBuffer() {
    if(realBuf_ != nullptr) {
        free(realBuf_);
    }
}

MessagePool::~MessagePool() {
    for(auto it = pool_.begin(); it != pool_.end(); it++) {
        delete it->second;
    }
}

void *MessagePool::addMessage(
        MPI_Request *request, 
        void *buf, 
        MPI_Datatype dataType, 
        int count,
        int tag,
        MPI_Comm comm,
        int src) {
    MPI_ASSERT(pool_.find(request) == pool_.end());
    pool_[request] = new MessageBuffer(
            request, 
            buf, 
            dataType, 
            count, 
            tag, 
            comm,
            src);
    return pool_[request]->realBuf_;
}

string MessagePool::loadMessage(MPI_Request *request) {
    MPI_ASSERT(pool_.find(request) != pool_.end());
    MessageBuffer *msgBuf = pool_[request];
    MPI_ASSERT(msgBuf->dataType_ == MPI_INT);
    string msg((char *)(msgBuf->realBuf_));
    vector<string> tokens = parse(msg, '|');
    MPI_ASSERT(tokens.size() == msgBuf->count_ + 1);
    for(int i = 0; i < msgBuf->count_; i++) {
        ((int *)(msgBuf->buf_))[i] = stoi(tokens[i]);
    }
    delete pool_[request];
    return tokens.back(); 
}

void MessagePool::deleteMessage(MPI_Request *request) {
    MPI_ASSERT(pool_.find(request) != pool_.end());
    delete pool_[request];
    pool_.erase(request);
}
