
#include "loops.h"

using namespace std;
extern Logger logger;
/* loopNode::loopNode(node *entry, set<node *> &nodes) */ 
    /* : entry(entry), parent(nullptr), nodes(nodes), children() { */
/* } */
// below is for the root node
loopNode::loopNode(
        string &entry, unordered_set<string> &nodes) 
    : entry(entry), parent(nullptr), nodes(nodes), exclusives(), children() {
}

loopNode::~loopNode() {
    for(auto &c: children) {
        delete c;
    }
}

void loopNode::addParent(loopNode *p) {
    this->parent = p;
}

void loopNode::addChild(loopNode *c) {
    this->children.insert(c);
}

// highly doubt this function is needed
void loopNode::addRoot(loopNode *r) {
    r->children.insert(this);
    /* r->nodes.insert(nodes.begin(), nodes.end()); */
}

void loopNode::print(
        ofstream& file, const string& funcname) {
    file << "subgraph " << funcname << "{\n"\
        << "label=\"" << funcname << "\";\n"\
        << "root_" << funcname << "[label=\"root\"];\n";
    string edges = "";
    for(auto &c: this->children) {
        edges += "root_" + funcname + " -> \"" + c->entry + "\";\n";
        edges += c->__print(file);
    }
    file << edges << "}\n\n";
}

void loopNode::print(
        ostream& os, const string& funcname) {
    // this should be the root node
    os << "subgraph " << funcname << "{\n";
    string edgeinfo = __print(os);
    os << edgeinfo << "}\n\n";
}

string loopNode::__print(ofstream& file) {
    string edges = "";
    /* string entryname = replaceall(entry->name, ":", "|"); */
    file << '\"' << entry << "\" [label=\"" << entry << "\\l";
    for(auto &n: nodes) {
        /* string nname = replaceall(n->name, ":", "|"); */
        file << n << "\\l";
    }
    file << "\"];\n";
    for(auto &c: children) {
        /* string centryname = replaceall(c->entry->name, ":", "|"); */
        edges += '\"' + entry + "\" -> \"" + c->entry + "\";\n";
        edges += c->__print(file);
    }
    return edges;
}

string loopNode::__print(ostream& os) {
    // print infos about node first
    os << this->entry << '{' << endl;
    for(auto &n: nodes) {
        os << '\t' << n << endl;
    }
    os << '}' << endl;
    string edges = "";
    for(auto &c:children) {
        edges += this->entry + " -> " + c->entry + ";\n";
    }
    for(auto &c: children) {
        edges += c->__print(os);
    }
    return edges;
}

void printEdgeInfo(
        Agnode_t* node, Agraph_t* subgraph) {
    char *label;
    for (Agedge_t* edge = agfstout(subgraph, node); edge; edge = agnxtout(subgraph, edge)) {
        label = agget(edge, (char *)"label");
        logger << "  -> " << agnameof(aghead(edge)) << std::endl;
        if(label && *label) {
            logger << "    Label: " << label << std::endl;
        }
    }
}

void printNodeInfo(
        Agnode_t* node, Agraph_t* subgraph) {
    char* label = agget(node, (char *)"label");
    // we should create a node here
    logger << "Node: " << agnameof(node) << endl;
    if(label && *label) {
        logger << "  Label: " << label << endl;
    }
    // this should get the children
    printEdgeInfo(node, subgraph);
}

