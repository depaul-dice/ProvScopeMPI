
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
pair<string, loopNode *> parseCluster(
        Agraph_t* subgraph, const std::string& prefix = "") {
    pair<string, loopNode *> ret;
    string subgraphName = agnameof(subgraph);
    MPI_ASSERT(subgraphName.rfind("root", 0) != 0);
    
    ret.first = subgraphName;
    /* bool flag = false; */
    /* if(subgraphName == "hypre_NewCommPkgCreate_core") { */
    /*     flag = true; */
    /* } */

    unordered_set<string> dummyset;
    string rootname = "root_" + subgraphName;;
    loopNode *root = new loopNode(rootname, dummyset);
    ret.second = root;

    // Iterate over nodes in the subgraph, and create a tree here
    unordered_map<string, unordered_set<string>> edges;
    unordered_map<string, loopNode *> nodes;
    nodes["root"] = root;

    char *label;
    string headname;
    for (Agnode_t* node = agfstnode(subgraph); node; node = agnxtnode(subgraph, node)) {
        /* logger << prefix << "  "; */
        /* if(flag) printNodeInfo(node, subgraph); */
        label = agget(node, (char *)"label");
        MPI_ASSERT(label && *label);
        if(string(agnameof(node)).rfind("root", 0) == 0) {
            // the name of the node starts with root_
            // this is the root node
            headname = "root";
            MPI_ASSERT(!strcmp(label, "root"));
        } else {
            headname = agnameof(node);
            /* if(flag) logger << "\nheadname: " << headname << endl; */
            string labelstr = string(label);
            unordered_set<string> content;
            splitNinsert(labelstr, "\\l", content);
            nodes[headname] = new loopNode(headname, content); 
            /* int rank; */
            /* MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
            /* if(flag && rank == 0) cerr << "\nnew node!\n" << *(nodes[headname]) << endl; */
        }

        for(Agedge_t *edge = agfstout(subgraph, node); edge; edge = agnxtout(subgraph, edge)) {
            string child = agnameof(aghead(edge));
            /* if(flag) logger << "child: " << child << endl << endl; */
            if(edges.find(headname) == edges.end()) {
                edges[headname] = unordered_set<string>();
            }
            edges[headname].insert(child);
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
    /* cerr << "Graph name: " << agnameof(graph) << endl; */
    /* cerr << "Graph has nodes: " << (agnode(graph, NULL, 0) != nullptr) << endl; */

    // Iterate over all subgraphs (clusters only)
    pair<string, loopNode *> root;
    while((graph = agread(file, nullptr)) != nullptr) {
        root = parseCluster(graph);
        ret[root.first] = root.second;
        agclose(graph);
    }
    /* cerr << "Parsed " << ret.size() << " clusters" << endl; */

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
