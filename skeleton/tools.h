#ifndef TOOLS_H
#define TOOLS_H

#include <vector>
#include <map>
#include <set>
#include <numeric>
#include <iostream>

namespace Tools {
    const std::string SAFEDELIM = "::";

    template<typename E> 
    void print(std::ostream& os, std::vector<E>& v) {
        for(auto e: v) {
            os << e << ",";
        }
        os << std::endl;
    }

    template<typename K, typename V>
    void print(std::ostream& os, std::map<K, V>& m) {
        os << "{";
        for(auto it = m.begin(); it != m.end(); it++) {
            os << it->first << ": " << it->second;
            if(std::next(it) != m.end()) {
                os << std::endl;
            }
        }
        os << "}";
    }

    template<typename K, typename V>
    bool isContained(std::map<K, V> const& m, K& key) {
        if(m.find(key) == m.end()) {
            return false;
        } else {
            return true;
        }
    }

    template<typename K, typename V>
    bool isContained(std::map<K, V> const& m, const K& key) {
        if(m.find(key) == m.end()) {
            return false;
        } else {
            return true;
        }
    }

    bool isContained(std::map<long, long> const& m, long key);

    template<typename T> 
    bool isContained(std::set<T> const& s, T& key) {
        if(s.find(key) == s.end()) return false;
        else return true;
    }

    template<typename T>
    bool isContained(std::set<T> const& s, const T& key) {
        if(s.find(key) == s.end()) return false;
        else return true;
    }

    template<typename T>
    bool isContained(std::set<T> const& s, std::set<T> const& t) {
        // s <= t
        for(auto e: s) {
            if(!isContained(t, e)) {
                return false;
            }
        }
        return true;
    }

    template <typename T>
    std::ostream& operator <<(std::ostream& os, const std::vector<T>& v) {
        os << "[";
        for(size_t i = 0; i < v.size(); i++) {
            os << v[i];
            if(i < v.size() - 1) {
                os << "\n";
            }
        }
        os << "]";
        return os;
    }

    template <typename T>
    std::ostream& operator <<(std::ostream& os, const std::vector<T *>& v) {
        os << "[";
        for(size_t i = 0; i < v.size(); i++) {
            os << *(v[i]);
            if(i < v.size() - 1) {
                os << "\n";
            }
        }
        os << "]";
        return os;
    }

    template <typename T>
    std::ostream& operator <<(std::ostream& os, const std::set<T>& s) {
        os << "{\n";
        for(auto it = s.begin(); it != s.end(); it++) {
            os << *it << std::endl;
        }
        os << "}";
        return os;
    }

    template <typename K, typename V>
    std::ostream& operator <<(std::ostream& os, const std::map<K, V>& m) {
        os << "{\n";
        for(auto it = m.begin(); it != m.end(); it++) {
            os << it->first << ": " << it->second << std::endl;
        }
        os << "\n}";
        return os;
    }

    template <typename T>
    std::ostream& operator <<(std::ostream& os, const std::pair<T, T>& p) {
        os << "(" << p.first << ", " << p.second << ")";
        return os;
    }

    bool stob(std::string str); 
 
    template <typename T>
    std::vector<T *> setDiff(std::vector<T *>& a, std::vector<T *>& b) {
        std::vector<T *> answer;
        bool found;
        for(unsigned i = 0; i < a.size(); i++) {
            found = false;
            for(unsigned j = 0; j < b.size(); j++) {
                if(*(a[i]) == *(b[j])) {
                    found = true;
                    break;
                }
            }
            if(!found) answer.push_back(a[i]);
        }
        return answer;
    } 

    template <typename T>
    std::set<T> operator - (const std::set<T>& s, const T& element) {
        std::set<T> res = s;
        res.erase(element);
        return res;
    }

    template <typename T>
    std::set<T> operator - (const std::set<T>& s, const std::set<T>& t) {
        std::set<T> res = s;
        for(auto element: t) {
            res.erase(element);
        }
        return res;
    }

    template<typename T>
    std::map<T, std::set<T>> extractEdges(const std::set<T> &sgNodes, std::map<T, std::set<T>>& edges) {
        std::map<T, std::set<T>> res;
        for(auto it = sgNodes.begin(); it != sgNodes.end(); it++) {
            for(auto k = edges[*it].begin(); k != edges[*it].end(); k++) {
                // *it is src, *k is dst
                if(isContained(sgNodes, *k)) {
                    if(!isContained(res, *it)) {
                        res[*it];
                    }
                    res[*it].insert(*k);
                }
            }
        }
        return res;
    }

    template<typename T>
    std::set<T> extractEntryPoints(const std::set<T> &scc, T &originalEntry, std::map<T, std::set<T>> &edges) {
        std::set<T> entryPoints;
        for(auto it = edges[originalEntry].begin(); it != edges[originalEntry].end(); it++) {
            if(isContained(scc, *it)) {
                entryPoints.insert(*it);
            }
        }
        return entryPoints;
    }

    template<typename T>
    bool entryOverlap(T &te, std::map<std::set<T>, T> &visitedSCCs) {
        for(auto it = visitedSCCs.begin(); it != visitedSCCs.end(); it++) {
            if(isContained(it->first, te)) {
                return true;
            }
        }
        return false;
    }

    double average(const std::vector<double>& vec);

    std::string replaceAll(const std::string &str);
}

#endif /*TOOLS_H*/
