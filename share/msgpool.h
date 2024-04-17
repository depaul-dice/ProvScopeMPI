#ifndef MSGPOOL_H
#define MSGPOOL_H
#include <mpi.h>
#include <unordered_map>
#include <string>
#include <list>
#include <set>
#include <memory>


#include "mpirecordreplay.h"
#include "tools.h"

class request_id {
public:
    MPI_Request *req;
    int src;
    int tag;
    int comm_id;
    void *buf;
    MPI_Datatype datatype;
    int count;

    request_id(MPI_Request *req, int src, int tag, int comm_id, void *buf, MPI_Datatype datatype, int count) : req(req), src(src), tag(tag), comm_id(comm_id), buf(buf), datatype(datatype), count(count) {}
    request_id(const request_id&) = default;
    request_id& operator=(const request_id&) = default;
    ~request_id() = default;

    /* std::string to_string(); */
    bool operator==(request_id const& rhs) const;
    bool operator!=(request_id const& rhs) const;
};

class MRU {
private:
    std::list<std::shared_ptr<request_id>> mru_list;
    std::unordered_map<MPI_Request *, std::list<std::shared_ptr<request_id>>::iterator> mru_map;

public:
    MRU() = default;
    MRU(const MRU&) = delete;
    MRU& operator=(const MRU&) = delete;
    ~MRU() = default;

    std::shared_ptr<request_id> get(MPI_Request *req);
    int put(MPI_Request *req, std::shared_ptr<request_id> id);
    int erase(MPI_Request *req);

    friend class recv_manager;
};

class recv_manager {
private:
    MRU mru;
    
public:
    recv_manager() = default;
    recv_manager(const recv_manager&) = delete;
    recv_manager& operator=(const recv_manager&) = delete;
    ~recv_manager() = default;

    // I add the request in the pending request here
    int mpi_irecv(MPI_Request *req, int src, int tag, int comm_id, void *buf, int count, MPI_Datatype datatype);
    // if I test or wait a request, I need to check if it is the right one
    int mpi_testwait(MPI_Request *req, MPI_Status *status, int src);

};


#endif
