#ifdef BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
#include <boost/test/framework.hpp>
using namespace boost::unit_test;
test_suite* init_unit_test_suite(int, char* []);
bool init_function() {
framework::master_test_suite().add(init_unit_test_suite(0,0));
return true;
}
int main( int argc, char* argv[] ) {
return ::boost::unit_test::unit_test_main( &init_function, argc, argv );
}
#endif