// here you need to manipulate ret to get the correct values
pair<string, loopNode *> parseGraph(
        Agraph_t* graph, const std::string& prefix = "") {
    pair<string, loopNode *> ret;
    string graphName = agnameof(graph);
    MPI_ASSERT(graphName.rfind("root", 0) != 0);
    
    ret.first = graphName;

    unordered_set<string> dummySet;
    string rootName = "root_" + graphName;;
    loopNode *root = new loopNode(rootName, dummySet);
    ret.second = root;

    // Iterate over nodes in the subgraph, and create a tree here
    unordered_map<string, unordered_set<string>> edges;
    unordered_map<string, loopNode *> nodes;
    nodes["root"] = root;

    char *label;
    string headName;
    unordered_map<string, string> headNameConversions;
    for (
            Agnode_t* node = agfstnode(graph); 
            node != nullptr; 
            node = agnxtnode(graph, node)) {
        /* logger << prefix << "  "; */
        /* if(flag) printNodeInfo(node, subgraph); */
        label = agget(node, (char *)"label");
        MPI_ASSERT(label && *label);
        headName = agnameof(node);
        if(headName == "root") {
            // the name of the node starts with root_
            // this is the root node
            MPI_ASSERT(!strcmp(label, "root"));
        } else {
            string lastHeadName = headName;
            string labelstr = string(label);
            unordered_set<string> content;
            headName = splitNinsert(labelstr, "\\l", content);
            MPI_ASSERT(headNameConversions.find(lastHeadName) == headNameConversions.end());
            headNameConversions[lastHeadName] = headName;
            /* DEBUG0("headName: %s\n", headName.c_str()); */
            nodes[headName] = new loopNode(headName, content); 
            /* int rank; */
            /* MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
            /* if(flag && rank == 0) cerr << "\nnew node!\n" << *(nodes[headname]) << endl; */
        }
    }
    for(
            Agnode_t *node = agfstnode(graph); 
            node != nullptr; 
            node = agnxtnode(graph, node)) {
        string src;
        if(headNameConversions.find(agnameof(node)) != headNameConversions.end()) {
            src = headNameConversions[agnameof(node)];
        } else {
            src = agnameof(node);
        }
        for(
                Agedge_t *edge = agfstout(graph, node); 
                edge != nullptr; 
                edge = agnxtout(graph, edge)) {
            string dest;
            if(headNameConversions.find(agnameof(aghead(edge))) != headNameConversions.end()) {
                dest = headNameConversions[agnameof(aghead(edge))];
            } else {
                dest = agnameof(aghead(edge));
            }
            if(edges.find(src) == edges.end()) {
                edges[src] = unordered_set<string>();
            }
            edges[src].insert(dest);
        }
    }

    // then connect the nodes from edges
    string parentname;
    loopNode *curr, *parent, *child;
    for(auto &e: edges) {
        parentname = e.first; 
        MPI_ASSERT(nodes.find(parentname) != nodes.end());
        curr = nodes[parentname];
        for(auto &childname: e.second) {
            MPI_ASSERT(nodes.find(childname) != nodes.end());
            child = nodes[childname]; 
            curr->addChild(child);
            child->addParent(curr);
        }
    }
    return ret;
}

unordered_map<string, loopNode *> parseDotFile(const std::string& filename) {
    unordered_map<string, loopNode *> ret; 

    // Open the DOT file and create the graph
    FILE* file = fopen(filename.c_str(), "r");
    MPI_ASSERT(file != nullptr);

    Agraph_t* graph = agread(file, nullptr);
    MPI_ASSERT(graph != nullptr);

    // Iterate over all subgraphs (clusters only)
    pair<string, loopNode *> root;
    while((graph = agread(file, nullptr)) != nullptr) {
        root = parseGraph(graph);
        ret[root.first] = root.second;
        agclose(graph);
    }

    fclose(file);
    return ret;
}

unordered_set<string> loopNode::fixExclusives(void) {
    if(exclusives.size() == 0) {
        exclusives = nodes;
        for(auto &c: children) {
            exclusives.erase(c->entry);
        }
    }
    for(auto &c: children) {
        if(nodes.size() > 0) {
            nodes += c->fixExclusives();
        } else {
            c->fixExclusives();
        }
    }
    return nodes;
}

ostream& operator << (
        ostream& os, loopNode& ln) {
    os << ln.entry << '{' << endl;
    for(auto &n: ln.nodes) {
        os << '\t' << n << endl;
    }
    os << '}' << endl;
    return os;
}
