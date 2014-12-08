//============================================================================
// Name        : Galois_KD_tree.cpp
// Author      : Kathryn McArdle
//============================================================================

#include <iostream>
#include <cstdlib>
#include <vector>
#include <utility>
#include <algorithm>

#include "Galois/Galois.h"
#include "Galois/Graph/FirstGraph.h"
#include "Galois/Statistic.h"

using namespace std;

struct KDNode {
	double* pt;
	int idx;
	int dim;
	bool isLeftChild;

	KDNode(double* pt, int idx) {
		this->pt = pt;
		this->idx = idx;
		dim = -1;
		isLeftChild = true;
	}
};

struct CompareNodes {
	int dim;
	CompareNodes(int dim) : dim(dim) {}

	bool operator() (const KDNode& i, const KDNode& j) {
		return (i.pt[dim] < j.pt[dim]);
	}
};

typedef Galois::Graph::FirstGraph<KDNode,void,true> Graph;
typedef Graph::GraphNode GNode;
typedef pair<GNode, GNode> Edge;

struct P_buildTree {
	int gnode_parent_idx;
	int kd_lo_idx;
	int kd_hi_idx;
	int dim;
	bool isLeftChild;
	vector<KDNode>& kdnodes;
	Graph* tree_ptr;
	GNode* gnodes;

	P_buildTree(int gnode_parent_idx, int kd_lo_idx, int kd_hi_idx, int dim, bool isLeftChild, vector<KDNode>& kdnodes, Graph* tree_ptr, GNode* gnodes) : kdnodes(kdnodes) {
		this->gnode_parent_idx = gnode_parent_idx;
		this->kd_lo_idx = kd_lo_idx;
		this->kd_hi_idx = kd_hi_idx;
		this->dim = dim;
		this->isLeftChild = isLeftChild;
		this->tree_ptr = tree_ptr;
		this->gnodes = gnodes;
	}


};

/* data in file was stored in binary format. Source: Chen */
bool read_data_from_file(double **data, char *filename, int N, int D) {
  FILE *fp = NULL;
  if (!(fp = fopen(filename, "rb"))) {
  	cout << filename << " didn't open" << endl;
  	return false;
  }

  int i;
  int num_in, num_total;
  for (i = 0; i < N; i++)
    {
      num_total = 0;
      while (num_total < D)
        {
          num_in = fread(data[i]+num_total, sizeof(double), D, fp);
          num_total += num_in;
        }
    }

  fclose(fp);
  return true;
}

