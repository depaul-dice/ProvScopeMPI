
#include "messagePool.h"

using namespace std;

MessageBuffer::MessageBuffer(
        MPI_Request *request, 
        void * const buf, 
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
    realBuf_ = static_cast<char *>(calloc(msgSize, sizeof(char)));
    if(timestamp_ == numeric_limits<unsigned long>::max()) {
        fprintf(stderr, "timestamp overflow\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
}

MessageBuffer::~MessageBuffer() {
    if(realBuf_ != nullptr) {
        free(realBuf_);
        /*
         * set it to nullptr to avoid double free
         */
        realBuf_ = nullptr;
    }
}

MessagePool::~MessagePool() {
    for(auto it = pool_.begin(); it != pool_.end(); it++) {
        //fprintf(stderr, "deleting msgBuffer: %p\n", it->second);
        delete it->second;
    }
    for(auto it = peeked_.begin(); it != peeked_.end(); it++) {
        delete *it;
    }
}

char *MessagePool::addMessage(
        MPI_Request *request, 
        void * const buf, 
        MPI_Datatype dataType, 
        int count,
        int tag,
        MPI_Comm comm,
        int src,
        bool isSend) {
    // the 2 lines below are simply for the debugging purpose, 
    // delete it for the prod
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(dataType != MPI_INT
            && dataType != MPI_CHAR
            && dataType != MPI_BYTE
            && dataType != MPI_DOUBLE
            && dataType != MPI_LONG_LONG_INT) {
        unsupportedDatatype(rank, __LINE__, dataType);
    }
    /* if(isSend == false) { */
    /*     fprintf(stderr, "addMessage at rank: %d, request: %p\n", */ 
    /*             rank, */ 
    /*             request); */
    /* } */
    if(pool_.find(request) != pool_.end()) {
        MPI_ASSERT(pool_[request]->request_ == request);
        /* DEBUG("deleting msgBuffer at %s: %p, at rank: %d\n", __func__, pool_[request], rank); */
        delete pool_[request];
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
    /* fprintf(stderr, "created msgBuffer: %p, at rank: %d\n", pool_[request], rank); */
    return pool_[request]->realBuf_;
}

MessageBuffer *MessagePool::peekMessage(
        MPI_Request *request) {
    /*
    if(pool_.find(request) == pool_.end()) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        fprintf(stderr, "message not found for request at peekMessage: %p \
                at rank: %d\n", 
                request, rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    */
    if(pool_.find(request) == pool_.end()) {
        return nullptr;
    }
    return pool_[request];
}

string MessagePool::loadMessage(
        MPI_Request *request, MPI_Status *status) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if(pool_.find(request) == pool_.end()) {
        fprintf(stderr, "message not found for request at loadMessage: %p \
                at rank: %d\n", 
                request, rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    MessageBuffer *msgBuf = pool_[request];
    MPI_ASSERT(request == msgBuf->request_);

    if(msgBuf->isSend_ == true) {
        /*
         * update the status for sends here
         */
        if(status != MPI_STATUS_IGNORE) {
            status->MPI_SOURCE = msgBuf->src_;
            status->MPI_TAG = msgBuf->tag_;
            status->MPI_ERROR = MPI_SUCCESS;
            int size;
            MPI_Type_size(msgBuf->dataType_, &size);
            status->_ucount = msgBuf->count_ * size;
        }

        return "";
    }

    MPI_ASSERT(msgBuf->realBuf_ != nullptr);
    //cerr << "msgBuf->realBuf_: " << (char *)(msgBuf->realBuf_) << endl;
    string msg = msgBuf->realBuf_;
    vector<string> tokens = parse(msg, '|');
    if(tokens.size() != msgBuf->count_ + 2) {
        string typeName = convertDatatype(msgBuf->dataType_); 
        fprintf(stderr, "at loadMessage tokens.size(): %lu, count: %d, isSend: %d, datatype: %s\nAborting at rank: %d for request: %p, realBuf: %p, src: %d\nmessage: %s\n", 
                tokens.size(), 
                msgBuf->count_, 
                msgBuf->isSend_,
                typeName.c_str(),
                rank, 
                request,
                msgBuf->realBuf_,
                msgBuf->src_,
                msg.c_str());
        throw runtime_error("tokens.size() != msgBuf->count_ + 2");
    }
    int size;
    MPI_Type_size(msgBuf->dataType_, &size);
    MPI_ASSERT(stoi(tokens.back()) == size);

    convertMsgs2Buf(
            msgBuf->buf_, 
            msgBuf->dataType_, 
            msgBuf->count_, 
            tokens,
            __LINE__,
            rank);
    if(status != MPI_STATUS_IGNORE) {
        status->MPI_SOURCE = msgBuf->src_;
        status->MPI_TAG = msgBuf->tag_;
        status->MPI_ERROR = MPI_SUCCESS;

        int size;
        MPI_Type_size(msgBuf->dataType_, &size);
        status->_ucount = msgBuf->count_ * size;
    }
    if(tokens[tokens.size() - 2].size() == 0) {
        fprintf(stderr, "empty message returned: %s\n", msg.c_str());
    }
    return tokens[tokens.size() - 2]; 
}

bool MessagePool::isSend(MPI_Request *request) {
    if(pool_.find(request) == pool_.end()) {
        throw runtime_error("request not found in pool_ at isSend");
    }
    return pool_[request]->isSend_;
}

char *MessagePool::getRealBuf(MPI_Request *request) {
    if(pool_.find(request) == pool_.end()) {
        throw runtime_error("request not found in pool_ at getBuf");
    }
    return pool_[request]->realBuf_;
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
            //fprintf(stderr, "peeked message found: %lu\n", i);

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
    int ind = 0;
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
            } else if(dataType == MPI_DOUBLE) {
                for(int i = 0; i < count; i++) {
                    ((double *)buf)[i] = stod(tokens[i]);
                }
            } else if(dataType == MPI_LONG_LONG_INT) {
                for(int i = 0; i < count; i++) {
                    ((long long int *)buf)[i] = stoll(tokens[i]);
                }
            } else {
                int rank;
                MPI_Comm_rank(MPI_COMM_WORLD, &rank);
                unsupportedDatatype(rank, __LINE__, dataType);
            }
            if(retSrc != nullptr) {
                *retSrc = (*it)->src_;
            }
            /* delete *it; */
            peeked_.erase(it);
            //fprintf(stderr, "loadPeekedMessage returning: %d, size: %lu\n", rv, peeked_.size());
            return ind;
        }
        ind++;
    }
    // if not found, let users call MPI_Recv or MPI_Irecv
    return -1;
}

void MessagePool::deleteMessage(MPI_Request *request) {
    MPI_ASSERT(pool_.find(request) != pool_.end());
    delete pool_[request];
    pool_.erase(request);
}
