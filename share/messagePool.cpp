
#include "messagePool.h"

using namespace std;

MessageBuffer::MessageBuffer(
        MPI_Request *request, 
        void *buf, 
        MPI_Datatype dataType,
        int count,
        int tag,
        MPI_Comm comm,
        int src,
        bool isSend,
        unsigned long timestamp) :
    request_(request),
    buf_(buf),
    realBuf_(nullptr),
    dataType_(dataType),
    count_(count),
    tag_(tag),
    comm_(comm),
    src_(src),
    isSend_(isSend),
    timestamp_(timestamp){
        if(isSend_ == false) {
            realBuf_ = malloc(sizeof(char) * msgSize);
        }
        if(timestamp_ == ULONG_MAX) {
            fprintf(stderr, "timestamp overflow\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
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
    for(auto it = peeked_.begin(); it != peeked_.end(); it++) {
        delete *it;
    }
}

void *MessagePool::addMessage(
        MPI_Request *request, 
        void *buf, 
        MPI_Datatype dataType, 
        int count,
        int tag,
        MPI_Comm comm,
        int src,
        bool isSend) {
    if(pool_.find(request) != pool_.end()) {
        fprintf(stderr, "message already exists for request: %p\n", request);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(dataType != MPI_INT
            && dataType != MPI_CHAR
            && dataType != MPI_BYTE) {
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
            src,
            isSend,
            timestamp_++);
    fprintf(stderr, "addMessage, request:%p, count:%d, tag:%d, src:%d, timestamp:%lu\n", 
            request, count, tag, src, timestamp_);
    return pool_[request]->realBuf_;
}

string MessagePool::loadMessage(
        MPI_Request *request, MPI_Status *status) {
    if(pool_.find(request) == pool_.end()) {
        fprintf(stderr, "message not found for request: %p\n", request);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MessageBuffer *msgBuf = pool_[request];
    MPI_ASSERT(msgBuf->dataType_ == MPI_INT 
            || msgBuf->dataType_ == MPI_CHAR
            || msgBuf->dataType_ == MPI_BYTE);

    if(msgBuf->isSend_) {
        delete pool_[request];
        pool_.erase(request);
        return "";
    }

    string msg((char *)(msgBuf->realBuf_));
    cerr << msg << endl;
    vector<string> tokens = parse(msg, '|');
    if(tokens.size() != msgBuf->count_ + 1) {
        fprintf(stderr, "tokens.size(): %lu, count: %d\n", 
                tokens.size(), msgBuf->count_);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(status != nullptr) {
        int size;
        MPI_Type_size(msgBuf->dataType_, &size);
        status->_ucount = msgBuf->count_ * size;
    }
    if(msgBuf->dataType_ == MPI_INT) {
        for(int i = 0; i < msgBuf->count_; i++) {
            ((int *)(msgBuf->buf_))[i] = stoi(tokens[i]);
        }
    } else if(msgBuf->dataType_ == MPI_CHAR) {
        for(int i = 0; i < msgBuf->count_; i++) {
            MPI_ASSERT(tokens[i].size() == 1);
            ((char *)(msgBuf->buf_))[i] = tokens[i][0];
        }
    } else if(msgBuf->dataType_ == MPI_BYTE) {
        for(int i = 0; i < msgBuf->count_; i++) {
            MPI_ASSERT(tokens[i].size() == 1);
            ((char *)(msgBuf->buf_))[i] = tokens[i][0];
        }
    } else {
        fprintf(stderr, "Unsupported data type\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    delete pool_[request];
    pool_.erase(request);
    return tokens.back(); 
}

void MessagePool::updateStatusCount(
        MPI_Request *request, MPI_Status *status) {
    MPI_ASSERT(pool_.find(request) != pool_.end());
    MPI_ASSERT(status != nullptr);
    MessageBuffer *msgBuf = pool_[request];
    /*
     * if the request is a send request, do NOT do anything
     */
    if(msgBuf->isSend_) {
        return;
    }
    string msg((char *)(msgBuf->realBuf_));
    vector<string> tokens = parse(msg, '|');
    MPI_ASSERT(tokens.size() <= msgBuf->count_ + 1);
    int size;
    MPI_Type_size(msgBuf->dataType_, &size);
    if(tokens.size() == msgBuf->count_ + 1) {
        status->_ucount = msgBuf->count_ * size;
    } else {
        for(unsigned i = 0; i < msgBuf->count_; i++) {
            if(tokens[i].size() > 1) {
                status->_ucount = i * size;
                break;
            }
        }
    }
}

/*
 * will be deleted
 */
void  MessagePool::updateStatusCount(
        int source, 
        int tag, 
        MPI_Comm comm, 
        MPI_Status *status) {
    MPI_ASSERT(status != nullptr);
    unsigned long minTimestamp = ULONG_MAX;
    MPI_Request *minRequest = nullptr;
    for(auto it = pool_.begin(); it != pool_.end(); it++) {
        if((it->second->src_ == source
                || source == MPI_ANY_SOURCE)
                && it->second->tag_ == tag
                && it->second->comm_ == comm) {
            if(it->second->timestamp_ < minTimestamp) {
                minTimestamp = it->second->timestamp_;
                minRequest = it->first;
                fprintf(stderr, "minTimestamp: %lu, minRequest: %p\n", 
                        minTimestamp, minRequest);
            }
        }
    }
    if(minRequest == nullptr) {
        fprintf(stderr, "message not found for source: %d, tag: %d\n",
            source, tag); 
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    updateStatusCount(minRequest, status);
}

int MessagePool::addPeekedMessage(
        void *realBuf, 
        MPI_Datatype dataType, 
        int count,
        int tag,
        MPI_Comm comm,
        int src,
        MPI_Status *status) {
    MPI_ASSERT(realBuf != nullptr);
    void *buf;
    string str((char *)realBuf);
    vector<string> tokens = parse(str, '|');
    if(dataType == MPI_INT) {
        buf = malloc(sizeof(int) * tokens.size() - 1);
        for(unsigned i = 0; i < tokens.size() - 1; i++) {
            ((int *)buf)[i] = stoi(tokens[i]);
        }
    } else if(dataType == MPI_CHAR
            || dataType == MPI_BYTE) {
        buf = malloc(sizeof(char) * tokens.size() - 1);     
        for(unsigned i = 0; i < tokens.size() - 1; i++) {
            MPI_ASSERT(tokens[i].size() == 1);
            ((char *)buf)[i] = tokens[i][0];
        }
    } else {
        fprintf(stderr, "Unsupported data type at addPeekedMessage\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(count != tokens.size() - 1) {
        fprintf(stderr, "tokens.size(): %lu, count: %d, it's not the same!!\n", 
                tokens.size(), count);
        count = tokens.size() - 1;
    }
    peeked_.push_back(new MessageBuffer(
                nullptr, 
                buf, 
                dataType, 
                count, 
                tag, 
                comm,
                src,
                false,
                timestamp_++));

    if(status == nullptr) {
        return tokens.size() - 1;
    }
    // else
    int size;
    MPI_Type_size(dataType, &size);
    status->_ucount = (tokens.size() - 1) * size; 
    return tokens.size() - 1;
}

int MessagePool::loadPeekedMessage(
        void *buf,
        MPI_Datatype dataType,
        int count,
        int tag,
        MPI_Comm comm,
        int src) {
    for(auto it = peeked_.begin(); it != peeked_.end(); it++) {
        if(dataType == (*it)->dataType_
                && count >= (*it)->count_
                && tag == (*it)->tag_
                && comm == (*it)->comm_
                && (src == (*it)->src_
                    || src == MPI_ANY_SOURCE)) {
            if(dataType == MPI_INT) {
                for(int i = 0; i < count; i++) {
                    ((int *)buf)[i] = ((int *)(*it)->buf_)[i];
                }
            } else if(dataType == MPI_CHAR
                    || dataType == MPI_BYTE) {
                for(int i = 0; i < count; i++) {
                    ((char *)buf)[i] = ((char *)(*it)->buf_)[i];
                }
            } else {
                fprintf(stderr, "Unsupported data type at loadPeekedMessage\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            int rv = (*it)->count_;
            delete *it;
            peeked_.erase(it);
            return rv;
        }
    }
    // if not found, let users call MPI_Recv or MPI_Irecv
    return -1;
}

void MessagePool::deleteMessage(MPI_Request *request) {
    MPI_ASSERT(pool_.find(request) != pool_.end());
    delete pool_[request];
    pool_.erase(request);
}
