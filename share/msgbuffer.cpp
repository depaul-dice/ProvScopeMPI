using namespace std;

Msg::Msg(int sender, MsgType type, void *content, unsigned long timestamp) : sender_(sender), type_(type), content_(content), timestamp_(timestamp) {}

bool Msg::isErrorMsg() const {
    return sender_ == -1 \
        && type_ == MPI_DATATYPE_NULL \
        && content_ == nullptr \
        && timestamp_ == 0
}

MsgBuffer::MsgBuffer(int rank) : rank_(rank), buffer_() {}

void MsgBuffer::addMsg(int sender, MsgType type, void *content, unsigned long timestamp) {
    Msg msg(sender, type, content, timestamp);
    buffer_[sender].push_back(msg);
}

void MsgBuffer::deleteMsg(int sender, unsigned long timestamp) {
    auto buffer = buffer_.find(sender);
    if(buffer == buffer_.end()) {
        cerr << "no such timestamp existed, so nothing was deleted\nsender: " \
            << sender \
            << "timestamp: " \
            << timestamp \
            << endl;        
        return;
    }
}

Msg MsgBuffer::getMsgByTimestamp(int sender, unsigned long timestamp) {
    auto tmp = buffer_.find(sender);
    if(tmp == buffer_.end()) {
        cerr << "no such timestamp existed, so nothing was fetched\nsender: " \
            << sender \
            << "timestamp: " \
            << timestamp \
            << endl;        
        return Msg(-1, MPI_DATATYPE_NULL, nullptr, 0);
    }
    auto buffer = tmp->second;
    unsigned long l = 0, r = buffer.size() - 1;
    while(l <= r) {
        unsigned long m = l + (r - l) / 2;
        if(buffer[m].timestamp_ == timestamp) {
            return buffer[m];
        }
        if(buffer[m].timestamp_ < timestamp) {
            l = m + 1;
        } else {
            r = m - 1;
        }
    }
    cerr << "no such timestamp existed, so nothing was fetched\nsender: " \
        << sender \
        << "timestamp: " \
        << timestamp \
        << endl;
    return Msg(-1, MPI_DATATYPE_NULL, nullptr, 0); 
}
