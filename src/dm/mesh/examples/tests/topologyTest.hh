#include <Sieve.hh>
#include <CoSieve.hh>
#include "sieveTest.hh"

namespace ALE {
  namespace Test {
    typedef ALE::Sieve<ALE::Point,int,int>     sieve_type;
    typedef ALE::New::Topology<int,sieve_type> topology_type;
  };
};
