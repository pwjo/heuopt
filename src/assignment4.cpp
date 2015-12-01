#include <iostream>
#include <memory>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <chrono>

#include <boost/shared_ptr.hpp>

#include <assignment_helpers.h>

#include <kpmp_instance.h>
#include <kpmp_solution.h>
#include <spine.h>

#define DEBUG 1

#define PROBABILISTIC_DECISION 1


// making this global for performance reasons
std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());

struct pathsegment
{
   pathsegment( int _cost )
    : cost(_cost), 
      weight(0.0)
    {
    };

    double cost; 
    double weight;

};
struct pathsegment_edge : pathsegment
{
    pathsegment_edge( int _page, edge_t _edge, int _cost )
    : pathsegment(_cost),
      page(_page),
      edge(_edge)
    {
    };

    int page;
    edge_t edge; 
};


struct pathsegment_vertex : pathsegment
{
    pathsegment_vertex( vertex_t _vertex, int _cost )
    : pathsegment(_cost),
      vertex(_vertex)
    {
    };

    vertex_t vertex;
};


class pheromone_matrix_edge
{
public:    
    pheromone_matrix_edge(double _initial_val)
    : initial_val( _initial_val )
    {
    }

    double get_pheromone(int page, const edge_t & e)
    {
        return initial_val;
    }
 

private:

    double initial_val;
    std::map<int, std::map<vertex_t, std::map<vertex_t, double> > > pheromone_tree;
};


class pheromone_matrix_vertex
{

public:    
    pheromone_matrix_vertex(double _initial_val)
    : initial_val( _initial_val )
    {
    }

    double get_pheromone(const vertex_t & from, vertex_t & to)
    {
        return initial_val;
    }
 

private:

    double initial_val;
    std::map<vertex_t, std::map<vertex_t, double> > pheromone_tree;
};





int build_local_pathsegment_edge_costs( const solution & sol,
                                         const std::list<edge_t> & edges, 
                             std::vector<pathsegment_edge> & pathsegment_edges )
    
{
    assert( pathsegment_edges.size() == 0 );

    int max_cost= 0;
    int cost_sum= 0;

    for( auto e= edges.begin(); e!=edges.end(); ++e )
    {
        for( int p= 0; p<sol.get_pages(); ++p )
        {
            int c= sol.try_num_crossing( p, *e );

            if( c > max_cost )
                max_cost= c;

            pathsegment_edges.push_back( pathsegment_edge(p, *e, c) );
            cost_sum+=c;
        }
    }
                

#ifdef DEBUG
    std::cout << "build_local_pathsegment_edges: max_cost= " << max_cost << std::endl;
#endif // DEBUG    


    return cost_sum;
}



int increment_local_pathsegment_edge_costs( const solution & sol,
                                 std::vector<pathsegment_edge> & pathsegment_edges )
{
    int max_cost= 0;
    int cost_sum= 0;

    for( auto ec= pathsegment_edges.begin(); ec!=pathsegment_edges.end(); ++ec )
    {
        int c= sol.try_num_crossing( ec->page, ec->edge ) + ec->cost;

        if( c > max_cost )
                max_cost= c;

        ec->cost= c;
        cost_sum+= c;
    }

#ifdef DEBUG
    std::cout << "increment_local_pathsegment_edges: max_cost= " << max_cost << std::endl;
#endif // DEBUG    

    return cost_sum;
}
 


int build_local_pathsegment_vertexs( const std::vector<std::vector<vertex_t> > & adjacency_list,
                               const std::map<vertex_t,bool> & vertex_hull,
                               std::vector<pathsegment_vertex> & pathsegment_vertexs )
{
    int max_cost= 0;
    int cost_sum= 0;

    for( auto e= vertex_hull.begin(); e!=vertex_hull.end(); ++e )
    {
        vertex_t v= e->first; 
        int c= (adjacency_list[v]).size();

        if( c > max_cost )
            max_cost= c;

        cost_sum+=c;
        pathsegment_vertexs.push_back( pathsegment_vertex(v, c) );
    }
                

#ifdef DEBUG
    std::cout << "build_local_pathsegment_vertexs: max_cost= " << max_cost << std::endl;
#endif // DEBUG    

    return cost_sum;
} 

std::vector<pathsegment_edge> filter_edge( const std::vector<pathsegment_edge> & pathsegment_edges, const edge_t & rm_edge )
{
    std::vector<pathsegment_edge> filtered;

    for( auto ec= pathsegment_edges.begin(); ec!=pathsegment_edges.end(); ++ec )
        if( ec->edge != rm_edge )
            filtered.push_back( *ec );
    return filtered;
}

