#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

struct node_s;

typedef enum
{
	undecided,
	deleted,
	required
} edge_state_e;

typedef struct
{
	unsigned required_degree, deleted_degree;
} node_state_t;

typedef struct
{
	struct node_s* node1, *node2;
	edge_state_e state;
} edge_t;

typedef struct node_s
{
	edge_t** edges;
	unsigned degree, required_degree, deleted_degree;
	
	//for checking connectivity & cycles
	bool visited;
	
	//for checking required cycles
	unsigned dfs_depth;
	struct node_s* dfs_parent;
} node_t;

typedef struct
{
	node_t** nodes;
	edge_t** edges;
	unsigned n, nedges;
} graph_t;

char* readfile(const char* path)
{
	FILE* fp = fopen(path, "r");
	if (!fp) return NULL;
	
	if (fseek(fp, 0, SEEK_END) != 0)
	{
		fclose(fp);
		return NULL;
	}
	long fsize = ftell(fp);
	if ((fsize <= 0)
		|| (fseek(fp, 0, SEEK_SET) != 0))
	{
		fclose(fp);
		return NULL;
	}
	
	char* data = (char*)malloc(fsize + 1);
	if (!data)
	{
		fclose(fp);
		return NULL;
	}
	
	if (fread(data, fsize, 1, fp) != 1)
	{
		fclose(fp);
		free(data);
		return NULL;
	}
	data[fsize] = '\0';
	
	fclose(fp);
	return data;
}

graph_t* graph_from_adj_matrix(bool* adj_matrix, unsigned n)
{
	graph_t* graph = malloc(sizeof(graph_t));
	graph->n = n;
	
	graph->nodes = malloc(graph->n * sizeof(node_t));
	for (int i = 0; i < graph->n; i++)
		graph->nodes[i] = calloc(sizeof(node_t), 1);
	
	//find number of edges
	graph->nedges = 0;
	for (int i = 0; i < graph->n; i++)
		for (int j = 0; j < i; j++)
			if (adj_matrix[graph->n * i + j])
			{
				graph->nodes[i]->degree++;
				graph->nodes[j]->degree++;
				graph->nedges++;
			}
	
	graph->edges = malloc(graph->nedges * sizeof(edge_t));
	for (int i = 0; i < graph->n; i++)
		graph->nodes[i]->edges = calloc(graph->nodes[i]->degree,
										sizeof(edge_t*));
	for (int i = 0; i < graph->nedges; i++)
		graph->edges[i] = calloc(1, sizeof(edge_t));
	
	unsigned* num_edges_assigned = calloc(graph->n, sizeof(unsigned));
	unsigned edge_idx = 0;
	for (int i = 0; i < graph->n; i++)
		for (int j = 0; j < i; j++)
			if (adj_matrix[graph->n * i + j])
			{
				graph->nodes[i]->edges[num_edges_assigned[i]++] =
				graph->edges[edge_idx];
				graph->nodes[j]->edges[num_edges_assigned[j]++] =
				graph->edges[edge_idx];
				graph->edges[edge_idx]->node1 = graph->nodes[i];
				graph->edges[edge_idx]->node2 = graph->nodes[j];
				edge_idx++;
			}
	
	free(num_edges_assigned);
	return graph;
}

//Parses an adjacency matrix to get a graph
graph_t* parse_graph(char* input)
{
	//First, find "N" by parsing until we get a new line and counting the number
	//of 1's and 0's
	unsigned n = 0;
	for (char* cur = input; *cur != '\n'; cur++)
		if (*cur == '0' || *cur == '1')
			n++;
	
	bool* adj_matrix = malloc(n * n * sizeof(bool));
	
	char* cur = input;
	for (int i = 0; i < n * n && *cur != '\0';
		 cur++)
	{
		if (*cur == '0')
			adj_matrix[i++] = false;
		else if (*cur == '1')
			adj_matrix[i++] = true;
	}
	
	graph_t* graph = graph_from_adj_matrix(adj_matrix, n);
	free(adj_matrix);
	return graph;
}

void delete_edge(edge_t* edge)
{
	if (edge->state == undecided)
	{
		edge->node1->deleted_degree++;
		edge->node2->deleted_degree++;
		edge->state = deleted;
	}
}

