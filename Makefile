LIBDIR=/usr/lib

install:
	gcc -fPIC -std=c++11 -shared -I/usr/include/mysql -I. lib_mysqludf_sys.cc -o /usr/lib/mysql/plugin/lib_mysqludf_sys.so 