#ifndef PROBABILISTIC_DECISION

    template<typename T>
    bool weight_order_func( const T & e1, const T & e2 )

    {
        return e1.weight > e2.weight;
    }



    template<typename T>
    typename std::vector<T>::const_iterator weight_choose_random_best( const std::vector<T> & list )
    {
        auto start= list.begin();
        auto best_val= start->weight;
        auto i= start+1;

        for( ; i!=list.end(); ++i )
        {
            if( i->weight!= best_val )
                break;
        }
        auto num= i-start;

        return start + rand() % num;
    }

#endif




template<typename T>
void calc_weight_relative_cost( std::vector<T> & pathsegment, int base_value )
{
    for( auto ec= pathsegment.begin(); ec!=pathsegment.end(); ++ec )
    {
        if( base_value == 0 )
            ec->weight= 1.0;
        else 
            ec->weight= (base_value-ec->cost) / base_value ;
    }
}


template<typename T>
const T & random_discrete_distribution_weight( const std::vector<T> & weight_container )
{
    std::list<double> wlist;
    for( auto i = weight_container.begin(); i!=weight_container.end(); ++i )
        wlist.push_back(i->weight);

    std::discrete_distribution<int> distribution(wlist.begin(), wlist.end());

    int choice= distribution(generator);

#ifdef DEBUG
    std::cout << "rand: distr: " <<  distribution << std::endl;
    std::cout << "rand: " << choice  << std::endl;
#endif // DEBUG   


    return weight_container[choice];
}

/**
 * the ant decides which edge to take next,
 * ant takes the complete hull into account (multiple edges, each edge for all pages)
 *
 * we are using an iterative crossing calculation, so the overhead is minimal
 */
void ant_edge_decission_hull_scope( solution & ant_solution, const std::list<edge_t> & edge_hull )
{
    std::vector<pathsegment_edge> pathsegment_edges;

    // build local info for edge decission
    int cost_sum= build_local_pathsegment_edge_costs( ant_solution,
                            edge_hull, pathsegment_edges );


    while( pathsegment_edges.size() )
    {

        calc_weight_relative_cost( pathsegment_edges, cost_sum );

#ifdef PROBABILISTIC_DECISION
        // choose probability based
        pathsegment_edge best_edge= random_discrete_distribution_weight(pathsegment_edges); 
#else
        // choose local best 
        sort( pathsegment_edges.begin(), pathsegment_edges.end(), weight_order_func<pathsegment_edge> );
        pathsegment_edge best_edge= *weight_choose_random_best(pathsegment_edges); 
#endif



        // TODO: remove if everyting is tested
        //assert( ant_solution.try_num_crossing( best_edge.page, best_edge.edge ) == best_edge.cost );
        ant_solution.add_edge( best_edge.page, best_edge.edge, best_edge.cost );  // TODO: dangerous ;)


        // rebuild local info for next edge decission
        // we now add this choosen edge to the prior calculated costs to save time
        {
            // temporar empty solution for incremental edge crossing calculation
            solution increment_solution ( ant_solution.get_pages(), ant_solution.get_num_vertices(), ant_solution.get_spine_order() );

            increment_solution.add_edge( best_edge.page, best_edge.edge ); 


            pathsegment_edges= filter_edge( pathsegment_edges, best_edge.edge);

            cost_sum= increment_local_pathsegment_edge_costs( increment_solution, pathsegment_edges );
        }

    }

}



/** collect edges and add none-added adjacent nodes to the hull 
 */
template<typename T1, typename T2>
void collect_hull( const solution * sol,
                   const std::vector<std::vector<vertex_t> > & adjacency_list,
                   const vertex_t & vertex,
                   T1 & vertex_hull, T2 & edge_hull )
{
    assert( vertex < adjacency_list.size() );

    auto neighbours= adjacency_list[vertex];

    for( auto neighbour= neighbours.begin(); neighbour!=neighbours.end(); ++neighbour )
    {
        if( sol->vertex_in_spine(*neighbour) ) 
            edge_hull.push_back( edge_t(vertex,*neighbour) );
        else
            vertex_hull[*neighbour]= 1;
    }
}



