#pragma once

#include <mpi.h>
#include <unordered_map>
#include <pair>

alias MsgType = MPI_Datatype;

class Msg {
public:
    Msg() = delete;
    Msg(const Msg& other) = delete;
    Msg& operator =(const Msg& other) = delete;
    ~Msg() = default;

    Msg(int sender, MsgType type, void *content, unsigned long timestamp);

    bool isErrorMsg() const;
private:
    int sender_;
    MsgType type_;
    void *content_; // freeing of this message is not our responsibility
    unsigned long timestamp_;
};

class MsgBuffer {
public:
    MsgBuffer() = delete;
    MsgBuffer(const MsgBuffer& other) = delete;
    MsgBuffer& operator =(const MsgBuffer& other) = delete;
    ~MsgBuffer() = default;

    MsgBuffer(const int rank);

    void addMsg(const int sender, const void *content, const unsigned long timestamp);
    void deleteMsg(const int sender, const unsigned long timestamp);
    Msg getMsgByTimestamp(const int sender, const unsigned long timestamp); // does the binary search and looks for the message

private:
    // message consists of triple: (sender rank, message content, timestamp);
    // we can create a unordered_map for each sender
    // we can store a pair of (message content, timestamp) for each message
    int rank_; // this is to make sure who I am
    std::unordered_map<int, std::deque<Msg>> buffer_;

};
