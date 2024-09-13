
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
    if(pool_.find(request) != pool_.end()) {
        fprintf(stderr, "message already exists for request: %p\n", request);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(dataType != MPI_INT
            && dataType != MPI_CHAR) {
        fprintf(stderr, "Unsupported data type\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
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
    if(pool_.find(request) == pool_.end()) {
        fprintf(stderr, "message not found for request: %p\n", request);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MessageBuffer *msgBuf = pool_[request];
    MPI_ASSERT(msgBuf->dataType_ == MPI_INT 
            || msgBuf->dataType_ == MPI_CHAR);
    string msg((char *)(msgBuf->realBuf_));
    cerr << msg << endl;
    vector<string> tokens = parse(msg, '|');
    if(tokens.size() != msgBuf->count_ + 1) {
        fprintf(stderr, "tokens.size(): %lu, count: %d\n", tokens.size(), msgBuf->count_);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(msgBuf->dataType_ == MPI_INT) {
        for(int i = 0; i < msgBuf->count_; i++) {
            ((int *)(msgBuf->buf_))[i] = stoi(tokens[i]);
        }
    } else if(msgBuf->dataType_ == MPI_CHAR) {
        for(int i = 0; i < msgBuf->count_; i++) {
            ((char *)(msgBuf->buf_))[i] = tokens[i][0];
        }
    }
    DEBUG("Loaded message: %p\n", request);
    delete pool_[request];
    pool_.erase(request);
    return tokens.back(); 
}

void MessagePool::deleteMessage(MPI_Request *request) {
    MPI_ASSERT(pool_.find(request) != pool_.end());
    delete pool_[request];
    pool_.erase(request);
}