void require_edge(edge_t* edge)
{
	if (edge->state == undecided)
	{
		edge->node1->required_degree++;
		edge->node2->required_degree++;
		edge->state = required;
	}
}

node_t* other_node(edge_t* edge, node_t* node)
{
	if (edge->node1 == node)
		return edge->node2;
	assert(node == edge->node2);
	return edge->node1;
}

typedef struct
{
	node_t** nodes;
	unsigned start, end;
	unsigned n;
} node_queue_t;

void node_queue_init(node_queue_t* queue, unsigned n)
{
	queue->n = n;
	queue->nodes = calloc(n + 1, sizeof(node_t*));
	queue->start = queue->end = 0;
}

void node_queue_push(node_queue_t* queue, node_t* node)
{
	queue->nodes[queue->start] = node;
	queue->start++;
	if (queue->start == queue->n + 1)
		queue->start = 0;
}

node_t* node_queue_pull(node_queue_t* queue)
{
	node_t* node = queue->nodes[queue->end];
	queue->end++;
	if (queue->end == queue->n + 1)
		queue->end = 0;
	return node;
}

void node_queue_delete(node_queue_t* queue)
{
	free(queue->nodes);
}

bool node_queue_empty(node_queue_t* queue)
{
	return queue->start == queue->end;
}

//Implements rule R1: "If a vertex has only two arcs incident, then both arcs
//are required
bool add_required_edges(graph_t* graph)
{
	bool progress = false;
	
	for (int i = 0; i < graph->n; i++)
	{
		if (graph->nodes[i]->degree - graph->nodes[i]->deleted_degree == 2)
		{
			for (int j = 0; j < graph->nodes[i]->degree; j++)
				if (graph->nodes[i]->edges[j]->state == undecided)
				{
					require_edge(graph->nodes[i]->edges[j]);
					progress = true;
				}
		}
	}
	
	return progress;
}

//Determines if we can get from start to end using only required nodes
bool connected_required(graph_t* graph, node_t* start, node_t* end)
{
	node_queue_t queue;
	node_queue_init(&queue, graph->n);
	
	for (int i = 0; i < graph->n; i++)
		graph->nodes[i]->visited = false;
	
	start->visited = true;
	node_queue_push(&queue, start);
	
	while (!node_queue_empty(&queue))
	{
		node_t* node = node_queue_pull(&queue);
		for (int i = 0; i < node->degree; i++)
		{
			if (node->edges[i]->state != required)
				continue;
			
			node_t* neighbor = other_node(node->edges[i], node);
			
			if (neighbor == end)
			{
				node_queue_delete(&queue);
				return true;
			}
			
			if (!neighbor->visited)
			{
				neighbor->visited = true;
				node_queue_push(&queue, neighbor);
			}
		}
	}
	
	node_queue_delete(&queue);
	return false;
}

//Implements:
//D1: "If a vertex has two required arcs incident, then all undecided arcs
//incident may be deleted"
//D3: "Delete any arc which forms a closed circuit with required arcs, unless
//it completes the Hamiltonian circuit"
//To implement D3, we need to know if we're on the last vertex and, if so,
//what the first vertex is to see if the current edge completes the cycle.
bool add_deleted_edges(graph_t* graph, bool is_last_node, node_t* last_node,
					   node_t* first_node)
{
	bool progress = false;
	for (int i = 0; i < graph->n; i++)
	{
		node_t* node = graph->nodes[i];
		if (node->required_degree == 2)
		{
			for (int j = 0; j < node->degree; j++)
				if (node->edges[j]->state == undecided)
				{
					delete_edge(node->edges[j]);
					progress = true;
				}
		}
	}
	
	for (int i = 0; i < graph->nedges; i++)
	{
		edge_t* edge = graph->edges[i];
		if (edge->state != undecided)
			continue;
		
		if (!(is_last_node &&
			((edge->node1 == last_node && edge->node2 == first_node) ||
			 (edge->node2 == last_node && edge->node1 == first_node))) &&
			connected_required(graph, edge->node1, edge->node2))
		{
			delete_edge(edge);
			progress = true;
		}
	}
	
	return progress;
}

