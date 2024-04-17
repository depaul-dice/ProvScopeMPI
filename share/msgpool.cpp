#include "msgpool.h"

using namespace std;


bool request_id::operator==(request_id const& rhs) const {
    return req == rhs.req && src == rhs.src && tag == rhs.tag && comm_id == rhs.comm_id && buf == rhs.buf;
}

bool request_id::operator!=(request_id const& rhs) const {
    return !(*this == rhs);
}

shared_ptr<request_id> MRU::get(MPI_Request *req) {
    auto it = mru_map.find(req);
    if (it == mru_map.end()) {
        return nullptr;
    }
    shared_ptr<request_id> rv = *(it->second);
    return rv;
}

// on success it returns 0 and on failure it returns 1
int MRU::put(MPI_Request *req, shared_ptr<request_id> id) {
    auto it = mru_map.find(req);
    if(it != mru_map.end()) {
        return 1;
    }
    mru_list.push_back(id);
    mru_map[req] = --mru_list.end();
    return 0;
}

// on success it returns 0 and on failure it returns 1
int MRU::erase(MPI_Request *req) {
    auto it = mru_map.find(req);
    if (it != mru_map.end()) {
        mru_list.erase(it->second);
        mru_map.erase(it);
        return 0;
    }
    return 1;
}

int recv_manager::mpi_irecv(MPI_Request *req, int src, int tag, int comm_id, void *buf, int count, MPI_Datatype datatype) {
    shared_ptr<request_id> id = make_shared<request_id>(req, src, tag, comm_id, buf, datatype, count);
    return mru.put(req, id);
}

/* static void swpdata(void *a, void *b, MPI_DataType datatype, int count) { */
/*     char *tmp = new char[count * datatype.size()]; */
/*     memcpy(tmp, a, count * datatype.size()); */
/*     memcpy(a, b, count * datatype.size()); */
/*     memcpy(b, tmp, count * datatype.size()); */
/*     delete[] tmp; */
/* } */

static inline int __swpdata(shared_ptr<request_id> src, shared_ptr<request_id> dst) {
    if(src->datatype != dst->datatype || src->tag != dst->tag || src->comm_id != dst->comm_id) {
        return 1;
    }

    if(src->count != dst->count) {
        DEBUG("count mismatch, we might need to rethink this");
        return 1;
    }

    int size;
    MPI_Type_size(src->datatype, &size);

    char *tmpdata = new char[src->count * size];
    memcpy(tmpdata, src->buf, src->count * size);
    memcpy(src->buf, dst->buf, src->count * size);
    memcpy(dst->buf, tmpdata, src->count * size);
    delete[] tmpdata;
    return 0; 
}

int recv_manager::mpi_testwait(MPI_Request *req, MPI_Status *status, int src) {
    shared_ptr<request_id> id = mru.get(req);
    MPI_ASSERT(id != nullptr);
    
    // if the request has the right source, we don't need to do anything, delete it from MRU 
    if(src == status->MPI_SOURCE) {
        mru.erase(req);
        return 0;
    } else {
        // let's search through the MRU if there's any request with the right source and swap data with ours
        for(auto it = mru.mru_list.begin(); it != mru.mru_list.end(); it++) {
            // let's iterate through the list and see if there's request that has been received
            if((*it)->src == src || (*it)->src == MPI_ANY_SOURCE) {
                // let's test it and check if they have the right source
                MPI_Status tmpstat;
                int flag, ret;
                if(original_MPI_Test == nullptr) {
                    original_MPI_Test = (int (*)(MPI_Request *, int *, MPI_Status *)) dlsym(RTLD_NEXT, "MPI_Test");
                }
                ret = original_MPI_Test((*it)->req, &flag, &tmpstat);
                if(flag && status->MPI_SOURCE == src && status->MPI_TAG == tmpstat.MPI_TAG \
                        && !__swpdata(id, *it)) {
                    mru.erase(req);
                    return 0;
                }
            }
        }
        DEBUG("No matching request found in MRU");
        MPI_ASSERT(0);
        return 1;
    }
}
