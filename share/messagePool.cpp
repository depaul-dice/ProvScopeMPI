
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
    if(dataType != MPI_INT
            && dataType != MPI_CHAR
            && dataType != MPI_BYTE) {
        fprintf(stderr, "Unsupported data type\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(pool_.find(request) == pool_.end()) {
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
    } else {
        pool_[request]->buf_ = buf;
        pool_[request]->dataType_ = dataType;
        pool_[request]->count_ = count;
        pool_[request]->tag_ = tag;
        pool_[request]->comm_ = comm;
        pool_[request]->src_ = src;
        pool_[request]->isSend_ = isSend;
        pool_[request]->timestamp_ = timestamp_++;
    }
    fprintf(stderr, "addMessage, request:%p, count:%d, tag:%d, src:%d, isSend:%d, timestamp:%lu\n", 
            request, 
            count, 
            tag, 
            src, 
            isSend,
            timestamp_);
    return pool_[request]->realBuf_;
}

MessageBuffer *MessagePool::peekMessage(
        MPI_Request *request) {
    if(pool_.find(request) == pool_.end()) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        fprintf(stderr, "message not found for request at peekMessage: %p \
                at rank:%d\n", 
                request, rank);
        return nullptr;
    }
    return pool_[request];
}

string MessagePool::loadMessage(
        MPI_Request *request, MPI_Status *status) {
    if(pool_.find(request) == pool_.end()) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        fprintf(stderr, "message not found for request at loadMessage: %p \
                at rank: %d\n", 
                request, rank);
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
    vector<string> tokens = parse(msg, '|');
    if(tokens.size() != msgBuf->count_ + 2) {
        fprintf(stderr, "tokens.size(): %lu, count: %d\n", 
                tokens.size(), msgBuf->count_);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(msgBuf->dataType_ == MPI_INT) {
        for(int i = 0; i < msgBuf->count_; i++) {
            ((int *)(msgBuf->buf_))[i] = stoi(tokens[i]);
        }
    } else if(msgBuf->dataType_ == MPI_CHAR
            || msgBuf->dataType_ == MPI_BYTE) {
        for(int i = 0; i < msgBuf->count_; i++) {
            ((char *)(msgBuf->buf_))[i] = (char)stoi(tokens[i]);
        }
    } else {
        fprintf(stderr, "Unsupported data type\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    if(status != nullptr 
            && status != MPI_STATUS_IGNORE) {
        status->MPI_SOURCE = msgBuf->src_;
        status->MPI_TAG = msgBuf->tag_;
        status->MPI_ERROR = MPI_SUCCESS;

        int size;
        MPI_Type_size(msgBuf->dataType_, &size);
        status->_ucount = msgBuf->count_ * size;
    }
    return tokens.back(); 
}

int MessagePool::peekPeekedMessage(
        int src,
        int tag,
        MPI_Comm comm,
        MPI_Status *status) {
    MPI_ASSERT(status != nullptr
            && status != MPI_STATUS_IGNORE);
    //fprintf(stderr, "peeking peeked_ size: %lu\n", peeked_.size());
    for(unsigned long i = 0; i < peeked_.size(); i++) {
        int cmpResult;
        MPI_Comm_compare(comm, peeked_[i]->comm_, &cmpResult);
        if(tag == peeked_[i]->tag_
                && cmpResult == MPI_IDENT 
                && (src == peeked_[i]->src_
                    || src == MPI_ANY_SOURCE)) {
            auto tokens = parse(string((char *)(peeked_[i]->buf_)), '|');
            int size = stoi(tokens.back());

            /*
             * currently we only update these 4 members, 
             * but could add more as needed
             */
            status->_ucount = (tokens.size() - 2) * size;
            status->MPI_SOURCE = peeked_[i]->src_;
            status->MPI_TAG = peeked_[i]->tag_;
            status->MPI_ERROR = MPI_SUCCESS;
            fprintf(stderr, "peeked message found: %lu\n", i);

            return (int)i;
        }
    }
    return -1;
}
 
void MessagePool::addPeekedMessage(
        void *buf, 
        int count,
        int tag,
        MPI_Comm comm,
        int src) {
    MPI_ASSERT(buf != nullptr);
    MPI_ASSERT(src != MPI_ANY_SOURCE);

    char *heapBuf = (char *)malloc(count);
    strncpy(heapBuf, (char *)buf, count);
    /*
     * we are pushing it back as a raw format
     * when we load, we formalize it
     */ 
    peeked_.push_back(new MessageBuffer(
                nullptr, 
                (void *)heapBuf, 
                MPI_CHAR, 
                count, 
                tag, 
                comm,
                src,
                false,
                timestamp_++));

}

int MessagePool::loadPeekedMessage(
        void *buf,
        MPI_Datatype dataType,
        int count,
        int tag,
        MPI_Comm comm,
        int src,
        int *retSrc) {
    //fprintf(stderr, "loadPeekedMessage called: %lu, tag: %d, src: %d, count: %d\n", peeked_.size(), tag, src, count);
    for(auto it = peeked_.begin(); it != peeked_.end(); it++) {
        int cmpResult;
        MPI_Comm_compare(comm, (*it)->comm_, &cmpResult);
        //fprintf(stderr, "tag: %d, src: %d, cmpResult: %d, count:%d\n", (*it)->tag_, (*it)->src_, cmpResult, (*it)->count_);
        if(count <= (*it)->count_
                && tag == (*it)->tag_
                && cmpResult == MPI_IDENT 
                && (src == (*it)->src_
                    || src == MPI_ANY_SOURCE)) {
            auto tokens = parse(string((char *)(*it)->buf_), '|');
            MPI_ASSERT(tokens.size() == count + 2);
            if(dataType == MPI_INT) {
                for(int i = 0; i < count; i++) {
                    ((int *)buf)[i] = stoi(tokens[i]);
                }
            } else if(dataType == MPI_CHAR
                    || dataType == MPI_BYTE) {
                for(int i = 0; i < count; i++) {
                    ((char *)buf)[i] = (char)stoi(tokens[i]);
                }
            } else {
                fprintf(stderr, "Unsupported data type at loadPeekedMessage\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
            int rv = count;
            if(retSrc != nullptr) {
                *retSrc = (*it)->src_;
            }
            delete *it;
            peeked_.erase(it);
            fprintf(stderr, "loadPeekedMessage returning: %d, size: %lu\n", rv, peeked_.size());
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