//Convert undecided to required & deleted edges using the classification rules
void classify_graph(graph_t* graph, bool is_last_node, node_t* last_node,
					node_t* first_node)
{
	bool progress = true;
	while (progress)
	{
		progress = add_required_edges(graph);
		progress = progress || add_deleted_edges(graph, is_last_node,
												 last_node, first_node);
	}
}

node_t* find_common_parent(node_t* node1, node_t* node2)
{
	while (node1->dfs_depth > node2->dfs_depth)
		node1 = node1->dfs_parent;
	while (node2->dfs_depth > node1->dfs_depth)
		node2 = node2->dfs_parent;
	
	while (node1 != node2)
	{
		node1 = node1->dfs_parent;
		node2 = node2->dfs_parent;
	}
	
	return node1;
}

bool check_required_cycle_dfs(graph_t* graph, node_t* node, node_t* root,
							  unsigned depth, edge_t* in_edge)
{
	node->visited = true;
	
	for (int i = 0; i < node->degree; i++)
	{
		edge_t* edge = node->edges[i];
		if (edge == in_edge || edge->state != required)
			continue;
		
		node_t* neighbor = other_node(edge, node);
		if (neighbor->visited)
		{
			//Check to see if the cycle is a Hamiltonian cycle
			if (node->dfs_depth + neighbor->dfs_depth < graph->n - 1)
				return true;
			node_t* parent = find_common_parent(node, neighbor);
			if (parent->dfs_depth != 0)
				return true;
		}
		else
		{
			neighbor->dfs_depth = node->dfs_depth + 1;
			neighbor->dfs_parent = node;
			if (check_required_cycle_dfs(graph, neighbor, root, depth + 1,
										 edge))
				return true;
		}
	}
	
	return false;
}


//Implements rule F6: "Fail if any set of required arcs forms a closed circuit,
//other than a Hamilton circuit"
bool check_required_cycle(graph_t* graph)
{
	for (int i = 0; i < graph->n; i++)
	{
		graph->nodes[i]->visited = false;
		graph->nodes[i]->dfs_depth = 0;
		graph->nodes[i]->dfs_parent = NULL;
	}
	
	for (int i = 0; i < graph->n; i++)
	{
		node_t* node = graph->nodes[i];
		if (node->visited)
			continue;
		
		if (check_required_cycle_dfs(graph, node, node, 0, NULL))
			return true;
	}
	
	return false;
}

//Implements rule F7/F8: makes sure the graph minus the partial path is connected
bool check_connectivity(graph_t* graph, node_t** partial_path,
						unsigned partial_path_size)
{
	for (int i = 0; i < graph->n; i++)
		graph->nodes[i]->visited = false;
	
	for (int i = 0; i < partial_path_size; i++)
		partial_path[i]->visited = true;
	
	node_queue_t queue;
	node_queue_init(&queue, graph->n);
	node_queue_push(&queue, partial_path[0]);
	
	while (!node_queue_empty(&queue))
	{
		node_t* node = node_queue_pull(&queue);
		for (int i = 0; i < node->degree; i++)
		{
			edge_t* edge = node->edges[i];
			if (edge->state == deleted)
				continue;
			
			node_t* neighbor = other_node(edge, node);
			if (!neighbor->visited)
			{
				neighbor->visited = true;
				node_queue_push(&queue, neighbor);
			}
		}
	}
	
	node_queue_delete(&queue);
	
	for (int i = 0; i < graph->n; i++)
		if (!graph->nodes[i]->visited)
			return false;
	
	return true;
}

//Implements all the failure rules
bool check_graph(graph_t* graph, node_t** partial_path,
				 unsigned partial_path_size)
{
	if (!check_connectivity(graph, partial_path, partial_path_size))
		return false;
	
	if (check_required_cycle(graph))
		return false;
	
	for (int i = 0; i < graph->n; i++)
	{
		node_t* node = graph->nodes[i];
		
		//F1: "Fail if any vertex becomes isolated, that is, it has no incident
		//arc"
		//F2: "Fail if any vertex has only one incident arc"
		if (node->degree - node->deleted_degree < 2)
			return false;
		
		//F3: "Fail if any vertex has three required arcs incident"
		if (node->required_degree >= 3)
			return false;
	}
	
	return true;
}