int main(int argc, char **argv) {
	char* datafile = argv[1];
	cout << "Results for " << datafile << "data:" << endl;
	int D = atoi(argv[2]);
	int N = atoi(argv[3]);
	int k = atoi(argv[4]);

	/* Read in data: */
	clock_t data_read_start = clock();
	double** data;
	data = (double**) malloc(N*sizeof(double *));
	for (int i = 0; i < N; ++i) {
		data[i] = (double*) malloc(D*sizeof(double));
	}
	if (!read_data_from_file(data, datafile, N, D)) {
		printf("error reading data!\n");
		exit(1);
	}
	clock_t data_read_end = clock();
	double data_read_time = ((double) (data_read_end - data_read_start)) / CLOCKS_PER_SEC;
	printf("time to read data: %f\n", data_read_time);


	/* Convert to vector of KDNodes and insert into graph: */
	clock_t points_start = clock();
	Graph kdtree;
	Graph* tree_ptr = &kdtree;
	vector<KDNode> kdnodes; // vector of kdtree nodes, NOT in indexed order (starts in order, but will be resorted)
	kdnodes.reserve(N);
	GNode* gnodes; // array of graph nodes, IN indexed order: gnodes[0] corresponds to the first data point in the file
	gnodes = (GNode*) malloc(N*sizeof(GNode));
	for (int i = 0; i < N; ++i) {
		kdnodes.emplace_back(data[i], i);
		gnodes[i] = kdtree.createNode(kdnodes[i]);
		kdtree.addNode(gnodes[i]);
	}
	clock_t points_end = clock();
	double points_time = ((double) (points_end - points_start)) / CLOCKS_PER_SEC;
	printf("time to fill graph with data points (no edges): %f\n", points_time);

	// test graph...
		KDNode& node_last = kdtree.getData(gnodes[N-1]);
		for (int i = 0; i < D; ++i) {
//			cout << node_last.pt[i] << " ";
			if (node_last.pt[i] != data[N-1][i]) {
				cout << node_last.pt[i] << " != " << data[N-1][i] << endl;
			}
		}
		cout << endl;
		KDNode& random = kdtree.getData(gnodes[378]);
		for (int i = 0; i < D; ++i) {
//			cout << random.pt[i] << " ";
			if (random.pt[i] != data[378][i]) {
				cout << random.pt[i] << " != " << data[378][i] << endl;
			}
		}
		cout << endl;

	/* Build KDTree */
	clock_t tree_start = clock();
	sort(kdnodes.begin(), kdnodes.end(), CompareNodes(0));
	int median = N/2;
	int root_node_idx = kdnodes[median].idx; // corresponds to the root's index in gnodes: gnodes[root_node_idx]
	KDNode& root = kdtree.getData(gnodes[root_node_idx]);
	root.dim = 0;
	vector<P_buildTree> worklist;
	worklist.emplace_back(root_node_idx, 0, median, 1, true, kdnodes, tree_ptr, gnodes);
	worklist.emplace_back(root_node_idx, median+1, N, 1, false, kdnodes, tree_ptr, gnodes);
	while (!worklist.empty()) {
		P_buildTree curr = worklist.back();
		worklist.pop_back();
		// base cases:
		if (curr.kd_hi_idx == curr.kd_lo_idx) { continue; }
		if ((curr.kd_hi_idx - curr.kd_lo_idx) == 1) {
			int gnode_idx = kdnodes[curr.kd_lo_idx].idx;
			kdtree.addEdge(gnodes[curr.gnode_parent_idx], gnodes[gnode_idx]);
			KDNode& n = kdtree.getData(gnodes[gnode_idx]);
			n.isLeftChild = curr.isLeftChild;
			n.dim = curr.dim;
		}

		sort(kdnodes.begin()+curr.kd_lo_idx, kdnodes.begin()+curr.kd_hi_idx, CompareNodes(curr.dim));
		int med_idx = curr.kd_lo_idx + (curr.kd_hi_idx-curr.kd_lo_idx)/2;
		int gnode_idx = kdnodes[med_idx].idx;
		kdtree.addEdge(gnodes[curr.gnode_parent_idx], gnodes[gnode_idx]);
		KDNode& n = kdtree.getData(gnodes[gnode_idx]);
		n.isLeftChild = curr.isLeftChild;
		n.dim = curr.dim;
		worklist.emplace_back(kdnodes[med_idx].idx, curr.kd_lo_idx, med_idx, (curr.dim+1)%D, true, curr.kdnodes, curr.tree_ptr, curr.gnodes);
		worklist.emplace_back(kdnodes[med_idx].idx, med_idx+1, curr.kd_hi_idx, (curr.dim+1)%D, false, curr.kdnodes, curr.tree_ptr, curr.gnodes);
	}
	clock_t tree_end = clock();
	double tree_time = ((double) (tree_end - tree_start)) / CLOCKS_PER_SEC;
	printf("time to build tree: %f\n", tree_time);

	// test tree:
//	KDNode& root_post = kdtree.getData(gnodes[root_node_idx]);
//	printf("root node = %d\n", root_post.idx);
//	for (int i = 0; i < D; ++i) {
//			cout << root_post.pt[i] << " ";
//	}
//	cout << endl;
//
//	for (auto edge : kdtree.out_edges(gnodes[root_node_idx])) {
//		GNode dest = kdtree.getEdgeDst(edge);
//		KDNode& dest_kd = kdtree.getData(dest);
//		if (dest_kd.isLeftChild) { printf("root's left child"); }
//		else { printf("root's right child"); }
//		printf("= %d\n", dest_kd.idx);
//		for (int i = 0; i < D; ++i) {
//			cout << dest_kd.pt[i] << " ";
//		}
//		cout << endl;
//	}

	free(data);
	free(gnodes);
}