solution * ant_walk( int num_pages, /*const std::vector<vertex_t> & vertices,*/
               const std::vector<std::vector<vertex_t> > & adjacency_list )
{
    //std::vector<int> free_vertices( adjacency_list.size(), 0);

    // everything thats reachable from current added vertices
    std::map<vertex_t,bool> vertex_hull; 

    solution * ant_solution= new solution(num_pages, adjacency_list.size());
    assert(ant_solution);


    // generate random start vertex
    std::uniform_int_distribution<int> random_vertex(0, adjacency_list.size()-1);
    vertex_t next_vertex= random_vertex(generator);

    do
    {
        vertex_hull.erase(next_vertex);
        ant_solution->add_vertices_to_spine_order( next_vertex );


        std::list<edge_t> edge_hull;

        collect_hull( ant_solution, adjacency_list, next_vertex, 
                      vertex_hull, edge_hull );


        ant_edge_decission_hull_scope( *ant_solution, edge_hull );

 

        if( vertex_hull.size() )
        {
            // decide node together with global info
            std::vector<pathsegment_vertex> vertex_pathsegment_alternatives;
            int cost_sum= build_local_pathsegment_vertexs( adjacency_list, vertex_hull, vertex_pathsegment_alternatives);
            calc_weight_relative_cost( vertex_pathsegment_alternatives, cost_sum );

#ifdef PROBABILISTIC_DECISION

            // probabilisitic choice
            next_vertex= random_discrete_distribution_weight( vertex_pathsegment_alternatives ).vertex;
#else
            // calculate best
            // if there are multiple bests, choos a random
            sort( vertex_pathsegment_alternatives.begin(), vertex_pathsegment_alternatives.end(), weight_order_func<pathsegment_vertex> );

            next_vertex= weight_choose_random_best( vertex_pathsegment_alternatives )->vertex;
#endif
        }

        // we could have islands or not connected vertices
        // we just add them
        else
        {
            if( ant_solution->get_spine_order().size() < adjacency_list.size() )
            {
                for( auto i= adjacency_list.begin(); i!=adjacency_list.end(); ++i )
                {
                    vertex_t v= i - adjacency_list.begin();
                    if( ! ant_solution->vertex_in_spine(v) )
                    { 
                        next_vertex= v;
                        break;
                    }
                }
 
            }
        }

#ifdef DEBUG        
        std::cout << "vertex_hull.size(): " << vertex_hull.size() << std::endl;
        std::cout << "next_vertex= " << next_vertex << std::endl;
#endif // DEBUG

    } while( ant_solution->get_spine_order().size() != adjacency_list.size());

    return ant_solution;
}





int main( int argc, char **argv)
{

    // Declare the supported options.
    boost::program_options::options_description desc("Allowed options");
    boost::program_options::variables_map vm;

    std::string input_filename;
    std::string output_filename;


    int num_ants= 3;
    int num_runs= 5;
   

    desc.add_options()
         ("num-ants", boost::program_options::value<int>(&num_ants), "number of ants in one run")
         ("num-runs", boost::program_options::value<int>(&num_runs), "number of ant walks and pheromon steps")

             ;

    if( common_cmdline( argc, argv,
                    desc, vm, 
                    input_filename, output_filename ) )
        return 1;


        

    //
    // instance input
    //
    std::auto_ptr<KPMPInstance> instance( KPMPInstance::readInstance( input_filename ) );

	ofstream outfile(output_filename, ios::out);

    if( !outfile )
    {
        std::cerr << "error opening output file for writing" << std::endl;
        return 1;
    }

    
    /*
    auto adjacency_list= instance->getAdjacencyList();
    for( auto i= adjacency_list.begin(); i!=adjacency_list.end(); ++i )
    {
        std::cout <<  i - adjacency_list.begin() << ": ";
        for( auto j= i->begin(); j!=i->end(); ++j )
            std::cout << *j << " ";

        std::cout << std::endl;
    }
    return 1;
    */

    boost::shared_ptr<solution> best_solution;
    
    for( int run=0; run<num_runs; ++run )
    {
        std::vector< boost::shared_ptr<solution> > ant_solutions(num_ants);

        for( int ant=0; ant<num_ants; ++ant )
        {
            ant_solutions[ant]= boost::shared_ptr<solution>( ant_walk(instance->getK(), instance->getAdjacencyList()) );

#ifdef DEBUG
                std::cout << "ant " << ant << ": " << * (ant_solutions[ant]) << ": " << (ant_solutions[ant])->get_crossings()<< std::endl;
#endif // DEBUG                

            if( !best_solution ||
                ant_solutions[ant]->get_crossings() < best_solution->get_crossings() )
            {
                best_solution= ant_solutions[ant];
            }

        }
    }


    //
    // output
    //
    write_solution( outfile, *best_solution );
	outfile.close();

    write_solution_statistics( std::cout, *best_solution );

    return 0;
}