void save_edge_states(graph_t* graph, edge_state_e* states)
{
	for (int i = 0; i < graph->nedges; i++)
		states[i] = graph->edges[i]->state;
}

void restore_edge_states(graph_t* graph, edge_state_e* states)
{
	for (int i = 0; i < graph->nedges; i++)
		graph->edges[i]->state = states[i];
}

void save_node_states(graph_t* graph, node_state_t* states)
{
	for (int i = 0; i < graph->n; i++)
	{
		states[i].required_degree = graph->nodes[i]->required_degree;
		states[i].deleted_degree = graph->nodes[i]->deleted_degree;
	}
}

void restore_node_states(graph_t* graph, node_state_t* states)
{
	for (int i = 0; i < graph->n; i++)
	{
		graph->nodes[i]->required_degree = states[i].required_degree;
		graph->nodes[i]->deleted_degree = states[i].deleted_degree;
	}
}

unsigned get_node_idx(graph_t* graph, node_t* node)
{
	for (int i = 0; i < graph->n; i++)
		if (graph->nodes[i] == node)
			return i;
	
	assert(0);
	return 0;
}

//partition handling

void parse_assignments(FILE* input, unsigned* ret, unsigned n)
{
	char buf[20];
	for (unsigned i = 0; i < n; i++)
	{
		fgets(buf, 20, input);
		ret[i] = atoi(buf);
	}
}

unsigned get_num_partitions(unsigned* assignment, unsigned n)
{
	unsigned ret = 0;
	for (unsigned i = 0; i < n; i++)
		if (assignment[i] > ret)
			ret = assignment[i];
	
	return ret + 1;
}

void parse_partitions(FILE* input, unsigned n, unsigned* assignment,
					 unsigned* num_partitions, unsigned** _partition_sizes)
{
	parse_assignments(input, assignment, n);
	*num_partitions = get_num_partitions(assignment, n);
	unsigned* partition_sizes = calloc(*num_partitions, sizeof(unsigned));
	for (unsigned i = 0; i < n; i++)
		partition_sizes[assignment[i]]++;
	
	*_partition_sizes = partition_sizes;
}

graph_t* get_subdomain_graph(graph_t* graph, unsigned* assignment,
							 unsigned num_partitions)
{
	bool* adj_matrix = calloc(num_partitions * num_partitions, sizeof(bool));
	
	for (unsigned i = 0; i < graph->nedges; i++)
	{
		edge_t* edge = graph->edges[i];
		unsigned node1_partition = assignment[get_node_idx(graph, edge->node1)];
		unsigned node2_partition = assignment[get_node_idx(graph, edge->node2)];
		if (node1_partition != node2_partition)
			adj_matrix[num_partitions*node1_partition + node2_partition] =
			adj_matrix[num_partitions*node2_partition + node1_partition] = true;
	}
	
	graph_t* ret = graph_from_adj_matrix(adj_matrix, num_partitions);
	free(adj_matrix);
	return ret;
}

