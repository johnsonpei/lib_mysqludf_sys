LIBDIR=`mysql_config --plugindir`

install:
	gcc -fPIC -std=c++11 -shared `mysql_config --include` -I. lib_mysqludf_sys.cc -o ${LIBDIR}/lib_mysqludf_sys.so 