bool hamiltonian_dfs(graph_t* graph, node_t* node, node_t** partial_path,
					 unsigned partial_path_size, unsigned* partition_order,
					 unsigned num_partitions, unsigned* partition_sizes,
					 unsigned* partition_assignment, unsigned cur_partition_order,
					 unsigned cur_partition_idx)
{
	edge_state_e* saved_edge_states = malloc(graph->nedges * sizeof(edge_state_e));
	node_state_t* saved_node_states = malloc(graph->n * sizeof(node_state_t));
	save_edge_states(graph, saved_edge_states);
	save_node_states(graph, saved_node_states);
	
	unsigned next_partition_idx, next_partition_order, next_partition;
	
	if (partition_assignment)
	{
		next_partition_idx = cur_partition_idx + 1;
		if (next_partition_idx == partition_sizes[partition_order[cur_partition_order]])
		{
			next_partition_idx = 0;
			next_partition_order = cur_partition_order + 1;
			if (next_partition_order == num_partitions)
				next_partition_order = 0;
		}
		else
			next_partition_order = cur_partition_order;
		
		next_partition = partition_order[cur_partition_order];
	}
	
	for (int i = 0; i < node->degree; i++)
	{
		edge_t* edge = node->edges[i];
		if (edge->state == deleted)
			continue;
		
		node_t* neighbor = other_node(edge, node);
		if (partial_path_size > 1 &&
			neighbor == partial_path[partial_path_size - 2])
			continue;
		
		if (partition_assignment &&
			partition_assignment[get_node_idx(graph, neighbor)] != next_partition)
			continue;
		
		require_edge(edge);
		classify_graph(graph, partial_path_size == graph->n, node,
					   partial_path[0]);
		
		if (check_graph(graph, partial_path, partial_path_size))
		{
			if (partial_path_size == graph->n)
			{
				free(saved_edge_states);
				free(saved_node_states);
				return true;
			}
			else
			{
				partial_path[partial_path_size] = neighbor;
				if (hamiltonian_dfs(graph, neighbor, partial_path,
									partial_path_size + 1, partition_order,
									num_partitions, partition_sizes,
									partition_assignment, next_partition_order,
									next_partition_idx))
				{
					free(saved_edge_states);
					free(saved_node_states);
					return true;
				}
			}
		}
		
		restore_edge_states(graph, saved_edge_states);
		restore_node_states(graph, saved_node_states);
	}
	
	free(saved_edge_states);
	free(saved_node_states);
	return false;
}

unsigned* get_partition_order(graph_t* graph, unsigned* assignment,
							  unsigned num_partitions)
{
	graph_t* subdomain_graph = get_subdomain_graph(graph, assignment,
												   num_partitions);
	
	node_t** partial_path = malloc(num_partitions * sizeof(node_t*));
	partial_path[0] = subdomain_graph->nodes[0];
	
	if (!hamiltonian_dfs(subdomain_graph, subdomain_graph->nodes[0],
						 partial_path, 1, NULL, 0, NULL, NULL, 0, 0))
	{
		fprintf(stderr, "Error: could not find hamilton cycle for subdomain graph\n");
		exit(1);
	}
	
	unsigned* ret = malloc(num_partitions * sizeof(unsigned));
	for (unsigned i = 0; i < num_partitions; i++)
		ret[i] = get_node_idx(subdomain_graph, partial_path[i]);
	
	return ret;
}

node_t** hamiltonian(graph_t* graph, FILE* partition_input)
{
	unsigned* partition_order = NULL;
	unsigned num_partitions = 0;
	unsigned* partition_sizes = NULL;
	unsigned* partition_assignment = NULL;
	
	if (partition_input)
	{
		partition_assignment = malloc(graph->n * sizeof(unsigned));
		parse_partitions(partition_input, graph->n, partition_assignment,
						 &num_partitions, &partition_sizes);
		partition_order = get_partition_order(graph, partition_assignment,
											  num_partitions);
	}
	
	node_t** partial_path = malloc(graph->n * sizeof(node_t*));
	partial_path[0] = graph->nodes[0];
	
	hamiltonian_dfs(graph, graph->nodes[0], partial_path, 1, partition_order,
					num_partitions, partition_sizes, partition_assignment, 0, 0);
	
	return partial_path;
}

void print_cycle(graph_t* graph, node_t** path)
{
	for (int i = 0; i < graph->n; i++)
		printf("%u ", get_node_idx(graph, path[i]));
	printf("\n");
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "Error: must specify input file\n");
		return 1;
	}
	
	char* input = readfile(argv[1]);
	if (!input)
	{
		fprintf(stderr, "Error: could not read file %s\n", argv[1]);
		return 1;
	}
	
	FILE* partition_input = NULL;
	if (argc > 2)
	{
		partition_input = fopen(argv[2], "r");
		if (!partition_input)
		{
			fprintf(stderr, "Error: could not read partition file: %s\n", argv[2]);
			return 1;
		}
	}
	
	graph_t* graph = parse_graph(input);
	node_t** cycle = hamiltonian(graph, partition_input);
	print_cycle(graph, cycle);
	
	return 0;
}

